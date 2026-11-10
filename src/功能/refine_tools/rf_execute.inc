static ProError rf_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};
    const wchar_t *verb=cmd->arg[0],*label;
    const int draft=!wcscmp(verb,L"SOLID_DRAFT"),rib=!wcscmp(verb,L"SOLID_RIB"),mirror=!wcscmp(verb,L"FEATURE_MIRROR"),
        circular=!wcscmp(verb,L"FEATURE_PATTERN_CIRCULAR");
    ProFeattype type=draft?PRO_FEAT_DRAFT:rib?PRO_FEAT_RIB:PRO_FEAT_GROUP_HEAD;
    ProError e;
    int bodies=0,i,mirror_adds=0;
    double volume=0;
    if(mc_recovery_uncertain){wcscpy_s(report,capacity,L"先前操作恢复未能核实，本次会话已停止加工写入。请检查零件后重新启动独立 BWTL。");return PRO_TK_BAD_CONTEXT;}
    if(!wcscmp(verb,L"FEATURE_DELETE"))return rf_delete(cmd,model,report,capacity);
    if((e=fd_begin(&tx,model,cmd->arg[2],type)))goto done;
    tx.stage="verify existing healthy single solid body";
    if((e=sd_body_count(model,&bodies)))goto done;
    if(bodies!=1||!tx.has_volume||!isfinite(tx.volume)||tx.volume<=0){e=PRO_TK_BAD_CONTEXT;goto done;}
    if(draft){e=rf_draft(&tx,cmd);label=L"定角拔模";}
    else if(rib){e=rf_rib(&tx,cmd);label=L"轮廓筋";}
    else if(mirror){e=rf_mirror(&tx,cmd,&mirror_adds);label=L"特征几何镜像";}
    else {e=rf_pattern(&tx,cmd,circular,&volume);label=circular?L"圆周阵列":L"线性阵列";}
    if(e)goto done;
    if(draft||rib||mirror){
        if((e=mc_verify(&tx,type,&volume)))goto done;
        if((rib&&volume<=tx.volume)||(mirror&&(mirror_adds?volume<=tx.volume:volume>=tx.volume))){e=PRO_TK_BAD_CONTEXT;goto done;}
    }
    swprintf_s(report,capacity,L"已创建%ls %ls（特征 ID %d）。已通过原生重生成、单实体和材料变化检查，体积 %.9g → %.9g mm³。未保存。",
        label,cmd->arg[2],tx.feature.id,tx.volume/pow(tx.scale,3),volume/pow(tx.scale,3));
done:
    if(e){
        trace(tx.stage,e);
        if(tx.errors.error_list)for(i=0;i<tx.errors.error_number&&i<32;i++){
            trace("refinement element id",tx.errors.error_list[i].err_item_id);
            trace("refinement element error",tx.errors.error_list[i].error);
        }
    }
    mc_cleanup(&tx,e,bodies);
    if(e){
        if(!tx.rollback_ok)mc_recovery_uncertain=1;
        swprintf_s(report,capacity,L"修形未完成：%hs，错误码%d。%ls；未保存。",tx.stage,e,
            tx.rollback_ok?L"已核对原特征和体积保持":L"恢复未能核实，本次会话已停止加工写入");
    }
    return e;
}
