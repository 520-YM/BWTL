static ProError dv_export_logs_remove(const wchar_t *folder,const wchar_t *name){
    ProPath pattern,path;
    WIN32_FIND_DATAW entry;
    if(swprintf_s(pattern,PRO_PATH_SIZE,L"%ls\\%ls__out.log.*",folder,name)<0)return PRO_TK_BAD_INPUTS;
    HANDLE find=FindFirstFileW(pattern,&entry);
    if(find==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
    ProError error=PRO_TK_NO_ERROR;
    do{
        int version;
        const wchar_t *dot=wcsrchr(entry.cFileName,L'.');
        if(!dot||!part_integer(dot+1,&version)||version<1||
           (entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
           swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",folder,entry.cFileName)<0||
           !DeleteFileW(path)){error=PRO_TK_CANT_WRITE;break;}
    }while(FindNextFileW(find,&entry));
    FindClose(find);
    return error;
}

static ProError dv_export_file(ProMdl model,const wchar_t *folder,ProIntf3DExportType format,ProPath leaf){
    ProPath base,path;
    ProMdlName name;
    ProError error=ProMdlMdlnameGet(model,name);
    if(error)return error;
    if(!dv_safe_leaf(name)||swprintf_s(base,PRO_PATH_SIZE,L"%ls\\%ls",folder,name)<0)return PRO_TK_BAD_INPUTS;
    error=ProIntf3DFileWriteWithDefaultProfile((ProSolid)model,format,base);
    if(error)return error;
    const wchar_t *extensions[]={format==PRO_INTF_EXPORT_STEP?L"stp":L"igs",
                                 format==PRO_INTF_EXPORT_STEP?L"step":L"iges"};
    for(int i=0;i<2;i++){
        if(swprintf_s(leaf,PRO_PATH_SIZE,L"%ls.%ls",name,extensions[i])<0||
           swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",folder,leaf)<0)return PRO_TK_BAD_INPUTS;
        if(dv_file_nonempty(path))return dv_export_logs_remove(folder,name);
    }
    return PRO_TK_CANT_WRITE;
}

static ProError dv_export(ProMdl model,const PartCommand *cmd,PartText *out){
    int additional=cmd->count-4;
    int step=wcscmp(cmd->arg[3],L"IGES")!=0,iges=wcscmp(cmd->arg[3],L"STEP")!=0;
    ProMdl parts[6];
    DeliveryOutput output={0};
    ProPath leaf;
    ProMassProperty mass;
    ProError error=dv_loaded_parts(model,cmd,4,additional,parts);
    if(error)return error;
    for(int i=0;i<=additional;i++)if((error=qa_geometry_mass(parts[i],&mass)))return error;
    if((error=dv_stage_begin(cmd->arg[2],&output)))return error;
    error=dv_stage_enter(&output);
    for(int i=0;!error&&i<=additional;i++){
        if(step)error=dv_export_file(parts[i],output.stage,PRO_INTF_EXPORT_STEP,leaf);
        if(!error&&iges)error=dv_export_file(parts[i],output.stage,PRO_INTF_EXPORT_IGES,leaf);
    }
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已批量导出 %d 个零件的 %d 个 %ls 文件。已核对输出文件非空。\n目录 %ls\n",
        additional+1,(additional+1)*(step+iges),cmd->arg[3],output.target);
    return error;
}
