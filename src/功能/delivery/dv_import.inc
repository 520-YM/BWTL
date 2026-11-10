static ProIntfImportType dv_import_type(const wchar_t *leaf){
    return dv_exchange_file(leaf)==1?PRO_INTF_IMPORT_STEP:PRO_INTF_IMPORT_IGES;
}

static ProError dv_source_part(ProPath path,ProIntfImportType format){
    ProMdlType type=PRO_MDL_UNUSED;
    ProError error=ProIntfimportWithProfileSourceTypeGet(path,NULL,format,&type);
    if(error)return error;
    return type==PRO_MDL_PART?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError dv_import(ProMdl model,const PartCommand *cmd,PartText *out){
    ProPath path;
    ProMdl created=NULL;
    ProMdlName new_name;
    int imports,bodies;
    ProError error;
    ProIntfImportType format=dv_import_type(cmd->arg[2]);
    if(!part_target_available(model,cmd->arg[1],L"prt"))return PRO_TK_E_FOUND;
    wcscpy_s(new_name,_countof(new_name),cmd->arg[1]);
    if((error=dv_work_file(cmd->arg[2],path))||(error=dv_source_part(path,format)))return error;
    error=ProIntfimportModelWithOptionsMdlnameCreate(path,NULL,format,PRO_MDL_PART,
        PRO_IMPORTREP_MASTER,new_name,NULL,NULL,&created);
    if(!error)error=dv_part(created);
    if(!error)error=dv_import_count(created,&imports);
    if(!error&&!imports)error=PRO_TK_GENERAL_ERROR;
    if(!error)error=sd_body_count(created,&bodies);
    if(!error){
        int healthy=!part_health(created);
        error=dv_display_new(created);
        if(!error)part_text_add(out,L"已将 %ls 导入独立零件 %ls，导入特征 %d，实体 %d。%ls。\n",
            cmd->arg[2],new_name,imports,bodies,
            healthy?(bodies?L"已读取导入几何":L"当前是曲面导入，可检查后尝试修复"):L"检测到重生成问题，需要核对导入几何");
    }
    if(error&&created&&ProMdlErase(created))
        part_text_add(out,L"本次新建导入对象仍在会话中，可检查其状态。\n");
    return error;
}
