static ProError fx_marker_verify(ProMdl model,const wchar_t *name,const PanelFrame *panel,
    const PanelProfile *marker,double scale){
    FdSketch sketch={0};
    ProMatrix frame;
    int *ids=NULL,count=0,circles=0;
    ProError error=fd_sketch_open(model,name,&sketch,0);
    if(!error)error=ProSectionLocationGet(sketch.section,frame);
    if(!error)error=ProSectionEntityIdsGet(sketch.section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        Pro2dEntdef *entity=NULL;
        error=ProSectionEntityGet(sketch.section,ids[i],&entity);
        if(error||entity->type!=PRO_2D_CIRCLE)continue;
        Pro2dCircledef *circle=(Pro2dCircledef *)entity;
        ProVector center,expected,delta;
        for(int j=0;j<3;j++)center[j]=frame[3][j]+circle->center[0]*frame[0][j]+circle->center[1]*frame[1][j];
        am_world(&panel->placement,marker->x*scale,marker->y*scale,0,expected);
        ad_sub(center,expected,delta);
        if(sqrt(ad_dot(delta,delta))>0.0001*scale||fabs(circle->radius-marker->width*scale/2)>0.0001*scale)
            error=PRO_TK_GENERAL_ERROR;
        circles++;
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&sketch);
    return error?error:circles==1?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

#ifdef BWTL_FASTENER_FAULT_TEST
static int fx_test_fail_after_cut;
static int fx_test_fault_hit;
#endif

static ProError fx_execute(const PartCommand *cmd,ProMdl model,int sheet,wchar_t *report,size_t capacity){
    static const wchar_t *const labels[]={L"",L"钥匙孔挂槽",L"长槽调节孔",L"四角安装孔",L"法兰分度孔组",
        L"铰链安装孔组",L"压铆螺母底孔",L"拉铆螺母底孔",L"接地端子安装位",L"扎带固定槽对",L"滑入式卡舌"};
    FdTxn tx={0};
    PanelFrame frame={0};
    FastenerPlan plan={0};
    ProName sketch,mask;
    ProMatrix placement,marker_frame;
    ProModelitem conflict;
    ProError error;
    int kind=fx_kind(cmd->arg[0]),bodies=0,mutated=0;
    double x=as_arg(cmd,6),y=as_arg(cmd,7),area=0,t=as_arg(cmd,8);
    if(fastener_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(pn_recovery_uncertain||(sheet&&sh_recovery_uncertain)){
        wcscpy_s(report,capacity,L"上次加工的恢复未能核实，本会话已停止对应写入。");
        return PRO_TK_BAD_CONTEXT;
    }
    switch(kind){
    case FX_KEY:fx_key(&plan,cmd,x,y);break;
    case FX_SLOT:fx_slot(&plan,cmd,x,y);break;
    case FX_CORNERS:fx_corners(&plan,cmd,x,y);break;
    case FX_PCD:fx_pcd(&plan,cmd,x,y);break;
    case FX_HINGE:fx_hinge(&plan,cmd,x,y);break;
    case FX_PRESS:fx_press(&plan,cmd,x,y);break;
    case FX_RIVET:fx_rivet(&plan,cmd,x,y);break;
    case FX_GROUND:fx_ground(&plan,cmd,x,y);break;
    case FX_TIE:fx_tie(&plan,cmd,x,y);break;
    case FX_TAB:fx_tab(&plan,cmd,x,y);break;
    }
    if((plan.max_thick&&(t<plan.min_thick||t>plan.max_thick))||
       (kind!=FX_TIE&&kind!=FX_TAB&&!pn_profiles_separate(&plan.panel))){
        wcscpy_s(report,capacity,L"安装与固定未执行。板厚超出输入规格范围，或孔位重叠。");
        return PRO_TK_BAD_INPUTS;
    }
#define FX(call,label) do{tx.stage=label;error=(call);trace(label,error);if(error)goto done;}while(0)
    FX(sheet?sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT):fd_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT),"prepare fastening operation");
    FX(sd_body_count(model,&bodies),"read fastening body count");
    if(bodies!=1||!tx.has_volume||tx.volume<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
    FX(pn_frame(&tx,cmd,&frame),"verify fastening face and coordinate system");
    if(plan.margin)FX(fx_check_boundary(&tx,cmd,&frame,&plan),"check physical edge and contour distances");
    for(int i=0;i<plan.panel.count;i++){
        FX(pn_profile_probes(&tx,&frame,&plan.panel.through[i],t,0),"check material and thickness at fastening profile");
        area+=pn_area(&plan.panel.through[i]);
    }
    if(kind==FX_TAB)FX(fx_tab_material(&tx,cmd,&frame,1),"check material at tongue and stops");
    if(plan.marker.width)FX(pn_profile_probes(&tx,&frame,&plan.marker,t,0),"check grounding marker region");
    swprintf_s(sketch,PRO_NAME_SIZE,L"%ls_SK",tx.name);
    swprintf_s(mask,PRO_NAME_SIZE,L"%ls_MASK",tx.name);
    if(!ProModelitemByNameInit(model,PRO_FEATURE,sketch,&conflict)||
       (plan.marker.width&&!ProModelitemByNameInit(model,PRO_FEATURE,mask,&conflict))){
        tx.stage="reserve fastening feature names";
        error=PRO_TK_E_FOUND;
        goto done;
    }
    mutated=1;
    FX(pn_sketch(model,cmd,&frame,sketch,plan.panel.through,plan.panel.count,sheet,placement),"create native fastening sketch");
    if(plan.marker.width)FX(pn_sketch(model,cmd,&frame,mask,&plan.marker,1,sheet,marker_frame),"create associative grounding marker");
    FX(pn_cut(&tx,&frame,tx.name,sketch,placement,t,sheet),"create native fastening through-cut");
#ifdef BWTL_FASTENER_FAULT_TEST
    if(fx_test_fail_after_cut){fx_test_fault_hit=1;error=PRO_TK_GENERAL_ERROR;goto done;}
#endif
    FX(md_expected_volume(&tx,-area*t),"verify fastening cut volume");
    for(int i=0;i<plan.panel.count;i++)
        FX(pn_profile_probes(&tx,&frame,&plan.panel.through[i],t,1),"verify each fastening opening");
    if(kind==FX_TAB)FX(fx_tab_material(&tx,cmd,&frame,0),"verify actual tongue and insertion stops remain");
    if(plan.marker.width)FX(fx_marker_verify(model,mask,&frame,&plan.marker,tx.scale),"verify actual ground marker circle");
    FX(part_health(model),"verify fastening regeneration");
    PartText out={report,capacity,0};
    part_text_add(&out,L"已创建%ls %ls，%d 个贯穿轮廓，已核对实际切口及去料体积。",labels[kind],tx.name,plan.panel.count);
    if(plan.margin&&kind!=FX_GROUND)part_text_add(&out,L"至现有面边界的最小净距%ls %.8g mm。",
        plan.panel.through[0].shape==PN_HEX?L"保守值":L"",plan.min_actual);
    if(kind==FX_HINGE)part_text_add(&out,L"至所选门边净距 %.8g mm。",plan.reference_clearance);
    if(plan.max_thick)part_text_add(&out,L"实测加工区板厚与输入 %.8g mm 一致，规格范围 %.8g–%.8g mm。",
        t,plan.min_thick,plan.max_thick);
    if(kind==FX_SLOT)part_text_add(&out,L"槽宽 %.8g mm，名义调节行程 %.8g mm。",plan.panel.through[0].height,as_arg(cmd,11));
    if(kind==FX_TIE)part_text_add(&out,L"中间留料 %.8g mm。",as_arg(cmd,11)-as_arg(cmd,10));
    if(kind==FX_TAB)part_text_add(&out,L"舌宽 %.8g mm，插入长 %.8g mm，按输入的配合槽计算单侧间隙 %.8g mm。两侧止挡已核对。",
        as_arg(cmd,9)-2*as_arg(cmd,10),as_arg(cmd,11),as_arg(cmd,10));
    if(plan.marker.width)part_text_add(&out,L"遮蔽外圈草绘 %ls，名义环形区域 %.8g mm²。",mask,pn_area(&plan.marker)-area);
    part_text_add(&out,L"未保存。");
done:
    if(error&&mutated){
        am_finish(&tx,error,bodies,report,capacity);
        if(!tx.rollback_ok)pn_recovery_uncertain=1;
    }else{
        if(error)swprintf_s(report,capacity,L"安装与固定未执行（%hs，Creo %d）。请核对实际板厚、边距、坐标系和特征名称。",tx.stage,error);
        fd_cleanup(&tx,0);
    }
    return error;
#undef FX
}
