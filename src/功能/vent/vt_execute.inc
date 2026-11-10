static ProError vt_execute(const PartCommand *cmd,ProMdl model,int sheet,wchar_t *report,size_t capacity){
    static const wchar_t *const labels[]={L"",L"圆孔散热区",L"长槽散热区",L"百叶通风口",L"冲孔网格区",
        L"面板底部排水孔",L"线缆防水接头孔",L"护线圈安装口",L"挡水台阶",L"矩形密封槽"};
    FdTxn tx={0};
    PanelFrame frame={0};
    PartText out={report,capacity,0};
    int k=vt_kind(cmd->arg[0]),bodies=0,started=0;
    ProError error;
    if(vent_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(k==VT_ROUTE)return vt_route(model,cmd,report,capacity);
    if(sheet&&(k==VT_LOUVER||k==VT_STOP||k==VT_SEAL)){
        wcscpy_s(report,capacity,L"此操作使用普通单实体零件的原生实体特征，请在普通零件中执行。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(pn_recovery_uncertain||(sheet&&sh_recovery_uncertain)){
        wcscpy_s(report,capacity,L"上次加工的恢复未能核实，本会话已停止对应写入。");
        return PRO_TK_BAD_CONTEXT;
    }
#define VT(call,label) do{tx.stage=label;error=(call);trace(label,error);if(error)goto done;}while(0)
    VT(sheet?sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT):fd_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT),
        "prepare ventilation feature");
    VT(sd_body_count(model,&bodies),"read ventilation body count");
    if(bodies!=1||!tx.has_volume||tx.volume<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
    VT(pn_frame(&tx,cmd,&frame),"verify face and outward coordinate system");
    started=1;
    part_text_add(&out,L"%ls %ls。",labels[k],tx.name);
    switch(k){
    case VT_ROUND:VT(vt_round(&tx,cmd,&frame,sheet,&out),"create round ventilation array");break;
    case VT_SLOT:VT(vt_slot(&tx,cmd,&frame,sheet,&out),"create capsule ventilation array");break;
    case VT_LOUVER:VT(vt_louver(&tx,cmd,&frame,&out),"create integral louver and opening");break;
    case VT_GRID:VT(vt_grid(&tx,cmd,&frame,sheet,&out),"create rectangular punched grid");break;
    case VT_DRAIN:VT(vt_drain(&tx,cmd,&frame,sheet,&out),"verify lower edge and cut drain");break;
    case VT_GLAND:VT(vt_gland(&tx,cmd,&frame,sheet,&out),"create gland opening");break;
    case VT_GROMMET:VT(vt_grommet(&tx,cmd,&frame,sheet,&out),"create grommet opening");break;
    case VT_STOP:VT(vt_stop(&tx,cmd,&frame,&out),"create raised water barrier");break;
    case VT_SEAL:VT(vt_seal(&tx,cmd,&frame,&out),"create blind rectangular seal groove");break;
    default:error=PRO_TK_BAD_INPUTS;goto done;
    }
    VT(part_health(model),"verify ventilation feature regeneration");
    int after=0;
    VT(sd_body_count(model,&after),"verify ventilation feature retained one body");
    if(after!=bodies){error=PRO_TK_BAD_CONTEXT;goto done;}
    part_text_add(&out,L"未保存。");
done:
    if(error&&started){
        am_finish(&tx,error,bodies,report,capacity);
        swprintf_s(report,capacity,L"%ls未完成（%hs，Creo %d）。%ls。",labels[k],tx.stage,error,
            tx.rollback_ok?L"已核对原实体和本次特征状态":L"恢复未能核实，请检查模型");
        if(!tx.rollback_ok)pn_recovery_uncertain=1;
    }else{
        if(error)swprintf_s(report,capacity,L"%ls未执行（%hs，Creo %d）。请核对单实体、加工面和定位基准。",labels[k],tx.stage,error);
        fd_cleanup(&tx,0);
    }
    return error;
#undef VT
}
