/* Clone the empty Creo template, then place it. The combined create-copy call
   is unsuitable here because native sheet-metal subtype can be lost. */
static ProError ax_new_part_name(ProMdl assembly,const wchar_t *requested,ProMdlName name){
    if(!_wcsicmp(requested,L"SAME"))return ProMdlMdlnameGet(assembly,name);
    if(_wcsicmp(requested,L"AUTO"))
        return part_name(requested)&&!wcscpy_s(name,PRO_NAME_SIZE,requested)?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    const DWORD nonce=GetTickCount();
    for(unsigned i=0;i<16;i++){
        swprintf_s(name,PRO_NAME_SIZE,L"ai_cmp_%08lx_%08lx",GetCurrentProcessId(),nonce+i);
        if(!ax_target_collision(name,PRO_MDL_PART))return PRO_TK_NO_ERROR;
    }
    return PRO_TK_E_FOUND;
}

static ProError ax_new_part_at(ProMdl assembly,const wchar_t *requested,int sheetmetal,ProMatrix wanted,
    int mode,double mm,ProMdl *created_part,ProAsmcomp *created_occurrence,PartText *out){
    const ProMdlsubtype expected=sheetmetal?PROMDLSTYPE_PART_SHEETMETAL:PROMDLSTYPE_PART_SOLID;
    const wchar_t *config=sheetmetal?L"template_sheetmetalpart":L"template_solidpart";
    const wchar_t *kind=sheetmetal?L"钣金":L"实体";
    ProPath template_path=L"";
    ProMdlName name=L"",actual_name=L"";
    ProMdl seed=NULL,child=NULL;
    ProMdlType model_type;
    ProMdlsubtype subtype;
    ProAsmcomp occurrence={0};
    TemplateDatums original={0},created={0};
    double template_mm=0;
    if(created_part)*created_part=NULL;
    if(created_occurrence)memset(created_occurrence,0,sizeof(*created_occurrence));
    ProError error=ax_new_part_name(assembly,requested,name);
    if(!error)error=ax_target_collision(name,PRO_MDL_PART);
    if(error){
        part_text_add(out,L"零件名 %ls 无法使用。请检查当前会话和工作目录内的同名 .prt 文件或读写权限。\n",name);
        return error;
    }

    error=ProConfigoptionGet((wchar_t *)config,template_path);
    if(!error&&!template_path[0])error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProMdlFiletypeLoad(template_path,PRO_MDLFILE_PART,PRO_B_FALSE,&seed);
    if(!error)error=ProMdlTypeGet(seed,&model_type);
    if(!error&&model_type!=PRO_MDL_PART)error=PRO_TK_INVALID_TYPE;
    if(!error)error=ProMdlSubtypeGet(seed,&subtype);
    if(!error&&subtype!=expected)error=PRO_TK_INVALID_TYPE;
    if(!error)error=template_empty((ProSolid)seed);
    if(!error)error=part_health(seed);
    if(!error)error=ProSolidFeatVisit((ProSolid)seed,template_feature,NULL,&original);
    if(error){
        part_text_add(out,L"无法使用 Creo 配置的空%ls零件模板，返回 Creo %d。\n",kind,error);
        return error;
    }
    if((error=qa_mm_per_unit(seed,&template_mm)))return error;
    if(!part_close(template_mm,mm)){
        part_text_add(out,L"装配与空%ls零件模板的长度单位不同，请核对 Creo 模板配置。\n",kind);
        return PRO_TK_BAD_CONTEXT;
    }

    error=ProMdlnameCopy(seed,name,&child);
    if(!error)error=ProMdlMdlnameGet(child,actual_name);
    if(!error&&_wcsicmp(actual_name,name))error=PRO_TK_GENERAL_ERROR;
    if(!error)error=ProMdlSubtypeGet(child,&subtype);
    if(!error&&subtype!=expected)error=PRO_TK_INVALID_TYPE;
    if(!error)error=template_empty((ProSolid)child);
    if(!error)error=part_health(child);
    if(!error)error=ProSolidFeatVisit((ProSolid)child,template_feature,NULL,&created);
    if(!error&&(created.total!=original.total||created.planes!=original.planes||created.csys!=original.csys))
        error=PRO_TK_GENERAL_ERROR;
    if(!error)error=ax_place_model(assembly,child,wanted,mode,mm,&occurrence,out);
    if(error){
        if(child&&!ax_recovery_uncertain&&ProMdlErase(child))
            part_text_add(out,L"新零件 %ls 仍在会话中，尚未保存。\n",name);
        return error;
    }

    if(created_part)*created_part=child;
    if(created_occurrence)*created_occurrence=occurrence;
    part_text_add(out,L"已在指定父装配新建空%ls零件 %ls.prt，组件 ID:%d。已核对模板基准与空几何，未主动保存。\n",
        kind,actual_name,occurrence.id);
    return PRO_TK_NO_ERROR;
}

static ProError ax_new_part_in(ProMdl assembly,const wchar_t *name,int sheetmetal,ProMdl *child,
    ProAsmcomp *occurrence,PartText *out){
    double mm;
    ProError error=qa_mm_per_unit(assembly,&mm);
    if(error)return error;
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    return ax_new_part_at(assembly,name,sheetmetal,identity,AX_PLACE_DEFAULT,mm,child,occurrence,out);
}

static ProError ax_new_part(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    ProMatrix wanted;
    int mode=ax_initial_pose(cmd,4,mm,wanted);
    return ax_new_part_at(assembly,cmd->arg[2],!wcscmp(cmd->arg[3],L"SHEETMETAL"),wanted,mode,mm,NULL,NULL,out);
}