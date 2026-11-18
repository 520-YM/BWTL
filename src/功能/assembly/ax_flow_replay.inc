
static ProError ax_flow_trial_one(ProMdl source,const wchar_t *assembly_case,const wchar_t *id,
    const wchar_t *child,const wchar_t *map,int branch,ProError *outcome,PartText *out){
    static unsigned sequence;
    ProMdlName name;
    swprintf_s(name,_countof(name),L"BW_TRIAL_%lx_%u",GetCurrentProcessId(),++sequence);
    ProMdl copy=NULL;
    ProError error=ProMdlnameCopy(source,name,&copy);
    if(error)return error;
    part_text_add(out,L"候选 %d，支撑对应 %ls。\n",branch,map);
    ax_recovery_uncertain=0;
    *outcome=ax_study_apply(copy,assembly_case,id,child,wcscmp(map,L"-")?map:NULL,0,out);
    if(!*outcome)*outcome=ax_study_audit(copy,NULL,NULL,0,out);
    part_text_add(out,L"候选 %d %ls（Creo %d）。\n",branch,*outcome?L"未通过":L"通过",*outcome);
    int uncertain=ax_recovery_uncertain;
    error=ProMdlErase(copy);
    if(uncertain){
        part_text_add(out,L"独立试件有未能核实的原生状态，已停止该试装流程。\n");
        return PRO_TK_INCOMPLETE;
    }
    return error;
}

static ProError ax_flow_compare_branches(ProMdl assembly,const wchar_t *source,const wchar_t *key,
    const wchar_t *replacement,const wchar_t *left,const wchar_t *right,PartText *out){
    if(!assembly||!*left||!*right||!wcscmp(left,right))return PRO_TK_BAD_INPUTS;
    ProMdlName name;
    ProPath original,origin;
    ProMdlType type;
    wchar_t id[AX_STUDY_PATH],base[MAX_PATH],scratch[MAX_PATH],path[MAX_PATH];
    AxStudyRule rule={0};
    ProMdl component=NULL;
    ProError error=ProMdlTypeGet(assembly,&type);
    if(error||type!=PRO_MDL_ASSEMBLY)return error?error:PRO_TK_BAD_CONTEXT;
    if((error=ProMdlMdlnameGet(assembly,name)))return error;
    if(!_wcsicmp(name,source)){
        part_text_add(out,L"请打开需要试装的目标装配；当前是装配规则来源。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    ProBoolean modified=PRO_B_TRUE;
    if((error=ProMdlModificationVerify(assembly,&modified)))return error;
    if(modified){
        part_text_add(out,L"请先保存目标装配，再在 E 盘独立试件进行比较。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    if((error=ax_order_saved_dependencies(assembly,out)))return error;
    if(!ax_flow_key(source,key,id,out))return PRO_TK_E_NOT_FOUND;
    if((error=ax_study_read(source,id,&rule)))return error;
    if((error=ax_study_find_child(assembly,&rule,replacement,&component,out)))return error;
    if((error=ProDirectoryCurrentGet(original))||(error=ProMdlOriginGet(assembly,origin)))return error;
    if(!storage_method_directory(base,L"装配试装",1)||
        swprintf_s(scratch,_countof(scratch),L"%ls\\probe-%lx-%llx",base,GetCurrentProcessId(),GetTickCount64())<0||
        swprintf_s(path,_countof(path),L"%ls\\候选回执.txt",scratch)<0||!CreateDirectoryW(scratch,NULL))
        return PRO_TK_CANT_WRITE;

    ProWVerstamp before=NULL,after=NULL;
    if((error=ProMdlVerstampGet(assembly,&before)))return error;
    error=ProDirectoryChange(scratch);
    ProError test[2]={PRO_TK_INCOMPLETE,PRO_TK_INCOMPLETE};
    if(!error)error=ax_flow_trial_one(assembly,source,id,replacement,left,1,&test[0],out);
    if(!error)error=ax_flow_trial_one(assembly,source,id,replacement,right,2,&test[1],out);
    ProError verify=ProDirectoryChange(original);
    ProPath after_origin;
    if(!verify)verify=ProMdlOriginGet(assembly,after_origin);
    if(!verify)verify=ProMdlVerstampGet(assembly,&after);
    if(!verify&&(_wcsicmp(origin,after_origin)||!ProVerstampEqual(before,after)))verify=PRO_TK_INCOMPLETE;
    ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    if(verify){part_text_add(out,L"试装后未能确认原装配的工作目录和版本，请先核对当前模型。\n");return verify;}
    if(error)return error;
    part_text_add(out,L"通过 %d/2。原装配版本核对一致，试装记录与基线副本位于独立目录。\n",(test[0]==0)+(test[1]==0));
    FILE *log=_wfopen(path,L"wtx, ccs=UTF-8");
    if(!log)return PRO_TK_CANT_WRITE;
    fwprintf(log,L"%ls\n",out->text);
    int failed=ferror(log);
    if(fclose(log)||failed)return PRO_TK_CANT_WRITE;
    part_text_add(out,L"比较回执已保存到 %ls。\n",path);
    return PRO_TK_NO_ERROR;
}

