static ProError fab_execute(ProMdl model,const PartCommand *cmd,PartText *out){
    if(dv_recovery_uncertain||sh_recovery_uncertain)return PRO_TK_BAD_CONTEXT;
    if(fabrication_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    ProError error=part_health(model);
    if(error)return error;
    switch(fab_kind(cmd->arg[0])){
    case FAB_BLANK:error=fab_blank(model,cmd,out);break;
    case FAB_BENDS:error=fab_bends(model,cmd,out);break;
    case FAB_HOLES:error=fab_holes(model,cmd,out);break;
    case FAB_PAINT:error=fab_paint(model,cmd,out);break;
    case FAB_DXF:error=fab_batch_dxf(model,cmd,out);break;
    case FAB_DXF_CHECK:error=fab_preflight(model,cmd,out);break;
    case FAB_TEMPLATE:error=fab_template(model,cmd,out);break;
    case FAB_INSPECT:error=fab_inspect(model,cmd,out);break;
    case FAB_SORT:error=fab_sort(model,cmd,out);break;
    case FAB_READBACK:error=!wcscmp(cmd->arg[2],L"STEP")?fab_readback_step(model,cmd,out):fab_readback_dxf(model,cmd,out);break;
    default:return PRO_TK_BAD_INPUTS;
    }
    trace("fabrication operation result",error);
    return error;
}
