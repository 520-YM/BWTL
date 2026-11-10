static int backrest_recovery_uncertain;
static ProError quick_backrest_build(ProMdl model,double width,double thick,double height,wchar_t *report,size_t capacity){
    if(backrest_recovery_uncertain){
        wcscpy_s(report,capacity,L"上次靠背操作的恢复未能核实，当前会话已停止对应写入。");
        return PRO_TK_BAD_CONTEXT;
    }
    const wchar_t *feature=L"BWTL_STOOL_BACK";
    const wchar_t *sketch=L"BWTL_STOOL_BACK_SK";
    ProSolidBody body;
    ProOutline outline;
    ProFeature legs;
    FdTxn tx={0};
    PartCommand setup={0};
    PanelFrame frame={0};
    ProMatrix sketch_matrix;
    int modified=0;
    ProError error=ux_named_feature(model,L"QP_STOOL_LEGS",&legs);
    if(error){
        wcscpy_s(report,capacity,L"活动零件缺少已验证的四腿凳结构，无法套用直板靠背，请核对模型。");
        return error;
    }
    error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodyOutlineGet(&body,outline);
    if(!error)error=fd_begin(&tx,model,feature,PRO_FEAT_PROTRUSION);
    if(error)goto done;
    double x0=outline[0][0]/tx.scale,x1=outline[1][0]/tx.scale;
    double y0=outline[0][1]/tx.scale,y1=outline[1][1]/tx.scale;
    double top=outline[1][2]/tx.scale;
    tx.stage="validate backrest fits the seat";
    if(width>x1-x0||thick>=y1-y0||top<=0||outline[0][2]>=0||!tx.has_volume){
        error=PRO_TK_BAD_CONTEXT;
        goto done;
    }
    PanelProfile plate=pn_rect((x0+x1)/2,y1-thick/2,width,thick);
    if((error=quick_frame(&tx,top,&setup,&frame))||
       (error=mp_rect_samples(&tx,&frame,&plate,0,0.1,1)))goto done;
    ProFeature collision;
    if(!ux_named_feature(model,sketch,&collision)){error=PRO_TK_E_FOUND;goto done;}
    tx.stage="build associated backrest sketch and solid";
    modified=1;
    error=pn_sketch(model,&setup,&frame,sketch,&plate,1,0,sketch_matrix);
    if(!error)error=md_extrude(&tx,feature,sketch,sketch_matrix,frame.placement.csys.z_vector,height,0);
    if(error)goto done;
    tx.stage="verify seat connection and backrest dimensions";
    if((error=md_expected_volume(&tx,width*thick*height))||
       (error=qa_single_body(model,&body))||
       (error=part_health(model)))goto done;
    PanelFrame raised=vt_raised_frame(&frame,height+0.5,tx.scale);
    error=mp_rect_samples(&tx,&raised,&plate,0.5,height+0.6,0);
    if(!error)swprintf_s(report,capacity,L"已在当前凳子后侧添加一体靠背，宽 %.8g mm、厚 %.8g mm、高出凳面 %.8g mm。已核对连接与新增体积，未保存。",width,thick,height);
done:
    if(error&&modified){
        am_finish(&tx,error,1,report,capacity);
        if(!tx.rollback_ok)backrest_recovery_uncertain=1;
    }else{
        if(error)swprintf_s(report,capacity,L"靠背未添加。请检查凳面参照、尺寸和是否已有靠背（Creo %d）。",error);
        fd_cleanup(&tx,0);
    }
    return error;
}

static ProError quick_backrest(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
    return quick_backrest_build(model,as_arg(cmd,2),as_arg(cmd,3),as_arg(cmd,4),report,capacity);
}
