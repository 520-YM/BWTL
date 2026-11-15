typedef struct {
    int count;
    ProVector point[64];
    double radius[64];
} ReplayPatternSketch;

static ProError replay_pattern_draw(ProSection section,const ProMatrix frame,double scale,void *context){
    ReplayPatternSketch *sketch=context;
    (void)scale;
    for(int i=0;i<sketch->count;i++){
        ProVector delta;
        ad_sub(sketch->point[i],frame[3],delta);
        double x=ad_dot(delta,frame[0]),y=ad_dot(delta,frame[1]);
        int id;
        ProError error;
        if(sketch->radius[i]>0){
            error=model_circle(section,x,y,sketch->radius[i],&id);
        }else{
            Pro2dPointdef point={PRO_2D_POINT,{x,y}};
            error=ProSectionEntityAdd(section,(Pro2dEntdef *)&point,&id);
        }
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError replay_pattern_point_definition(ProFeature *locations,ProElement *definition){
    FdTxn tx={0};
    FdRef ref={0};
    ref.item=*locations;
    ProElement points,section;
    ProError error=ProSelectionAlloc(NULL,&ref.item,&ref.selection);
    if(!error)error=ProElementAlloc(PRO_E_PATTERN_ROOT,definition);
    if(!error)error=model_element(*definition,PRO_E_GENPAT_TYPE,1,PRO_GENPAT_POINT_DRIVEN,NULL);
    if(!error)error=model_element(*definition,PRO_E_GENPAT_REGEN_METHOD,1,PRO_PAT_GENERAL,NULL);
    if(!error)error=model_element(*definition,PRO_E_GENPAT_POINT,0,0,&points);
    if(!error)error=model_element(points,PRO_E_GENPAT_POINT_REF_TYPE,1,PRO_GENPAT_REF_SKETCH,NULL);
    if(!error)error=model_element(*definition,PRO_E_GENPAT_OFFSET_FROM_SKET,1,0,NULL);
    if(!error)error=model_element(*definition,PRO_E_STD_SECTION,0,0,&section);
    if(!error)error=model_element(section,PRO_E_STD_SEC_METHOD,1,0,NULL);
    if(!error)error=fd_ref_element(&tx,section,PRO_E_SEC_USE_SKETCH,&ref);
    if(!error)error=model_element(*definition,PRO_E_GENPAT_CRV_OR_TYPE,1,1,NULL);
    fd_ref_free(&ref);
    return error;
}

static ProError replay_ref_pattern_definition(ProElement source,ProElement definition){
    ProElement setting,reference;
    int type;
    ProError error=mc_tree_element(source,(ProElemId[]){PRO_E_GENPAT_REF,PRO_E_GENPAT_REF_TYPE},2,&setting);
    if(!error)error=ProElementIntegerGet(setting,NULL,&type);
    if(!error&&type!=0&&type!=PRO_PAT_FEATURE)error=PRO_TK_UNSUPPORTED;
    if(!error)error=model_element(definition,PRO_E_GENPAT_REF,0,0,&reference);
    if(!error)error=model_element(reference,PRO_E_GENPAT_REF_TYPE,1,type?type:PRO_PAT_FEATURE,NULL);
    return error;
}



#define REPLAY_REFERENCE_PATTERN_MAX 32
#define REPLAY_GUIDE_CURVE_MAX 8

typedef struct {
    ProModelitem source;
    ProModelitem local;
    ProVector center;
    ProVector midpoint;
    double radius;
    int external;
} ReplayGuideCurve;

typedef struct {
    ProFeature consumer,provider;
    ReplayGuideCurve curves[REPLAY_GUIDE_CURVE_MAX];
    ReplayExternalReference surfaces[4];
    int count,surface_count;
} ReplayGuideMember;

struct ReplayReferencePattern {
    ReplayGuideMember members[REPLAY_REFERENCE_PATTERN_MAX];
    int count,seed,assembly_ref,provider_ref;
    ProVector normal;
};

static ProError replay_circle_geometry(ProModelitem *item,ProVector center,double *radius){
    ProGeomitemdata *raw=NULL;
    ProError error=PRO_TK_INVALID_TYPE;
    if(item->type==PRO_EDGE){
        ProEdge edge;
        error=ProGeomitemToEdge(item,&edge);
        if(!error)error=ProEdgeDataGet(edge,&raw);
    }else if(item->type==PRO_CURVE){
        ProCurve curve;
        error=ProGeomitemToCurve(item,&curve);
        if(!error)error=ProCurveDataGet(curve,&raw);
    }
    const ProCurvedata *data=raw?raw->data.p_curve_data:NULL;
    if(!error){
        if(data&&data->arc.type==PRO_ENT_ARC){
            memcpy(center,data->arc.origin,sizeof(ProVector));
            *radius=data->arc.radius;
        }else if(data&&data->circle.type==PRO_ENT_CIRCLE){
            memcpy(center,data->circle.center,sizeof(ProVector));
            *radius=data->circle.radius;
        }else error=PRO_TK_INVALID_TYPE;
    }
    if(raw)ProGeomitemdataFree(&raw);
    return error;
}

static ProError replay_guide_curve(ReplayExternalReference *ref,ReplayGuideCurve *curve){
    ProMatrix provider,consumer;
    ProVector point,intermediate;
    ProEdge edge;
    ProError error=replay_circle_geometry(&ref->item,point,&curve->radius);
    if(!error)error=ProAsmcomppathTrfGet(&ref->provider,PRO_B_TRUE,provider);
    if(!error)error=ProAsmcomppathTrfGet(&ref->consumer,PRO_B_FALSE,consumer);
    if(!error)error=ProPntTrfEval(point,provider,intermediate);
    if(!error)error=ProPntTrfEval(intermediate,consumer,curve->center);
    if(!error)error=ProGeomitemToEdge(&ref->item,&edge);
    if(!error)error=ProEdgeXyzdataEval(edge,0.5,point,NULL,NULL,NULL);
    if(!error)error=ProPntTrfEval(point,provider,intermediate);
    if(!error)error=ProPntTrfEval(intermediate,consumer,curve->midpoint);
    if(!error){
        curve->source=ref->item;
        curve->external=ref->index;
    }
    return error;
}

static ProError replay_pattern_read_guide(ImitationReplay *replay,ProFeature *seed,ReplayReferencePattern *plan){
    ProPattern downstream,upstream;
    ProFeature header,upstream_header,*members=NULL,*guides=NULL;
    int member_count=0,guide_count=0,group_count=0;
    ProExtRefInfo *groups=NULL;
    ProError error=ProFeaturePatternGet(seed,PRO_FEAT_PATTERN,&downstream);
    if(!error)error=ProPatternHeaderGet(&downstream,&header);
    if(!error)error=ProPatternMembersGet(&downstream,&members);
    if(!error)error=ProArraySizeGet((ProArray)members,&member_count);
    if(!error&&(member_count<2||member_count>REPLAY_REFERENCE_PATTERN_MAX))error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProSolidExternParentsGet((ProSolid)replay->source,PRO_ALL_EXT_REF_TYPES,&groups,&group_count);
    plan->assembly_ref=plan->provider_ref=plan->seed=-1;
    int index=0;
    for(int g=0;!error&&g<group_count;g++)for(int i=0;!error&&i<groups[g].n_refs;i++,index++){
        ProExtFeatRef ref=groups[g].ext_refs[i];
        ProFeature user;
        error=ProExtRefOwnFeatGet(ref,&user);
        if(error||user.id!=header.id)continue;
        ProModelitem item;
        error=ProExtRefModelitemGet(ref,&item);
        if(error)break;
        if((item.type==PRO_PART||item.type==PRO_ASSEMBLY)&&plan->assembly_ref<0)plan->assembly_ref=index;
        else if(item.type==PRO_FEATURE&&item.owner&&plan->provider_ref<0){
            error=ProFeatureInit((ProSolid)item.owner,item.id,&upstream_header);
            if(!error)error=ProFeaturePatternGet(&upstream_header,PRO_FEAT_PATTERN,&upstream);
            if(!error)plan->provider_ref=index;
        }else error=PRO_TK_UNSUPPORTED;
    }
    if(!error&&plan->provider_ref<0)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProPatternMembersGet(&upstream,&guides);
    if(!error)error=ProArraySizeGet((ProArray)guides,&guide_count);
    if(!error&&guide_count!=member_count)error=PRO_TK_UNSUPPORTED;
    for(int j=0;!error&&j<member_count;j++){
        plan->members[j].consumer=members[j];
        if(members[j].id==seed->id)plan->seed=j;
    }
    if(!error&&plan->seed<0)error=PRO_TK_INCOMPLETE;
    plan->count=member_count;
    index=0;
    for(int g=0;!error&&g<group_count;g++)for(int i=0;!error&&i<groups[g].n_refs;i++,index++){
        ProExtFeatRef ref=groups[g].ext_refs[i];
        ProFeature user;
        error=ProExtRefOwnFeatGet(ref,&user);
        if(error)break;
        int j=0;
        while(j<member_count&&plan->members[j].consumer.id!=user.id)j++;
        if(j==member_count)continue;
        ReplayGuideMember *member=&plan->members[j];
        ReplayExternalReference ext={0};
        ext.index=index;
        error=ProExtRefModelitemGet(ref,&ext.item);
        if(!error)error=ProExtRefAsmcompsGet(ref,&ext.consumer,&ext.provider);
        if(error)break;
        if(ext.item.type==PRO_SURFACE){
            if(member->surface_count==_countof(member->surfaces))error=PRO_TK_UNSUPPORTED;
            else member->surfaces[member->surface_count++]=ext;
        }else if(ext.item.type==PRO_EDGE&&member->count<REPLAY_GUIDE_CURVE_MAX){
            ProFeature owner;
            error=ProGeomitemFeatureGet(&ext.item,&owner);
            int k=0;
            while(!error&&k<guide_count&&(guides[k].id!=owner.id||guides[k].owner!=owner.owner))k++;
            if(!error&&(k==guide_count||(member->count&&member->provider.id!=owner.id)))error=PRO_TK_UNSUPPORTED;
            if(!error){
                member->provider=owner;
                error=replay_guide_curve(&ext,&member->curves[member->count]);
                if(!error)member->count++;
            }
        }else error=PRO_TK_UNSUPPORTED;
    }
    if(!error){
        ReplayGuideMember *first=&plan->members[plan->seed];
        if(first->count<=0)error=PRO_TK_UNSUPPORTED;
        for(int i=0;!error&&i<plan->count;i++){
            ReplayGuideMember *member=&plan->members[i];
            if(member->count!=first->count||member->surface_count!=first->surface_count)error=PRO_TK_UNSUPPORTED;
            for(int j=0;!error&&j<i;j++)
                if(member->provider.id==plan->members[j].provider.id)error=PRO_TK_UNSUPPORTED;
        }
    }
    if(groups)ProExtRefInfoFree(&groups);
    if(members)ProArrayFree((ProArray *)&members);
    if(guides)ProArrayFree((ProArray *)&guides);
    return error;
}



static int replay_guide_position(const ReplayReferencePattern *plan,const double *a,const double *b){
    ProVector delta;
    ad_sub(a,b,delta);
    const double offset=ad_dot(delta,plan->normal);
    for(int i=0;i<3;i++)delta[i]-=offset*plan->normal[i];
    return ad_dot(delta,delta)<0.0002*0.0002;
}

typedef struct {
    ReplayReferencePattern *plan;
    ReplayGuideMember *member;
} ReplayGuideSearch;

static ProError replay_guide_locate(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    ReplayGuideSearch *search=data;
    ProCurve curve;
    ProEnttype type;
    ProError error=ProGeomitemToCurve(item,&curve);
    if(!error)error=ProCurveTypeGet(curve,&type);
    if(error||type!=PRO_ENT_ARC)return error;
    ProVector center,midpoint;
    double radius;
    error=replay_circle_geometry(item,center,&radius);
    if(!error)error=ProCurveXyzdataEval(curve,0.5,midpoint,NULL,NULL);
    for(int i=0;!error&&i<search->member->count;i++){
        ReplayGuideCurve *expected=&search->member->curves[i];
        if(fabs(radius-expected->radius)>0.0002||
           !replay_guide_position(search->plan,center,expected->center)||
           !replay_guide_position(search->plan,midpoint,expected->midpoint))continue;
        if(expected->local.id)error=PRO_TK_E_AMBIGUOUS;
        else expected->local=*item;
    }
    return error;
}

static ProError replay_pattern_build_guide(ImitationReplay *replay,ProSection original){
    ReplayReferencePattern *plan=replay->reference_pattern;
    ReplayGuideMember *seed=&plan->members[plan->seed];
    ReplayPatternSketch points={0},arcs={0};
    ProMatrix frame;
    ProError error=ProSectionLocationGet(original,frame);
    if(error)return error;
    memcpy(plan->normal,frame[2],sizeof(ProVector));
    if(!ad_unit(plan->normal))return PRO_TK_INCOMPLETE;
    int axis=0;
    for(int i=1;i<3;i++)if(fabs(plan->normal[i])>fabs(plan->normal[axis]))axis=i;
    if(fabs(plan->normal[axis])<0.9999999)return PRO_TK_UNSUPPORTED;
    const wchar_t *plane=imitation_template_plane(replay->target,axis);
    const wchar_t *orient=imitation_template_plane(replay->target,(axis+1)%3);
    if(!plane||!orient)return PRO_TK_E_NOT_FOUND;

    points.count=plan->count;
    arcs.count=seed->count;
    for(int i=0;i<plan->count;i++)for(int j=0;j<seed->count;j++)
        for(int k=0;k<3;k++)points.point[i][k]+=plan->members[i].curves[j].center[k]/seed->count;
    for(int j=0;j<seed->count;j++){
        memcpy(arcs.point[j],seed->curves[j].center,sizeof(ProVector));
        arcs.radius[j]=seed->curves[j].radius;
    }

    ProName locations_name,guide_name;
    swprintf_s(locations_name,_countof(locations_name),L"BWTL_REF_POINTS_%d",seed->consumer.id);
    swprintf_s(guide_name,_countof(guide_name),L"BWTL_REF_GUIDE_%d",seed->consumer.id);
    ProFeature locations,guide,*members=NULL;
    ProPattern pattern;
    ProElement tree=NULL;
    int count=0;
    error=smx_custom_sketch(replay->target,locations_name,plane,orient,replay_pattern_draw,&points,frame);
    if(!error)error=smx_custom_sketch(replay->target,guide_name,plane,orient,replay_pattern_draw,&arcs,frame);
    if(!error)error=ux_named_feature(replay->target,locations_name,&locations);
    if(!error)error=ux_named_feature(replay->target,guide_name,&guide);
    if(!error)error=replay_pattern_point_definition(&locations,&tree);
    if(!error)error=ProPatternCreate(&guide,PRO_FEAT_PATTERN,tree);
    if(!error)error=part_regenerate(replay->target);
    if(!error)error=ProFeaturePatternGet(&guide,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternMembersGet(&pattern,&members);
    if(!error)error=ProArraySizeGet((ProArray)members,&count);
    if(!error&&count!=plan->count)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<count;i++){
        ReplayGuideSearch search={plan,&plan->members[i]};
        error=ProFeatureGeomitemVisit(&members[i],PRO_CURVE,replay_guide_locate,NULL,&search);
        for(int j=0;!error&&j<search.member->count;j++)
            if(!search.member->curves[j].local.id)error=PRO_TK_INCOMPLETE;
    }
    if(!error&&members[plan->seed].id!=guide.id)error=PRO_TK_UNSUPPORTED;
    if(members)ProArrayFree((ProArray *)&members);
    if(tree)ProElementFree(&tree);
    return error;
}

static ProError replay_pattern_project(ImitationReplay *replay,const ReplayExternalReference *external,
                                      ProSection section,int *id){
    ReplayReferencePattern *plan=replay->reference_pattern;
    if(!plan)return PRO_TK_E_NOT_FOUND;
    ReplayGuideMember *seed=&plan->members[plan->seed];
    for(int i=0;i<seed->count;i++)if(seed->curves[i].external==external->index){
        ProSelection selection=NULL;
        ProError error=ProSelectionAlloc(NULL,&seed->curves[i].local,&selection);
        if(!error)error=ProSectionEntityFromProjection(section,selection,id);
        if(selection)ProSelectionFree(&selection);
        return error;
    }
    return PRO_TK_E_NOT_FOUND;
}

static ProError replay_pattern_acknowledge(ImitationReplay *replay){
    ReplayReferencePattern *plan=replay->reference_pattern;
    const ReplayGuideMember *seed=&plan->members[plan->seed];
    for(int i=0;i<seed->count;i++)if(!replay->external_used[seed->curves[i].external])return PRO_TK_INCOMPLETE;
    for(int i=0;i<plan->count;i++){
        const ReplayGuideMember *member=&plan->members[i];
        if(i==plan->seed)continue;
        for(int j=0;j<member->surface_count;j++){
            const ReplayExternalReference *s=&member->surfaces[j];
            int match=0;
            for(int k=0;k<seed->surface_count;k++){
                const ReplayExternalReference *t=&seed->surfaces[k];
                if(replay->external_used[t->index]&&s->item.owner==t->item.owner&&s->item.id==t->item.id&&
                   replay_same_path(&s->consumer,&t->consumer)&&replay_same_path(&s->provider,&t->provider))match++;
            }
            if(match!=1)return PRO_TK_INCOMPLETE;
            replay->external_used[s->index]=1;
        }
        for(int j=0;j<member->count;j++)replay->external_used[member->curves[j].external]=1;
    }
    if(plan->assembly_ref>=0)replay->external_used[plan->assembly_ref]=1;
    replay->external_used[plan->provider_ref]=1;
    return PRO_TK_NO_ERROR;
}

