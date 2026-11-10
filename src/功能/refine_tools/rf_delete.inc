static ProError rf_delete_check(ProMdl model,int id,const FdTxn *before,double *volume){
    FdTxn after={0};
    ProMassProperty mass;
    int i,bodies;
    ProError e;
    if((e=part_health(model))||(e=sd_body_count(model,&bodies))||
       (e=ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&after))||
       (e=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)))return e;
    if(bodies!=1||after.before_count!=before->before_count-1||!isfinite(mass.volume)||mass.volume<=0)return PRO_TK_BAD_CONTEXT;
    for(i=0;i<after.before_count;i++)
        if(after.before_ids[i]==id||!fd_was_feature(before,after.before_ids[i]))return PRO_TK_BAD_CONTEXT;
    *volume=mass.volume;
    return PRO_TK_NO_ERROR;
}

static ProError rf_delete_backup(ProMdl model,ProMdl *copy,ProPath folder,ProMdlName copy_name){
    ProPath profile;
    WIN32_FIND_DATAW file;
    wchar_t pattern[PRO_PATH_SIZE];
    DWORD length=GetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",profile,_countof(profile)),attributes;
    HANDLE search;
    ProError e;
    if(!length||length>=_countof(profile)||swprintf_s(folder,PRO_PATH_SIZE,L"%ls\\deletion-backups",profile)<0)return PRO_TK_BAD_CONTEXT;
    if(!CreateDirectoryW(folder,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return PRO_TK_CANT_WRITE;
    attributes=GetFileAttributesW(folder);
    if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return PRO_TK_BAD_CONTEXT;
    swprintf_s(copy_name,PRO_MDLNAME_SIZE,L"bwtl_del_%lx_%llx",GetCurrentProcessId(),GetTickCount64());
    if((e=ProMdlnameCopy(model,copy_name,copy))||(e=ProMdlnameBackup(*copy,folder)))return e;
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.prt.*",folder,copy_name)<0)return PRO_TK_CANT_OPEN;
    search=FindFirstFileW(pattern,&file);
    if(search==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    FindClose(search);
    return file.nFileSizeHigh||file.nFileSizeLow?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
}

static ProError rf_delete(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn before={0},restored={0};
    ProMdl copy=NULL;
    ProFeature target,trial;
    ProFeattype type,trial_type;
    ProGroupStatus group;
    ProPatternStatus pattern;
    ProBoolean readonly,embedded;
    ProExtRefInfo *external=NULL;
    ProMassProperty mass;
    ProPath backup=L"";
    ProMdlName copy_name=L"";
    int id,*children=NULL,count=0,bodies,attempted=0,verified=0,i,backup_ok=0;
    double trial_volume,final_volume;
    const char *stage="verify exact active feature ID and name";
    ProError e;
#define DELETE_STEP(call) do{e=(call);if(e)goto done;}while(0)
    part_integer(cmd->arg[3]+8,&id);
    DELETE_STEP(rf_active_feature(model,cmd->arg[2],&target,&type));
    if(target.id!=id||(type!=PRO_FEAT_HOLE&&type!=PRO_FEAT_CUT&&type!=PRO_FEAT_ROUND&&type!=PRO_FEAT_CHAMFER&&
       type!=PRO_FEAT_DRAFT&&type!=PRO_FEAT_RIB&&type!=PRO_FEAT_SHELL&&type!=PRO_FEAT_PROTRUSION)){e=PRO_TK_BAD_CONTEXT;goto done;}
    stage="protect grouped, patterned, embedded and dependent features";
    DELETE_STEP(ProFeatureIsReadonly(&target,&readonly));
    DELETE_STEP(ProFeatureIsEmbedded(&target,&embedded));
    DELETE_STEP(ProFeatureGroupStatusGet(&target,&group));
    DELETE_STEP(ProFeaturePatternStatusGet(&target,&pattern));
    DELETE_STEP(ProFeatureChildrenGet(&target,&children,&count));
    if(readonly||embedded||group!=PRO_GROUP_NONE||pattern!=PRO_PATTERN_NONE||count){e=PRO_TK_BAD_CONTEXT;goto done;}
    e=ProFeatureExternChildrenGet(&target,PRO_ALL_EXT_REF_TYPES,&external,&count);
    if((e&&e!=PRO_TK_E_NOT_FOUND)||count){if(!e)e=PRO_TK_BAD_CONTEXT;goto done;}
    DELETE_STEP(sd_body_count(model,&bodies));
    if(bodies!=1){e=PRO_TK_BAD_CONTEXT;goto done;}
    DELETE_STEP(ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&before));
    DELETE_STEP(ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass));
    before.volume=mass.volume;
    stage="save and verify isolated recovery copy";
    DELETE_STEP(rf_delete_backup(model,&copy,backup,copy_name));
    backup_ok=1;
    stage="trial delete only the named leaf in recovery copy";
    DELETE_STEP(rf_active_feature(copy,cmd->arg[2],&trial,&trial_type));
    if(trial.id!=id||trial_type!=type){e=PRO_TK_BAD_CONTEXT;goto done;}
    DELETE_STEP(model_remove_feature((ProSolid)copy,id));
    DELETE_STEP(part_regenerate(copy));
    DELETE_STEP(rf_delete_check(copy,id,&before,&trial_volume));
    stage="delete only the verified leaf in the original";
    attempted=1;
    DELETE_STEP(model_remove_feature((ProSolid)model,id));
    DELETE_STEP(part_regenerate(model));
    stage="verify original matches successful deletion trial";
    DELETE_STEP(rf_delete_check(model,id,&before,&final_volume));
    if(!part_close(final_volume,trial_volume)){e=PRO_TK_GENERAL_ERROR;goto done;}
    swprintf_s(report,capacity,L"已删除叶子特征 %ls（ID %d）。重生成通过，其余特征 ID 保持。当前零件未保存。恢复副本位于 %ls，文件名 %ls.prt。",
        cmd->arg[2],id,backup,copy_name);
done:
    if(e){
        trace(stage,e);
        if(attempted&&!part_health(model)&&!ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&restored)&&
           restored.before_count==before.before_count&&!ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)&&
           part_close(mass.volume,before.volume)){
            for(i=0;i<restored.before_count&&fd_was_feature(&before,restored.before_ids[i]);i++){}
            verified=i==restored.before_count;
        }
        if(attempted&&!verified)mc_recovery_uncertain=1;
        swprintf_s(report,capacity,L"删除未完成：%hs，错误码%d。%ls。%ls%ls%ls%ls",stage,e,
            !attempted?L"当前零件未提交删除":verified?L"已核对原特征和体积保持":L"恢复未能核实，本次会话已停止加工写入",
            backup_ok?L"恢复副本位于 ":L"",backup_ok?backup:L"",backup_ok?L"，文件名 ":L"",backup_ok?copy_name:L"");
    }
    if(children)ProArrayFree((ProArray *)&children);
    if(external)ProExtRefInfoFree(&external);
    if(copy)ProMdlErase(copy);
    return e;
#undef DELETE_STEP
}
