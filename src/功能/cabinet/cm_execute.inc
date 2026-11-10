static ProError cm_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};
    PartText out={report,capacity,0};
    int k=cm_kind(cmd->arg[0]),bodies=0,started=0;
    ProError error;
    if(cabinet_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(cm_recovery_uncertain||sh_recovery_uncertain){
        wcscpy_s(report,capacity,L"上次钣金恢复未能核实，本次会话已停止对应写入。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(k==BWTL_CM_TRAY&&(error=cm_tray_clearance(cmd,&out)))return error;
    int edit=k==BWTL_CM_BENDS;
    int empty=k==BWTL_CM_TRAY||k==BWTL_CM_U;
    if(edit){
        tx.model=model;
        tx.rollback_ok=1;
        double mm;
        if((error=qa_mm_per_unit(model,&mm)))goto done;
        tx.scale=1/mm;
        ProMassProperty mass;
        if((error=qa_geometry_mass(model,&mass)))goto done;
        tx.volume=mass.volume;
        tx.has_volume=1;
    }else{
        ProName name;
        if(k==BWTL_CM_GAP||k==BWTL_CM_END)
            swprintf_s(name,PRO_NAME_SIZE,L"%.20ls_%ls",cmd->arg[2],k==BWTL_CM_GAP?L"GAP":L"END");
        else wcscpy_s(name,PRO_NAME_SIZE,cmd->arg[2]);
        if((error=sh_begin(&tx,model,name,PRO_FEAT_WALL)))goto done;
    }
    if((error=sd_body_count(model,&bodies)))goto done;
    if(empty?(bodies!=0||!tx.was_empty):(bodies!=1||!tx.has_volume||tx.volume<=0)){
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    started=1;
    switch(k){
    case BWTL_CM_SEGMENTS:error=cm_segments(&tx,cmd,&out);break;
    case BWTL_CM_TRAY:error=cm_tray(&tx,cmd,&out);break;
    case BWTL_CM_U:error=cm_u(&tx,cmd,&out);break;
    case BWTL_CM_Z:error=cm_z(&tx,cmd,&out);break;
    case BWTL_CM_GAP:error=cm_gap(&tx,cmd,&out);break;
    case BWTL_CM_END:error=cm_end(&tx,cmd,&out);break;
    case BWTL_CM_RETURN:error=cm_return(&tx,cmd,&out);break;
    case BWTL_CM_EAR:error=cm_ear(&tx,cmd,&out);break;
    case BWTL_CM_COLLAR:error=cm_collar(&tx,cmd,&out);break;
    case BWTL_CM_BENDS:error=cm_bends(&tx,cmd,&out);break;
    default:error=PRO_TK_BAD_INPUTS;break;
    }
    if(!error&&!edit){
        int final_bodies;
        error=sd_body_count(model,&final_bodies);
        if(!error&&final_bodies!=1)error=PRO_TK_GENERAL_ERROR;
        if(!error)error=part_health(model);
    }
done:
    if(error){
        trace("cabinet operation error",error);
        if(!edit&&started){
            tx.stage="cabinet operation";
            am_finish(&tx,error,bodies,report,capacity);
            if(!tx.rollback_ok)cm_recovery_uncertain=1;
        }else fd_cleanup(&tx,0);
        if(!report[0]||cm_recovery_uncertain)swprintf_s(report,capacity,L"钣金与机箱操作未完成（Creo %d）。%ls。未保存。",error,
            cm_recovery_uncertain?L"恢复未能核实，本会话已停止对应写入":L"已核对原有特征状态，请检查板边、尺寸和依赖");
    }else{
        fd_cleanup(&tx,0);
        part_text_add(&out,L"已核对重生成，未保存。");
    }
    return error;
}
