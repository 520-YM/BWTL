static ProError mp_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    static const wchar_t *const labels[]={L"",L"多级台阶孔",L"交叉通道",L"孔背面沉台",L"批量孔口去毛刺倒角",L"带岛型腔",
        L"分层型腔",L"内角过切避让",L"孔内方位平面",L"轴肩退刀槽",L"螺钉头工具让位"};
    FdTxn tx={0};
    ShopSetup setup;
    PartText out={report,capacity,0};
    int k=mp_kind(cmd->arg[0]),bodies=0,started=0;
    ProError error;
    if(shop_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(pn_recovery_uncertain||am_recovery_uncertain){
        wcscpy_s(report,capacity,L"上次加工的恢复未能核实，本会话已停止对应写入。");
        return PRO_TK_BAD_CONTEXT;
    }
#define MP(call,label) do{tx.stage=label;error=(call);trace(label,error);if(error)goto done;}while(0)
    MP(fd_begin(&tx,model,cmd->arg[2],k==MP_DEBURR?PRO_FEAT_CHAMFER:PRO_FEAT_CUT),"prepare shop operation");
    MP(sd_body_count(model,&bodies),"read shop body count");
    if(bodies!=1||!tx.has_volume||tx.volume<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
    MP(part_health(model),"check model before shop operation");
    if(k!=MP_DEBURR&&k!=MP_GROOVE)MP(mp_setup(&tx,cmd,3,&setup),"verify machining face and coordinate system");
    started=1;
    switch(k){
    case MP_STEP:MP(mp_step(&tx,cmd,&setup,&out),"verify and create stepped bore");break;
    case MP_CROSS:MP(mp_cross(&tx,cmd,&setup,&out),"verify and create intersecting passages");break;
    case MP_BACK:MP(mp_back(&tx,cmd,&setup,&out),"verify and create rear spotface");break;
    case MP_DEBURR:MP(mp_deburr(&tx,cmd,&out),"verify and chamfer circular mouths");break;
    case MP_ISLAND:MP(mp_island(&tx,cmd,&setup,&out),"verify and create island pocket");break;
    case MP_LEVEL:MP(mp_level(&tx,cmd,&setup,&out),"verify and create stepped pocket");break;
    case MP_DOG:MP(mp_dog(&tx,cmd,&setup,&out),"verify and create dogbone reliefs");break;
    case MP_FLAT:MP(mp_flat(&tx,cmd,&setup,&out),"verify and create flat-sided bore relief");break;
    case MP_GROOVE:MP(mp_groove(&tx,cmd,&out),"verify and create radiused shaft groove");break;
    case MP_TOOL:MP(mp_tool(&tx,cmd,&setup,&out),"verify and create tool clearance");break;
    default:error=PRO_TK_BAD_INPUTS;goto done;
    }
    MP(part_health(model),"verify shop model regeneration");
    int after=0;
    MP(sd_body_count(model,&after),"verify shop operation preserved one solid body");
    if(after!=bodies){error=PRO_TK_BAD_CONTEXT;goto done;}
    part_text_add(&out,L"特征 %ls，未保存。",tx.name);
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
#undef MP
}
