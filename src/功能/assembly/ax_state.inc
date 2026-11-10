static ProError ax_state_apply(ProMdl assembly,int *ids,int suppress){
    if(suppress){
        ProFeatureDeleteOptions *options=NULL;
        ProError error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
        if(!error){
            options[0]=PRO_FEAT_DELETE_NO_OPTS;
            error=ProFeatureWithoptionsSuppress((ProSolid)assembly,ids,options,PRO_REGEN_NO_RESOLVE_MODE);
        }
        if(options)ProArrayFree((ProArray *)&options);
        return error;
    }
    ProFeatureResumeOptions *options=NULL;
    ProError error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        options[0]=PRO_FEAT_RESUME_NO_OPTS;
        error=ProFeatureWithoptionsResume((ProSolid)assembly,ids,options,PRO_REGEN_NO_RESOLVE_MODE);
    }
    if(options)ProArrayFree((ProArray *)&options);
    return error;
}

static ProError ax_state(ProMdl assembly,const PartCommand *cmd,PartText *out){
    ProAsmcomp feature;
    int id[AX_PATH_DEPTH],*ids=NULL;
    int suppress=!wcscmp(cmd->arg[3],L"SUPPRESS");
    ProFeatStatus before,actual;
    ProFeatStatus desired=suppress?PRO_FEAT_SUPPRESSED:PRO_FEAT_ACTIVE;
    ProError error;
    if(ax_path_parse(cmd->arg[2],id)!=1)return PRO_TK_BAD_INPUTS;
    if((error=ax_component_feature(assembly,id[0],&feature))||
       (error=ProFeatureStatusGet(&feature,&before))||
       (error=ax_ordinary_direct(&feature))||
       (error=ax_leaf_without_children(&feature)))return error;
    if(before!=(suppress?PRO_FEAT_ACTIVE:PRO_FEAT_SUPPRESSED))return PRO_TK_BAD_CONTEXT;
    if((error=ProArrayAlloc(1,sizeof(int),1,(ProArray *)&ids)))return error;
    ids[0]=id[0];
    error=ax_state_apply(assembly,ids,suppress);
    if(!error)error=part_regenerate(assembly);
    if(!error)error=ProFeatureStatusGet(&feature,&actual);
    if(!error&&actual!=desired)error=PRO_TK_GENERAL_ERROR;
    if(error){
        if(ProFeatureStatusGet(&feature,&actual)||
           (actual!=before&&ax_state_apply(assembly,ids,!suppress))||
           part_regenerate(assembly)||ProFeatureStatusGet(&feature,&actual)||actual!=before)ax_uncertain(out);
    }else part_text_add(out,L"%ls 已%ls，重生成及组件状态读回通过。\n",cmd->arg[2],suppress?L"抑制":L"恢复");
    ProArrayFree((ProArray *)&ids);
    return error;
}
