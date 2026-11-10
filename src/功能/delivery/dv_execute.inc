static ProError dv_execute(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
    int kind=dv_kind(cmd->arg[0]);
    PartText out={report,capacity,0};
    ProError error;
    if(delivery_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(dv_recovery_uncertain&&kind!=DV_RECOGNIZE){
        part_text_add(&out,L"上次操作的恢复未能核实，本次会话已停止交付模块写入。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    if(kind!=DV_IMPORT&&(error=dv_part(model)))goto done;
    switch(kind){
    case DV_REF:case DV_REDEFINE:error=dv_redefine(model,cmd,&out);break;
    case DV_REORDER:error=dv_reorder(model,cmd,&out);break;
    case DV_IMPORT:error=dv_import(model,cmd,&out);break;
    case DV_HEAL:error=dv_heal(model,cmd,&out);break;
    case DV_RECOGNIZE:error=dv_recognize(model,cmd,&out);break;
    case DV_ATTR:error=dv_attributes(model,cmd,&out);break;
    case DV_EXPORT:error=dv_export(model,cmd,&out);break;
    case DV_REPORT:error=dv_report(model,cmd,&out);break;
    case DV_PACKAGE:error=dv_package(model,cmd,&out);break;
    default:error=PRO_TK_BAD_INPUTS;break;
    }
done:
    if(error)part_text_add(&out,L"%ls 未完成（Creo %d）。请核对目标零件、参照、文件与工作目录。\n",cmd->arg[0],error);
    return error;
}
