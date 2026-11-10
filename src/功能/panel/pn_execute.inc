static int pn_recovery_uncertain;
#ifdef BWTL_PANEL_FAULT_TEST
static int pn_test_fail_after_through;
#endif

static ProError pn_execute(const PartCommand *cmd,ProMdl model,int sheet,wchar_t *report,size_t capacity){
    static const wchar_t *const labels[]={L"",L"按钮开孔",L"急停按钮开孔",L"电源插座开孔",L"接口开孔",
        L"风扇安装孔组",L"显示屏窗口",L"透明面板沉位",L"把手安装孔组",L"锁具安装孔",L"走线梳齿口"};
    FdTxn tx={0};
    PanelFrame frame={0};
    PanelPlan plan={0};
    ProMatrix through_frame,seat_frame;
    ProName through_sketch,seat_sketch,seat_feature;
    ProModelitem collision;
    ProError error;
    int kind=pn_kind(cmd->arg[0]),bodies=0,mutated=0;
    double x=as_arg(cmd,6),y=as_arg(cmd,7),size[5]={0},area=0;
    if(panel_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(pn_recovery_uncertain||(sheet&&sh_recovery_uncertain)){
        wcscpy_s(report,capacity,L"上次操作的恢复未能核实，本会话已停止对应写入。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(sheet&&kind==PN_SEAT){
        wcscpy_s(report,capacity,L"定深沉位用于普通实体板件。此钣金零件可使用通孔类面板功能。");
        return PRO_TK_BAD_CONTEXT;
    }
    for(int i=9;i<cmd->count;i++)size[i-9]=as_arg(cmd,i);
    plan.thickness=as_arg(cmd,8);
    switch(kind){
    case PN_BUTTON:pn_button(&plan,x,y,size[0],size[1],size[2]);break;
    case PN_ESTOP:pn_estop(&plan,x,y,size);break;
    case PN_POWER:pn_power(&plan,x,y,size);break;
    case PN_PORT:pn_port(&plan,x,y,size);break;
    case PN_FAN:pn_fan(&plan,x,y,size);break;
    case PN_DISPLAY:pn_display(&plan,x,y,size);break;
    case PN_SEAT:pn_seat(&plan,x,y,size);break;
    case PN_HANDLE:pn_handle(&plan,x,y,size);break;
    case PN_LOCK:pn_lock(&plan,x,y,size);break;
    case PN_COMB:pn_comb(&plan,x,y,size);break;
    }
    if(!pn_profiles_separate(&plan)){
        wcscpy_s(report,capacity,L"主窗口与固定孔或槽的轮廓相交或间隔不足，请核对孔径与孔距。");
        return PRO_TK_BAD_INPUTS;
    }
#define PN(call,label) do{tx.stage=label;error=(call);trace(label,error);if(error)goto done;}while(0)
    PN(sheet?sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT):fd_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT),"prepare panel operation");
    PN(sd_body_count(model,&bodies),"read panel solid body");
    if(bodies!=1||!tx.has_volume||tx.volume<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
    PN(pn_frame(&tx,cmd,&frame),"verify planar face and outward coordinate system");
    for(int i=0;i<plan.count;i++){
        PN(pn_profile_probes(&tx,&frame,&plan.through[i],plan.thickness,0),"verify panel thickness and intact opening locations");
        area+=pn_area(&plan.through[i]);
    }
    if(plan.seat_depth)PN(pn_seat_probes(&tx,&frame,&plan,0),"verify intact seat shoulder");
    if(kind==PN_DISPLAY)PN(pn_display_support(&tx,&frame,&plan),"verify display bezel fits selected face");
    swprintf_s(through_sketch,PRO_NAME_SIZE,L"%ls_SK",tx.name);
    swprintf_s(seat_sketch,PRO_NAME_SIZE,L"%ls_SSK",tx.name);
    swprintf_s(seat_feature,PRO_NAME_SIZE,L"%ls_SEAT",tx.name);
    if(!ProModelitemByNameInit(model,PRO_FEATURE,through_sketch,&collision)||
       (plan.seat_depth&&(!ProModelitemByNameInit(model,PRO_FEATURE,seat_sketch,&collision)||
                          !ProModelitemByNameInit(model,PRO_FEATURE,seat_feature,&collision)))){
        tx.stage="reserve unused sketch and seat names";
        error=PRO_TK_E_FOUND;
        goto done;
    }
    mutated=1;
    PN(pn_sketch(model,cmd,&frame,through_sketch,plan.through,plan.count,sheet,through_frame),"build associative opening sketch");
    if(plan.seat_depth)
        PN(pn_sketch(model,cmd,&frame,seat_sketch,&plan.seat,1,0,seat_frame),"build associative seat sketch");
    PN(pn_cut(&tx,&frame,tx.name,through_sketch,through_frame,plan.thickness,sheet),"cut all openings on the panel");
    PN(md_expected_volume(&tx,-area*plan.thickness),"verify actual removed opening volume");
    for(int i=0;i<plan.count;i++)
        PN(pn_profile_probes(&tx,&frame,&plan.through[i],plan.thickness,1),"verify each opening is through");
    if(plan.seat_depth){
#ifdef BWTL_PANEL_FAULT_TEST
        if(pn_test_fail_after_through){error=PRO_TK_GENERAL_ERROR;goto done;}
#endif
        PN(pn_cut(&tx,&frame,seat_feature,seat_sketch,seat_frame,plan.seat_depth,0),"cut depth-limited transparent panel seat");
        PN(md_expected_volume(&tx,-area*plan.thickness-(pn_area(&plan.seat)-area)*plan.seat_depth),
            "verify opening and seat volume");
        PN(pn_seat_probes(&tx,&frame,&plan,1),"verify actual shoulder depth and remaining material");
    }
    PN(part_health(model),"verify regenerated panel");
    PartText out={report,capacity,0};
    part_text_add(&out,L"已创建%ls %ls，%d 个贯穿轮廓，板厚 %.8g mm，已核对位置和去料体积。",
        labels[kind],tx.name,plan.count,plan.thickness);
    if(plan.seat_depth)part_text_add(&out,L"沉位深 %.8g mm，台阶剩余厚度 %.8g mm。",plan.seat_depth,plan.thickness-plan.seat_depth);
    if(kind==PN_DISPLAY)part_text_add(&out,L"对中时单侧遮边 X %.8g mm，Y %.8g mm。",plan.bezel_x,plan.bezel_y);
    part_text_add(&out,L"未保存。");
done:
    if(error&&mutated){
        am_finish(&tx,error,bodies,report,capacity);
        if(!tx.rollback_ok)pn_recovery_uncertain=1;
    }else{
        if(error)swprintf_s(report,capacity,L"面板操作未执行（%hs，Creo %d）。请核对选面、局部板厚、坐标方向、空腔和特征名称。",
            tx.stage,error);
        fd_cleanup(&tx,0);
    }
    return error;
#undef PN
}
