static int dv_recovery_uncertain;

static ProError dv_part(ProMdl model){
    ProMdlType type;
    ProMdlsubtype subtype;
    ProError error=ProMdlTypeGet(model,&type);
    if(!error)error=ProMdlSubtypeGet(model,&subtype);
    if(error)return error;
    return type==PRO_MDL_PART&&subtype!=PROMDLSTYPE_PART_SHEETMETAL?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError dv_feature(ProMdl model,const wchar_t *name,ProFeature *feature,int *number){
    ProBoolean readonly,embedded;
    ProGroupStatus group;
    ProPatternStatus pattern;
    ProError error;
    if((error=ux_named_feature(model,name,feature))||
       (error=ProFeatureIsReadonly(feature,&readonly))||(error=ProFeatureIsEmbedded(feature,&embedded))||
       (error=ProFeatureGroupStatusGet(feature,&group))||(error=ProFeaturePatternStatusGet(feature,&pattern))||
       (error=ProFeatureNumberGet(feature,number)))return error;
    return readonly||embedded||group!=PRO_GROUP_NONE||pattern!=PRO_PATTERN_NONE?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
}

static ProError dv_work_file(const wchar_t *leaf,ProPath path){
    ProPath directory;
    WIN32_FILE_ATTRIBUTE_DATA info;
    ProError error=ProDirectoryCurrentGet(directory);
    if(error)return error;
    if(!dv_exchange_file(leaf)||swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",directory,leaf)<0)
        return PRO_TK_BAD_INPUTS;
    if(!GetFileAttributesExW(path,GetFileExInfoStandard,&info)||
       (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       !(info.nFileSizeHigh||info.nFileSizeLow))return PRO_TK_CANT_OPEN;
    return PRO_TK_NO_ERROR;
}

static ProError dv_loaded_parts(ProMdl model,const PartCommand *cmd,int start,int count,ProMdl parts[6]){
    parts[0]=model;
    for(int i=0;i<=count;i++){
        ProError error;
        if(i&&(error=ProMdlnameInit((wchar_t *)cmd->arg[start+i-1],PRO_MDL_PART,&parts[i])))return error;
        if((error=dv_part(parts[i]))||(error=part_health(parts[i])))return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError dv_display_new(ProMdl model){
    ProMdlName name;
    int window;
    ProError error;
    if((error=ProMdlMdlnameGet(model,name))||
       (error=ProObjectwindowMdlnameCreate(name,PRO_PART,&window))||
       (error=ProWindowCurrentSet(window))||(error=ProMdlDisplay(model)))return error;
    return ProWindowActivate(window);
}

static ProError dv_import_count_action(ProFeature *feature,ProError status,ProAppData context){
    if(status)return status;
    ProFeattype type;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error&&type==PRO_FEAT_IMPORT)(*(int *)context)++;
    return error;
}

static ProError dv_import_count(ProMdl model,int *count){
    *count=0;
    ProError error=ProSolidFeatVisit((ProSolid)model,dv_import_count_action,NULL,count);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}
