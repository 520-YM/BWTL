static ProError ax_verify_single_export(const wchar_t *folder,const wchar_t *leaf){
    ProPath wildcard;
    WIN32_FIND_DATAW item;
    if(swprintf_s(wildcard,PRO_PATH_SIZE,L"%ls\\*",folder)<0)return PRO_TK_BAD_INPUTS;
    HANDLE find=FindFirstFileW(wildcard,&item);
    if(find==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    int matching=0,other=0;
    do{
        if(!wcscmp(item.cFileName,L".")||!wcscmp(item.cFileName,L".."))continue;
        if(!_wcsicmp(item.cFileName,leaf)&&!(item.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
            matching++;
        else other++;
    }while(FindNextFileW(find,&item));
    FindClose(find);
    return matching==1&&!other?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_step(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxScan *scan=NULL;
    DeliveryOutput output={0};
    ProPath leaf;
    ProError error=ax_scan(assembly,1,&scan);
    if(error)return error;
    int parts=scan->parts;
    for(int i=0;!error&&i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(item->model&&item->type==PRO_MDL_PART){
            ProMassProperty geometry;
            error=qa_geometry_mass(item->model,&geometry);
        }
    }
    free(scan);
    if(error||!parts)return error?error:PRO_TK_E_NOT_FOUND;
    if((error=dv_stage_begin(cmd->arg[2],&output)))return error;
    error=dv_stage_enter(&output);
    if(!error)error=dv_export_file(assembly,output.stage,PRO_INTF_EXPORT_STEP,leaf);
    if(!error){
        error=ax_verify_single_export(output.stage,leaf);
        if(error)part_text_add(out,L"Creo 导出配置产生了多个文件，请使用单文件装配 STEP 配置。\n");
    }
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已导出装配 STEP（%d 个活动零件实例）。\n%ls\\%ls\n",
        parts,output.target,leaf);
    return error;
}
