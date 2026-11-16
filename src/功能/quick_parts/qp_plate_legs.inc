static ProError quick_plate_legs(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
    FdTxn tx={0};
    ProSolidBody body;
    ProOutline bounds;
    QuickPlan plan={0};
    const double height=as_arg(cmd,2);
    plan.leg_width=as_arg(cmd,3);
    plan.leg_inset=as_arg(cmd,4);
    ProFeature existing;
    ProError error=fd_begin(&tx,model,L"QP_STOOL_LEGS",PRO_FEAT_PROTRUSION);
    if(!error&&!ux_named_feature(model,L"QP_STOOL_LEG_LAYOUT",&existing))error=PRO_TK_E_FOUND;
    if(!error)error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodyOutlineGet(&body,bounds);
    if(!error){
        plan.width=bounds[1][0]/tx.scale;
        plan.depth=bounds[1][1]/tx.scale;
        plan.thickness=bounds[1][2]/tx.scale;
        plan.height=plan.thickness+height;
        error=quick_box_check(&tx,plan.width,plan.depth,plan.thickness,0);
        const double box_volume=bounds[1][0]*bounds[1][1]*bounds[1][2];
        if(!error&&(!tx.has_volume||box_volume<=0||!part_close(tx.volume,box_volume)||
            2*(plan.leg_width+plan.leg_inset)+0.3>fmin(plan.width,plan.depth)))error=PRO_TK_BAD_CONTEXT;
    }
    if(error){
        fd_cleanup(&tx,0);
        swprintf_s(report,capacity,L"四角方腿未添加（Creo %d）。此工具适用于从原点沿 +Z 建立的单实体矩形板，四腿位置需在板面范围内。",error);
        return error;
    }

    tx.stage="build four square legs from the measured lower face";
    error=quick_stool(&tx,&plan);
    if(!error)error=quick_stool_outline(&tx,&plan,0);
    if(!error)error=part_health(model);
    if(error){
        am_finish(&tx,error,1,report,capacity);
        return error;
    }
    fd_cleanup(&tx,0);
    swprintf_s(report,capacity,L"已在当前 %.8g × %.8g × %.8g mm 板的实测底面添加四条方腿。"
        L"腿宽 %.8g mm，高 %.8g mm，距板边 %.8g mm，总高 %.8g mm。已核对四处连接、实体尺寸及再生，未保存。",
        plan.width,plan.depth,plan.thickness,plan.leg_width,height,plan.leg_inset,plan.height);
    return PRO_TK_NO_ERROR;
}