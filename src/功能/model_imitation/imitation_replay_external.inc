typedef struct {
    ProModelitem item;
    ProAsmcomppath consumer,provider;
    int index;
} ReplayExternalReference;

static int replay_pattern_context_reference(ProExtFeatRef ref,const ProModelitem *item){
    ProFeature consumer;
    ProPatternStatus state;
    if(ProExtRefOwnFeatGet(ref,&consumer)||ProFeaturePatternStatusGet(&consumer,&state)||
       state!=PRO_PATTERN_HEADER)return 0;
    if(item->type==PRO_PART||item->type==PRO_ASSEMBLY)return item->id>0;
    ProFeature provider;
    return item->type==PRO_FEATURE&&item->owner&&
        !ProFeatureInit((ProSolid)item->owner,item->id,&provider)&&
        !ProFeaturePatternStatusGet(&provider,&state)&&state==PRO_PATTERN_HEADER;
}

static ProError replay_external_state(ProMdl source,int *total){
    ProExtRefInfo *groups=NULL;
    int count=0;
    *total=0;
    ProError error=ProSolidExternParentsGet((ProSolid)source,PRO_ALL_EXT_REF_TYPES,&groups,&count);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<count;i++)for(int j=0;!error&&j<groups[i].n_refs;j++){
        ProRefState state;
        ProModelitem item={0};
        ProAsmcomppath consumer={0},provider={0};
        ProExtFeatRef ref=groups[i].ext_refs[j];
        if(++*total>REPLAY_EXTERNAL_MAX){error=PRO_TK_OUT_OF_RANGE;break;}
        if(ProExtRefStateGet(ref,&state)||state!=PRO_REF_ACTIVE||
           ProExtRefModelitemGet(ref,&item)||
           ProExtRefAsmcompsGet(ref,&consumer,&provider))error=PRO_TK_BAD_CONTEXT;
        else if((!consumer.owner||consumer.owner!=provider.owner)&&
                (consumer.table_num||provider.table_num||!replay_pattern_context_reference(ref,&item)))
            error=PRO_TK_BAD_CONTEXT;
        if(error){
            trace("Replay external preflight entry",*total);
            trace("Replay external preflight type",item.type);
            trace("Replay external preflight ID",item.id);
        }
    }
    if(groups)ProExtRefInfoFree(&groups);
    return error;
}

static ProError replay_source_dependencies(ProMdl source,int *external){
    ProError error=replay_external_state(source,external);
    if(error)return error;
    ProMdl generic=NULL;
    error=ProFaminstanceGenericGet(source,1,&generic);
    if(!error)return PRO_TK_NO_ERROR;
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    ProFamtable table;
    error=ProFamtableInit(source,&table);
    if(!error){
        error=ProFamtableCheck(&table);
        if(!error)return PRO_TK_BAD_CONTEXT;
    }
    return error==PRO_TK_E_NOT_FOUND||error==PRO_TK_EMPTY?PRO_TK_NO_ERROR:error;
}


static int replay_same_path(const ProAsmcomppath *a,const ProAsmcomppath *b){
    return a->owner==b->owner&&a->table_num==b->table_num&&a->table_num>=0&&
        a->table_num<=_countof(a->comp_id_table)&&
        !memcmp(a->comp_id_table,b->comp_id_table,a->table_num*sizeof(a->comp_id_table[0]));
}

static ProError replay_external_position(ReplayExternalReference *ref,ProVector input,ProVector output,int vector)
{
    ProMatrix to_root,from_root;
    ProVector intermediate;
    ProError error=ProAsmcomppathTrfGet(&ref->provider,PRO_B_TRUE,to_root);
    if(!error)error=ProAsmcomppathTrfGet(&ref->consumer,PRO_B_FALSE,from_root);
    if(!error)error=vector?ProVectorTrfEval(input,to_root,intermediate):
        ProPntTrfEval(input,to_root,intermediate);
    if(!error)error=vector?ProVectorTrfEval(intermediate,from_root,output):
        ProPntTrfEval(intermediate,from_root,output);
    return error;
}

static ProError replay_external_reference(ImitationReplay *replay,ProReference ref,ReplayExternalReference *found){
    ProMdl owner=NULL;
    ProType type;
    int id;
    ProError error=ProReferenceOwnerGet(ref,&owner);
    if(!error)error=ProReferenceTypeGet(ref,&type);
    if(!error)error=ProReferenceIdGet(ref,&id);
    ProAsmcomppath path={0};
    if(!error)error=ProReferenceAsmcomppathGet(ref,&path);
    if(error)return error;
    ProExtRefInfo *groups=NULL;
    int count=0,matches=0,index=0;
    error=ProSolidExternParentsGet((ProSolid)replay->source,PRO_ALL_EXT_REF_TYPES,&groups,&count);
    for(int i=0;!error&&i<count;i++)for(int j=0;j<groups[i].n_refs;j++,index++){
        ProExtFeatRef ext=groups[i].ext_refs[j];
        ProFeature user;
        ProRefState state;
        ReplayExternalReference candidate={0};
        if(ProExtRefOwnFeatGet(ext,&user)||user.id!=replay->current_feature||
           ProExtRefStateGet(ext,&state)||state!=PRO_REF_ACTIVE||
           ProExtRefModelitemGet(ext,&candidate.item)||candidate.item.owner!=owner||
           candidate.item.type!=type||candidate.item.id!=id||
           ProExtRefAsmcompsGet(ext,&candidate.consumer,&candidate.provider)||
           !replay_same_path(&candidate.provider,&path))continue;
        ProMdl consumer_model=NULL,provider_model=NULL;
        if(candidate.consumer.owner!=candidate.provider.owner||
           ProAsmcomppathMdlGet(&candidate.consumer,&consumer_model)||consumer_model!=replay->source||
           ProAsmcomppathMdlGet(&candidate.provider,&provider_model)||provider_model!=owner)continue;
        if(index>=REPLAY_EXTERNAL_MAX){error=PRO_TK_OUT_OF_RANGE;break;}
        if(!matches||!replay_same_path(&candidate.consumer,&found->consumer)){
            candidate.index=index;
            *found=candidate;
            matches++;
        }
    }
    if(groups)ProExtRefInfoFree(&groups);
    if(error)return error;
    return matches==1?PRO_TK_NO_ERROR:matches?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;
}

static ProError replay_local_point(ImitationReplay *replay,ProVector xyz,ProModelitem *target)
{
    ProName name;
    swprintf_s(name,_countof(name),L"BWTL_XPT_%03d",++replay->external_datums);
    FdTxn tx={0};
    FdRef base={0};
    ProElement points=NULL,point=NULL,name_element=NULL;
    ProError error=sh_begin(&tx,replay->target,name,PRO_FEAT_DATUM_POINT);
    if(!error){
        error=fd_ref_resolve(replay->target,L"CSYS:PRT_CSYS_DEF",PRO_CSYS,&base);
        if(error==PRO_TK_E_NOT_FOUND)error=fd_ref_resolve(replay->target,L"CSYS:PRT_CSYS",PRO_CSYS,&base);
    }
    if(!error)error=model_element(tx.root,PRO_E_DPOINT_TYPE,1,PRO_DPOINT_TYPE_OFFSET_CSYS,NULL);
    if(!error)error=model_element(tx.root,PRO_E_DPOINT_OFST_CSYS_TYPE,1,PRO_DTMPNT_OFFCSYS_CARTESIAN,NULL);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_DPOINT_OFST_CSYS_REF,&base);
    if(!error)error=model_element(tx.root,PRO_E_DPOINT_OFST_CSYS_WITH_DIMS,1,PRO_B_TRUE,NULL);
    if(!error)error=model_element(tx.root,PRO_E_DPOINT_OFST_CSYS_PNTS_ARRAY,0,0,&points);
    if(!error)error=model_element(points,PRO_E_DPOINT_OFST_CSYS_PNT,0,0,&point);
    if(!error)error=model_element(point,PRO_E_DPOINT_OFST_CSYS_PNT_NAME,0,0,&name_element);
    if(!error)error=ProElementWstringSet(name_element,name);
    const ProElemId coordinates[]={PRO_E_DPOINT_OFST_CSYS_DIR1_VAL,PRO_E_DPOINT_OFST_CSYS_DIR2_VAL,
        PRO_E_DPOINT_OFST_CSYS_DIR3_VAL};
    for(int i=0;!error&&i<3;i++){
        ProElement value;
        error=model_element(point,coordinates[i],2,xyz[i],&value);
        if(!error)error=ProElementDecimalsSet(value,8);
        if(!error)error=ProElementDoubleSet(value,xyz[i]);
    }
    if(!error)error=fd_create(&tx,0);
    if(!error)error=fd_verify(&tx);
    if(!error)error=fd_feature_geom(&tx.feature,PRO_POINT,target);
    FdRef actual={0};
    ProVector verified;
    if(!error){actual.item=*target;error=ad_point(&actual,verified);}
    if(!error&&!ad_near(xyz,verified,0.0002)){
        part_text_add(replay->out,L"本地点读回为 (%.8g, %.8g, %.8g)，期望 (%.8g, %.8g, %.8g)。\n",
            verified[0],verified[1],verified[2],xyz[0],xyz[1],xyz[2]);
        error=PRO_TK_INCOMPLETE;
    }
    fd_ref_free(&base);
    fd_cleanup(&tx,error);
    return error;
}

static ProError replay_external_point(ImitationReplay *replay,ProReference ref,ProModelitem *target)
{
    ReplayExternalReference ext={0};
    ProVector point,localized;
    ProError error=replay_external_reference(replay,ref,&ext);
    FdRef item={.item=ext.item};
    if(!error&&replay->external_local[ext.index].owner==replay->target){
        *target=replay->external_local[ext.index];
        return PRO_TK_NO_ERROR;
    }
    if(!error)error=ad_point(&item,point);
    if(!error)error=replay_external_position(&ext,point,localized,0);
    if(!error)error=replay_local_point(replay,localized,target);
    if(!error){
        replay->external_used[ext.index]=1;
        replay->external_local[ext.index]=*target;
        part_text_add(replay->out,L"外部点 ID %d 已建立本地点 ID %d，坐标 (%.8g, %.8g, %.8g)。\n",
            ext.item.id,target->id,localized[0],localized[1],localized[2]);
    }
    return error;
}

static ProError replay_external_line(ImitationReplay *replay,ReplayExternalReference *ext,ProModelitem *target)
{
    if(replay->external_local[ext->index].owner==replay->target){
        *target=replay->external_local[ext->index];
        return PRO_TK_NO_ERROR;
    }
    ProVector a,b,m,local[2],expected;
    ProCurve original;
    ProEnttype type;
    ProError error=ProGeomitemToCurve(&ext->item,&original);
    if(!error)error=ProCurveTypeGet(original,&type);
    if(!error&&type!=PRO_ENT_LINE)return PRO_TK_UNSUPPORTED;
    if(!error)error=ProCurveXyzdataEval(original,0,a,NULL,NULL);
    if(!error)error=ProCurveXyzdataEval(original,1,b,NULL,NULL);
    if(!error)error=replay_external_position(ext,a,local[0],0);
    if(!error)error=replay_external_position(ext,b,local[1],0);
    if(!error&&cm_distance(local[0],local[1])<=0.0002)error=PRO_TK_INCOMPLETE;
    if(error)return error;

    ProName name;
    swprintf_s(name,_countof(name),L"BWTL_XLN_%03d",++replay->external_datums);
    FdTxn tx={0};
    FdRef basis={0};
    ProElement equation=NULL;
    const wchar_t *basis_name=L"CSYS:PRT_CSYS_DEF";
    wchar_t expression[256];
    error=sh_begin(&tx,replay->target,name,PRO_FEAT_CURVE);
    if(!error){
        error=fd_ref_resolve(replay->target,basis_name,PRO_CSYS,&basis);
        if(error==PRO_TK_E_NOT_FOUND)error=fd_ref_resolve(replay->target,L"CSYS:PRT_CSYS",PRO_CSYS,&basis);
    }
    if(!error)error=model_element(tx.root,PRO_E_CURVE_TYPE,1,PRO_CURVE_TYPE_FROM_EQUATION,NULL);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_CRV_FR_EQ_REF_CSYS,&basis);
    if(!error)error=model_element(tx.root,PRO_E_CRV_FR_EQ_CSYS_TYPE,1,PRO_CRV_FR_EQ_CSYS_TYPE_CARTESIAN,NULL);
    if(!error)error=model_element(tx.root,PRO_E_CRV_FR_EQ_PARAM_MIN,2,0,NULL);
    if(!error)error=model_element(tx.root,PRO_E_CRV_FR_EQ_PARAM_MAX,2,1,NULL);
    if(!error&&swprintf_s(expression,_countof(expression),L"x=%.15g+(%.15g)*t\ny=%.15g+(%.15g)*t\nz=%.15g+(%.15g)*t",
        local[0][0],local[1][0]-local[0][0],local[0][1],local[1][1]-local[0][1],local[0][2],local[1][2]-local[0][2])<0)
        error=PRO_TK_BAD_INPUTS;
    if(!error)error=model_element(tx.root,PRO_E_CRV_ENTER_EQUATION,0,0,&equation);
    if(!error)error=ProElementWstringSet(equation,expression);
    if(!error)error=fd_create(&tx,0);
    if(!error)error=fd_verify(&tx);
    ProCurve curve;
    if(!error)error=ad_single_curve(&tx.feature,target,&curve);
    for(int i=0;!error&&i<3;i++){
        const double t=i/2.0;
        for(int j=0;j<3;j++)expected[j]=local[0][j]+t*(local[1][j]-local[0][j]);
        error=ProCurveXyzdataEval(curve,t,m,NULL,NULL);
        if(!error&&!ad_near(m,expected,0.0002))error=PRO_TK_INCOMPLETE;
    }
    fd_ref_free(&basis);
    fd_cleanup(&tx,error);
    if(!error){
        replay->external_used[ext->index]=1;
        replay->external_local[ext->index]=*target;
        part_text_add(replay->out,L"外部直线 ID %d 已重画为本地方程曲线 %ls。\n",ext->item.id,name);
    }
    return error;
}

static ProError replay_external_surface(ImitationReplay *replay,ProReference ref,ProModelitem *target){
    ReplayExternalReference ext={0};
    ProVector point,normal,local_point,local_normal;
    ProError error=replay_external_reference(replay,ref,&ext);
    if(!error)error=fd_plane_data(&ext.item,point,normal);
    if(!error)error=replay_external_position(&ext,point,local_point,0);
    if(!error)error=replay_external_position(&ext,normal,local_normal,1);
    if(error)return error;
    if(!ad_unit(local_normal))return PRO_TK_INCOMPLETE;
    int axis=0;
    for(int i=1;i<3;i++)if(fabs(local_normal[i])>fabs(local_normal[axis]))axis=i;
    if(fabs(local_normal[axis])<0.9999999)return PRO_TK_UNSUPPORTED;
    const wchar_t *base=imitation_template_plane(replay->target,axis);
    FdRef reference={0};
    FdTxn tx={0};
    ProVector origin,direction,delta;
    if(!base)return PRO_TK_E_NOT_FOUND;
    error=fd_ref_resolve(replay->target,base,PRO_SURFACE,&reference);
    if(!error)error=fd_plane_data(&reference.item,origin,direction);
    if(!error&&(!ad_unit(direction)||ad_dot(direction,local_normal)<0.9999999))error=PRO_TK_UNSUPPORTED;
    if(!error){
        ad_sub(local_point,origin,delta);
        double offset=ad_dot(delta,direction);
        if(fabs(offset)<=0.0002){
            *target=reference.item;
        }else{
            wchar_t name[PRO_NAME_SIZE];
            swprintf_s(name,_countof(name),L"BWTL_XREF_%03d",++replay->external_planes);
            error=sh_begin(&tx,replay->target,name,PRO_FEAT_DATUM);
            ProElement constraints=NULL,entry=NULL;
            if(!error)error=model_element(tx.root,PRO_E_DTMPLN_CONSTRAINTS,0,0,&constraints);
            if(!error)error=model_element(constraints,PRO_E_DTMPLN_CONSTRAINT,0,0,&entry);
            if(!error)error=model_element(entry,PRO_E_DTMPLN_CONSTR_TYPE,1,PRO_DTMPLN_OFFS,NULL);
            if(!error)error=fd_ref_element(&tx,entry,PRO_E_DTMPLN_CONSTR_REF,&reference);
            if(!error)error=model_element(entry,PRO_E_DTMPLN_CONSTR_REF_OFFSET,2,offset,NULL);
            if(!error)error=fd_create(&tx,0);
            if(!error)error=fd_verify(&tx);
            if(!error)error=fd_feature_geom(&tx.feature,PRO_SURFACE,target);
            if(!error)error=fd_plane_data(target,origin,direction);
            if(!error){
                ad_sub(local_point,origin,delta);
                if(!ad_unit(direction)||ad_dot(direction,local_normal)<0.9999999||
                   fabs(ad_dot(delta,direction))>0.0002)error=PRO_TK_INCOMPLETE;
            }
        }
    }
    if(!error){
        if(ext.index<0||ext.index>=replay->external_total)error=PRO_TK_OUT_OF_RANGE;
        else replay->external_used[ext.index]=1;
    }
    if(!error)part_text_add(replay->out,L"源特征 %d 的外部平面 %d 已在新件建立本地参照 %d。\n",
        replay->current_feature,ext.item.id,target->id);
    fd_cleanup(&tx,PRO_TK_NO_ERROR);
    fd_ref_free(&reference);
    return error;
}


static ProError replay_external_section_orientation(ImitationReplay *replay,FdTxn *tx,ProSection source){
    ProElement setup=NULL,orientation=NULL,direction=NULL;
    ProError error=mc_tree_element(tx->root,(ProElemId[]){PRO_E_STD_SECTION,PRO_E_STD_SEC_SETUP_PLANE},2,&setup);
    if(error)return error;
    error=mc_tree_element(setup,(ProElemId[]){PRO_E_STD_SEC_PLANE_ORIENT_REF},1,&orientation);
    if(!error)return PRO_TK_NO_ERROR;
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    ProMatrix frame;
    error=ProSectionLocationGet(source,frame);
    if(error)return error;
    int axis=0;
    for(int i=1;i<3;i++)if(fabs(frame[1][i])>fabs(frame[1][axis]))axis=i;
    if(fabs(frame[1][axis])<0.9999999)return PRO_TK_UNSUPPORTED;
    const wchar_t *base=imitation_template_plane(replay->target,axis);
    if(!base)return PRO_TK_E_NOT_FOUND;
    FdRef ref={0};
    ProVector point,normal;
    error=fd_ref_resolve(replay->target,base,PRO_SURFACE,&ref);
    if(!error)error=fd_plane_data(&ref.item,point,normal);
    if(!error)error=mc_tree_element(setup,(ProElemId[]){PRO_E_STD_SEC_PLANE_ORIENT_DIR},1,&direction);
    if(!error)error=ProElementIntegerSet(direction,
        ad_dot(normal,frame[1])>0?PRO_SEC_ORIENT_DIR_UP:PRO_SEC_ORIENT_DIR_DOWN);
    if(!error)error=fd_ref_element(tx,setup,PRO_E_STD_SEC_PLANE_ORIENT_REF,&ref);
    fd_ref_free(&ref);
    return error;
}
