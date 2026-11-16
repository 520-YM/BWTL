static ProError replay_add_mapping(ImitationReplay *replay,int source,int target){
    if(replay->count==REPLAY_FEATURE_MAX)return PRO_TK_OUT_OF_RANGE;
    replay->mapped[replay->count].source=source;
    replay->mapped[replay->count++].target=target;
    return PRO_TK_NO_ERROR;
}

typedef struct {ProMdl original,model;} ReplayPatternSource;

static ProError replay_pattern_source_close(ReplayPatternSource *source){
    if(!source->model)return PRO_TK_NO_ERROR;
    ProError error=ProMdlErase(source->model);
    if(!error)source->model=NULL;
    return error;
}

static ProPatternClass replay_pattern_class(ProFeattype type){
    return type==PRO_FEAT_GROUP_HEAD?PRO_GROUP_PATTERN:PRO_FEAT_PATTERN;
}

static ProError replay_pattern_definition_class(ProFeature *seed,ProPatternClass pattern_class,
                                               ReplayPatternSource *source,ProElement *tree,int *kind){
    static volatile LONG serial;
    ProError error=PRO_TK_NO_ERROR;
    if(!source->model){
        ProMdlName name;
        swprintf_s(name,_countof(name),L"bwtl_pat_%08lx_%08lx",GetCurrentProcessId(),
                   (unsigned long)InterlockedIncrement(&serial));
        source->original=seed->owner;
        /* Creo 10 can mark a source with an axis pattern modified during
           extraction. Extract on a temporary model kept only in memory. */
        error=ProMdlnameCopy(seed->owner,name,&source->model);
    }
    if(!error&&source->original!=seed->owner)error=PRO_TK_BAD_CONTEXT;
    ProFeature scratch;
    if(!error)error=ProFeatureInit((ProSolid)source->model,seed->id,&scratch);
    ProPatternStatus status;
    if(!error)error=ProFeaturePatternStatusGet(&scratch,&status);
    if(!error&&status!=PRO_PATTERN_LEADER)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProPatternElemtreeCreate(&scratch,pattern_class,tree);
    ProElement type;
    if(!error)error=mc_tree_element(*tree,(ProElemId[]){PRO_E_GENPAT_TYPE},1,&type);
    if(!error)error=ProElementIntegerGet(type,NULL,kind);
    return error;
}

static ProError replay_pattern_definition(ProFeature *seed,ReplayPatternSource *source,ProElement *tree,int *kind){
    return replay_pattern_definition_class(seed,PRO_FEAT_PATTERN,source,tree,kind);
}

static int replay_group_pattern_supported(int kind){
    return kind==PRO_GENPAT_DIR_DRIVEN||kind==PRO_GENPAT_AXIS_DRIVEN;
}


static int replay_pattern_supported(int kind){
    return kind==PRO_GENPAT_DIM_DRIVEN||kind==PRO_GENPAT_DIR_DRIVEN||kind==PRO_GENPAT_AXIS_DRIVEN||kind==PRO_GENPAT_REF_DRIVEN||
        kind==PRO_GENPAT_POINT_DRIVEN||kind==PRO_GENPAT_CRV_DRIVEN;
}

static ProError replay_pattern_field(ImitationReplay *replay,ProElement source,ProElement target,
                                      ProElemId id){
    ProElement field=NULL,local=NULL;
    ProError error=mc_tree_element(source,&id,1,&field);
    if(!error)error=replay_element(replay,field,&local);
    if(!error&&!local)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProElemtreeElementAdd(target,NULL,local);
    if(error&&local)ProElementFree(&local);
    return error;
}

static ProError replay_group_pattern_branch(ImitationReplay *replay,ProElement original,
                                            ProElement definition,int kind,int instances){
    const int circular=kind==PRO_GENPAT_AXIS_DRIVEN;
    const ProElemId branch_id=circular?PRO_E_GENPAT_AXIS:PRO_E_GENPAT_DIR;
    ProElement old_branch=NULL,new_branch=NULL,unused=NULL;
    int count=0;
    ProError error=mc_tree_element(original,&branch_id,1,&old_branch);
    if(!error)error=mc_tree_element(old_branch,(ProElemId[]){PRO_E_GENPAT_DIM_FIRST_DIR_NUM_INST},1,&unused);
    if(!error)error=ProElementIntegerGet(unused,NULL,&count);
    if(!error&&count!=instances)error=PRO_TK_UNSUPPORTED;
    if(!error&&!circular){
        ProReference ref=NULL;
        ProError second=mc_tree_element(old_branch,(ProElemId[]){PRO_E_GENPAT_DIR2,
            PRO_E_DIRECTION_COMPOUND,PRO_E_DIRECTION_REFERENCE},3,&unused);
        if(!second)second=ProElementReferenceGet(unused,NULL,&ref);
        if(ref){ProReferenceFree(ref);error=PRO_TK_UNSUPPORTED;}
        else if(second&&second!=PRO_TK_E_NOT_FOUND&&second!=PRO_TK_EMPTY)error=second;
    }
    if(!error)error=model_element(definition,branch_id,0,0,&new_branch);
    const ProElemId linear_fields[]={PRO_E_GENPAT_DIR1,PRO_E_GENPAT_DIR1_INC,PRO_E_DIR_PAT_DIR1_FLIP,
        PRO_E_DIR_PAT_DIR1_OPT,PRO_E_GENPAT_DIM_FIRST_DIR,PRO_E_GENPAT_DIM_FIRST_DIR_NUM_INST};
    const ProElemId axis_fields[]={PRO_E_GENPAT_AXIS_REF,PRO_E_GENPAT_AXIS1_INC,PRO_E_AXIS_PAT_DIR1_FLIP,
        PRO_E_GENPAT_DIM_FIRST_DIR,PRO_E_GENPAT_DIM_FIRST_DIR_NUM_INST};
    const ProElemId *fields=circular?axis_fields:linear_fields;
    const int fields_count=circular?_countof(axis_fields):_countof(linear_fields);
    for(int i=0;!error&&i<fields_count;i++)error=replay_pattern_field(replay,old_branch,new_branch,fields[i]);
    return error;
}

static ProError replay_pattern_sketch(ImitationReplay *replay,ProElement source,ProElement target,int kind){
    const ProElemId defaults[][2]={
        {PRO_E_GENPAT_PROJECT,PRO_E_GENPAT_PROJ_OPT},
        {PRO_E_GENPAT_ALT_ORIG,PRO_E_GENPAT_ALT_ORG_OPT}
    };
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<_countof(defaults);i++){
        ProElement field;
        int value;
        error=mc_tree_element(source,(ProElemId *)defaults[i],2,&field);
        if(!error)error=ProElementIntegerGet(field,NULL,&value);
        if(!error&&value)error=PRO_TK_UNSUPPORTED;
    }
    ProElement source_section,target_section;
    if(!error)error=mc_tree_element(source,(ProElemId[]){PRO_E_STD_SECTION},1,&source_section);
    if(!error)error=model_element(target,PRO_E_STD_SECTION,0,0,&target_section);
    if(!error)error=replay_pattern_field(replay,source_section,target_section,PRO_E_STD_SEC_METHOD);
    if(!error)error=replay_pattern_field(replay,source_section,target_section,PRO_E_SEC_USE_SKETCH);
    if(!error)error=replay_pattern_field(replay,source,target,PRO_E_GENPAT_CRV_OR_TYPE);
    if(!error&&kind==PRO_GENPAT_POINT_DRIVEN){
        ProElement field;
        int reference_type;
        error=mc_tree_element(source,(ProElemId[]){PRO_E_GENPAT_POINT,PRO_E_GENPAT_POINT_REF_TYPE},2,&field);
        if(!error)error=ProElementIntegerGet(field,NULL,&reference_type);
        if(!error&&reference_type!=PRO_GENPAT_REF_SKETCH)error=PRO_TK_UNSUPPORTED;
        if(!error)error=replay_pattern_field(replay,source,target,PRO_E_GENPAT_OFFSET_FROM_SKET);
    }
    return error;
}

static ProError replay_group_instance_members(ImitationReplay *replay,ProFeature *source,ProFeature *target){
    ProFeature *old_members=NULL,*new_members=NULL;
    int old_count=0,new_count=0;
    ProError error=ProGroupFeaturesCollect((ProGroup *)source,&old_members);
    if(!error)error=ProGroupFeaturesCollect((ProGroup *)target,&new_members);
    if(!error)error=ProArraySizeGet((ProArray)old_members,&old_count);
    if(!error)error=ProArraySizeGet((ProArray)new_members,&new_count);
    if(!error&&old_count!=new_count)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<old_count;i++){
        if(old_members[i].id==source->id&&new_members[i].id==target->id)continue;
        ProFeattype before,after;
        ProFeatStatus old_status,new_status;
        error=ProFeatureTypeGet(&old_members[i],&before);
        if(!error)error=ProFeatureTypeGet(&new_members[i],&after);
        if(!error)error=ProFeatureStatusGet(&old_members[i],&old_status);
        if(!error)error=ProFeatureStatusGet(&new_members[i],&new_status);
        if(!error&&(before!=after||before==PRO_FEAT_GROUP_HEAD||old_status!=new_status))error=PRO_TK_INCOMPLETE;
        if(!error)error=replay_add_mapping(replay,old_members[i].id,new_members[i].id);
    }
    if(old_members)ProArrayFree((ProArray *)&old_members);
    if(new_members)ProArrayFree((ProArray *)&new_members);
    return error;
}

typedef struct {
    ProVector axis,center;
    double radius;
    int found;
} ReplayPatternBore;

static int replay_same_bore(const ReplayPatternBore *a,const ReplayPatternBore *b){
    return a->found&&b->found&&ad_near(a->center,b->center,0.0002)&&
        fabs(ad_dot(a->axis,b->axis))>=1.0-1e-7&&fabs(a->radius-b->radius)<=0.0002;
}

static ProError replay_pattern_bore(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    ProSurface face;
    ProGeomitemdata *raw=NULL;
    ProError error=ProGeomitemToSurface(item,&face);
    if(!error)error=ProSurfaceDataGet(face,&raw);
    if(!error&&raw->data.p_surface_data->type==PRO_SRF_CYL){
        ReplayPatternBore *bore=data;
        const ProCylinderdata *cylinder=&raw->data.p_surface_data->srf_shape.cylinder;
        ReplayPatternBore current={.radius=fabs(cylinder->radius),.found=1};
        memcpy(current.axis,cylinder->e3,sizeof(current.axis));
        if(!ad_unit(current.axis)||current.radius<=0)error=PRO_TK_INCOMPLETE;
        if(!error){
            double along=ad_dot(cylinder->origin,current.axis);
            for(int i=0;i<3;i++)current.center[i]=cylinder->origin[i]-along*current.axis[i];
            if(!bore->found)*bore=current;
            else if(!replay_same_bore(bore,&current))error=PRO_TK_UNSUPPORTED;
        }
    }
    if(raw)ProGeomitemdataFree(&raw);
    return error;
}

static ProError replay_pattern_bores(ProFeature *members,int count,ReplayPatternBore *bores){
    for(int i=0;i<count;i++){
        ProFeattype type;
        ProFeatStatus status;
        ProError error=ProFeatureTypeGet(&members[i],&type);
        if(!error)error=ProFeatureStatusGet(&members[i],&status);
        if(!error&&(type!=PRO_FEAT_CUT_SMT||status!=PRO_FEAT_ACTIVE))error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProFeatureGeomitemVisit(&members[i],PRO_SURFACE,replay_pattern_bore,NULL,&bores[i]);
        if(error)return error;
        if(!bores[i].found)return PRO_TK_E_NOT_FOUND;
    }
    return PRO_TK_NO_ERROR;
}

static ProError replay_pattern_before_next(ProMdl scratch,int leader,int last,ProFeature **members,int *count){
    ProFeature previous,seed;
    ProPattern pattern;
    ProError error=ProFeatureInit((ProSolid)scratch,last,&previous);
    if(!error)error=ProFeatureInsertmodeWithoptionsActivate(&previous,PRO_REGEN_NO_RESOLVE_MODE);
    if(!error)error=ProFeatureInit((ProSolid)scratch,leader,&seed);
    if(!error)error=ProFeaturePatternGet(&seed,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternMembersGet(&pattern,members);
    if(!error)error=ProArraySizeGet((ProArray)*members,count);
    return error;
}

/* If two direction vectors overlap, omitted slots may coincide with retained
   holes. Keep the native direction pattern only if the member correspondence
   is unique and every extra slot exactly repeats a retained circular cut. */
static ProError replay_coincident_member_map(ProFeature *original,int old_count,
                                             ProFeature *created,int new_count,int *index){
    if(old_count<2||new_count<=old_count||new_count>REPLAY_FEATURE_MAX)return PRO_TK_UNSUPPORTED;
    ReplayPatternBore before[REPLAY_FEATURE_MAX]={0},after[REPLAY_FEATURE_MAX]={0};
    ProError error=replay_pattern_bores(original,old_count,before);
    if(!error)error=replay_pattern_bores(created,new_count,after);
    if(error)return error;
    int next=0;
    for(int i=0;i<old_count;i++){
        while(next<new_count&&!replay_same_bore(&before[i],&after[next]))next++;
        if(next==new_count)return PRO_TK_INCOMPLETE;
        index[i]=next++;
    }
    next=new_count-1;
    for(int i=old_count-1;i>=0;i--){
        while(next>=0&&!replay_same_bore(&before[i],&after[next]))next--;
        if(next!=index[i])return PRO_TK_E_AMBIGUOUS;
        next--;
    }
    if(index[0])return PRO_TK_INCOMPLETE;
    int original_index=0;
    for(int i=0;i<new_count;i++){
        if(original_index<old_count&&i==index[original_index]){
            original_index++;
            continue;
        }
        int repeated=0;
        for(int j=0;j<old_count&&!repeated;j++)repeated=replay_same_bore(&before[j],&after[i]);
        if(!repeated)return PRO_TK_INCOMPLETE;
    }
    return PRO_TK_NO_ERROR;
}

static ProError replay_map_coincident_direction(ProMdl scratch,int seed,ProFeature *old_members,int old_count,
    ProFeature *new_members,int new_count,int *index){
    if(old_count<2||new_count<=old_count||new_count>REPLAY_FEATURE_MAX)return PRO_TK_UNSUPPORTED;
    ProFeature *original=NULL;
    int count=0;
    ProError error=replay_pattern_before_next(scratch,seed,old_members[old_count-1].id,&original,&count);
    if(!error&&count!=old_count)error=PRO_TK_INCOMPLETE;
    if(!error)error=replay_coincident_member_map(original,count,new_members,new_count,index);
    if(original)ProArrayFree((ProArray *)&original);
    return error;
}
static ProError replay_read_direction_members(ProMdl model,int leader,ReplayPatternSource *scratch,
                                               ProFeature **members,int *count){
    ProFeature source, *live=NULL;
    ProElement definition=NULL;
    ProPattern pattern;
    int kind=0,total=0;
    ProError error=ProFeatureInit((ProSolid)model,leader,&source);
    if(!error)error=replay_pattern_definition(&source,scratch,&definition,&kind);
    if(!error&&kind!=PRO_GENPAT_DIR_DRIVEN)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProFeaturePatternGet(&source,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternMembersGet(&pattern,&live);
    if(!error)error=ProArraySizeGet((ProArray)live,&total);
    if(!error&&total<2)error=PRO_TK_INCOMPLETE;
    if(!error)error=replay_pattern_before_next(scratch->model,leader,live[total-1].id,members,count);
    if(!error&&*count!=total)error=PRO_TK_INCOMPLETE;
    if(live)ProArrayFree((ProArray *)&live);
    if(definition)ProElementFree(&definition);
    return error;
}

static ProError replay_verify_coincident_direction(ProMdl source,ProMdl result,int old_id,int new_id){
    ReplayPatternSource before={0},after={0};
    ProFeature *original=NULL,*created=NULL;
    int old_count=0,new_count=0,index[REPLAY_FEATURE_MAX];
    ProError error=replay_read_direction_members(source,old_id,&before,&original,&old_count);
    if(!error)error=replay_read_direction_members(result,new_id,&after,&created,&new_count);
    if(!error)error=replay_coincident_member_map(original,old_count,created,new_count,index);
    if(original)ProArrayFree((ProArray *)&original);
    if(created)ProArrayFree((ProArray *)&created);
    ProError old_cleanup=replay_pattern_source_close(&before);
    ProError new_cleanup=replay_pattern_source_close(&after);
    return error?error:old_cleanup?old_cleanup:new_cleanup;
}
static ProError replay_pattern(ImitationReplay *replay,ProFeature *source,ProFeature *created,ProPatternClass pattern_class){
    ProElement original=NULL,definition=NULL;
    ProPattern old_pattern,new_pattern;
    ProFeature old_header,new_header,*old_members=NULL,*new_members=NULL;
    int kind=0,old_count=0,new_count=0;
    ReplayPatternSource read={0};
    ProError error=replay_pattern_definition_class(source,pattern_class,&read,&original,&kind);
    replay->definition_owner=read.model;
    if(!error&&!(pattern_class==PRO_GROUP_PATTERN?replay_group_pattern_supported(kind):replay_pattern_supported(kind)))
        error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProFeaturePatternGet(source,pattern_class,&old_pattern);
    if(!error)error=ProPatternHeaderGet(&old_pattern,&old_header);
    if(!error)error=ProPatternMembersGet(&old_pattern,&old_members);
    if(!error)error=ProArraySizeGet((ProArray)old_members,&old_count);
    if(!error)error=ProElementAlloc(PRO_E_PATTERN_ROOT,&definition);
    if(!error)error=replay_pattern_field(replay,original,definition,PRO_E_GENPAT_TYPE);
    if(!error)error=replay_pattern_field(replay,original,definition,PRO_E_GENPAT_REGEN_METHOD);
    if(!error)switch(kind){
    case PRO_GENPAT_DIM_DRIVEN:
        error=replay_pattern_field(replay,original,definition,PRO_E_GENPAT_DIM);
        break;
    case PRO_GENPAT_REF_DRIVEN:
        error=replay_ref_pattern_definition(original,definition);
        break;
    case PRO_GENPAT_POINT_DRIVEN:
    case PRO_GENPAT_CRV_DRIVEN:
        error=replay_pattern_sketch(replay,original,definition,kind);
        if(!error)error=replay_pattern_field(replay,original,definition,
            kind==PRO_GENPAT_POINT_DRIVEN?PRO_E_GENPAT_POINT:PRO_E_GENPAT_CURVE);
        break;
    default:
        if(pattern_class==PRO_GROUP_PATTERN)error=replay_group_pattern_branch(replay,original,definition,kind,old_count);
        else error=replay_pattern_field(replay,original,definition,
            kind==PRO_GENPAT_AXIS_DRIVEN?PRO_E_GENPAT_AXIS:PRO_E_GENPAT_DIR);
        break;
    }
    if(!error){error=ProPatternCreate(created,pattern_class,definition);trace("replay create native pattern",error);}
    if(!error){error=part_regenerate(replay->target);trace("replay regenerate native pattern",error);}
    if(!error){error=ProFeaturePatternGet(created,pattern_class,&new_pattern);trace("replay read native pattern",error);}
    if(!error)error=ProPatternHeaderGet(&new_pattern,&new_header);
    if(!error)error=ProPatternMembersGet(&new_pattern,&new_members);
    if(!error)error=ProArraySizeGet((ProArray)new_members,&new_count);
    int member_index[REPLAY_FEATURE_MAX]={0};
    if(!error&&(old_count>REPLAY_FEATURE_MAX||new_count>REPLAY_FEATURE_MAX))error=PRO_TK_OUT_OF_RANGE;
    for(int i=0;!error&&i<old_count;i++)member_index[i]=i;
    if(!error&&old_count!=new_count){
        error=PRO_TK_INCOMPLETE;
        if(kind==PRO_GENPAT_DIR_DRIVEN&&pattern_class==PRO_FEAT_PATTERN)
            error=replay_map_coincident_direction(read.model,source->id,old_members,old_count,
                new_members,new_count,member_index);
        if(error==PRO_TK_UNSUPPORTED||error==PRO_TK_E_AMBIGUOUS||error==PRO_TK_E_NOT_FOUND){
            part_text_add(replay->out,L"这组阵列有排除位置，Creo 生成的实例数不同，未能证明差异只来自重合孔位。\n");
            error=PRO_TK_INCOMPLETE;
        }
    }
    if(!error&&replay->reference_pattern)error=replay_pattern_acknowledge(replay);
    if(!error)error=replay_add_mapping(replay,old_header.id,new_header.id);
    for(int i=0;!error&&i<old_count;i++){
        const int mapped=member_index[i];
        ProFeatStatus old_status,new_status;
        error=ProFeatureStatusGet(&old_members[i],&old_status);
        if(!error)error=ProFeatureStatusGet(&new_members[mapped],&new_status);
        if(!error&&(old_status!=new_status||
           (old_members[i].id==source->id)!=(new_members[mapped].id==created->id)))error=PRO_TK_INCOMPLETE;
        if(!error&&old_members[i].id!=source->id){
            error=replay_add_mapping(replay,old_members[i].id,new_members[mapped].id);
            if(!error&&pattern_class==PRO_GROUP_PATTERN)
                error=replay_group_instance_members(replay,&old_members[i],&new_members[mapped]);
        }
    }
    if(!error){
        ProName label=L"";
        if(!ProModelitemNameGet((ProModelitem *)&old_header,label)&&label[0])
            error=ProModelitemNameSet((ProModelitem *)&new_header,label);
    }
    part_text_add(replay->out,L"%ls阵列种子 ID %d，阵列类型 %d，实例 %d / %d，Creo %d。\n",
        pattern_class==PRO_GROUP_PATTERN?L"特征组":L"",source->id,kind,new_count,old_count,error);
    if(!error&&old_count)replay->last_pattern_member=old_members[old_count-1].id;
    if(old_members)ProArrayFree((ProArray *)&old_members);
    if(new_members)ProArrayFree((ProArray *)&new_members);
    if(original)ProElementFree(&original);
    if(definition)ProElementFree(&definition);
    replay->definition_owner=NULL;
    ProError cleanup=replay_pattern_source_close(&read);
    if(!error)error=cleanup;
    return error;
}
