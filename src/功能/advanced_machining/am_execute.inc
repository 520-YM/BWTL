static int am_recovery_uncertain=0;
static void am_finish(FdTxn *tx,ProError error,int before_bodies,wchar_t *report,size_t capacity){
    if(error&&tx->root&&tx->before_count){
        FdTxn after={0};ProMassProperty mass;ProError e;int bodies=0;
        e=ProSolidFeatVisit((ProSolid)tx->model,fd_feature_ids,NULL,&after);trace("advanced rollback feature scan",e);
        if(e)tx->rollback_ok=0;
        else for(int i=after.before_count-1;i>=0;i--)if(!fd_was_feature(tx,after.before_ids[i])){
            FdTxn current={0};
            e=ProSolidFeatVisit((ProSolid)tx->model,fd_feature_ids,NULL,&current);
            if(e){tx->rollback_ok=0;continue;}
            /* Removing a hole can also remove its generated helper feature. */
            if(!fd_was_feature(&current,after.before_ids[i]))continue;
            e=model_remove_feature((ProSolid)tx->model,after.before_ids[i]);trace("advanced rollback remove owned feature",e);if(e)tx->rollback_ok=0;
        }
        e=part_regenerate(tx->model);trace("advanced rollback regenerate",e);if(e)tx->rollback_ok=0;
        e=sd_body_count(tx->model,&bodies);trace("advanced rollback body count",e);
        if(e||bodies!=before_bodies)tx->rollback_ok=0;
        if(before_bodies){
            e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);trace("advanced rollback mass",e);
            if(e||!part_close(mass.volume,tx->volume))tx->rollback_ok=0;
        }
    }
    tx->created=0;fd_cleanup(tx,error);
    if(error)swprintf_s(report,capacity,L"高级加工未完成：%hs，错误码%d。%ls；未保存。",tx->stage,error,
        tx->rollback_ok?L"未新增或已核对移除本次特征，原实体保持":L"恢复未能核实，请检查模型");
}
static ProError am_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};AmHole hole={0};McProfiledHole thread={0};int k=am_kind(cmd->arg[0]),bodies=0;
    double volume=0;ProError e;ProFeattype type=k<=AM_SPOTFACE?PRO_FEAT_HOLE:k==AM_CHAMFER?PRO_FEAT_CHAMFER:k>=AM_VARIABLE?PRO_FEAT_ROUND:PRO_FEAT_CUT;
    const wchar_t *labels[]={L"",L"曲面入口孔",L"指定轴线斜孔",L"贯穿螺纹孔",L"非 ISO 标准螺纹孔",L"孔口点平",L"键槽",L"卡簧槽",L"变半径圆角",L"相切边链圆角",L"不等距／角度倒角"};
    if(advanced_machining_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(k==AM_INFO)return am_info(model,report,capacity);
    if(am_recovery_uncertain){wcscpy_s(report,capacity,L"上次高级加工的恢复未能核实，请检查模型后重新启动此独立 BWTL 会话。");return PRO_TK_BAD_CONTEXT;}
#define AM(call,label) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    AM(fd_begin(&tx,model,cmd->arg[2],type),"prepare advanced machining");
    AM(sd_body_count(model,&bodies),"read advanced machining body count");
    if(bodies!=1||!tx.has_volume||tx.volume<=0){e=PRO_TK_BAD_CONTEXT;goto done;}
    switch(k){
    case AM_RADIAL:AM(am_radial(&tx,cmd,&hole),"create radial surface entry hole");break;
    case AM_AXIS:AM(am_axis(&tx,cmd,&hole,0),"create point and axis oriented hole");break;
    case AM_SPOTFACE:AM(am_spotface(&tx,cmd,&hole),"create flat bottom spotface");break;
    case AM_THREAD_THRU:AM(am_thread_thru(&tx,cmd,&thread),"create native through threaded hole");break;
    case AM_THREAD_STD:AM(am_thread_std(&tx,cmd,&thread),"create native UNC or UNF threaded hole");break;
    case AM_KEYWAY:case AM_RING:
        AM(k==AM_KEYWAY?am_keyway(&tx,cmd):am_ring(&tx,cmd),"create associated keyway or retaining ring groove");
        AM(ProModelitemByNameInit(model,PRO_FEATURE,tx.name,(ProModelitem *)&tx.feature),"find actual groove cut feature");tx.created=1;break;
    case AM_VARIABLE:AM(am_variable(&tx,cmd),"create native variable radius round");break;
    case AM_TANGENT:AM(am_tangent(&tx,cmd),"create native tangent chain round");break;
    case AM_CHAMFER:AM(am_chamfer(&tx,cmd),"create native unequal or angle chamfer");break;
    default:e=PRO_TK_BAD_INPUTS;goto done;
    }
    AM(mc_verify(&tx,type,&volume),"verify machining state, references and material");
    AM(part_health(model),"verify regenerated advanced machining model");
    if(k<=AM_AXIS||k==AM_SPOTFACE)AM(am_verify_hole(&tx,&hole),"verify actual hole cylinder and axis");
    if(k<=AM_AXIS||k==AM_SPOTFACE)AM(am_verify_floor(&tx,cmd,&hole,k),"verify full circular flat floor and axial depth");
    if(k==AM_THREAD_THRU||k==AM_THREAD_STD)AM(am_verify_thread(&tx,cmd,&thread,volume),"verify native thread definition and drill volume");
    if(k>=AM_VARIABLE)AM(am_verify_edge(&tx,cmd,k),"verify actual native radii or chamfer dimensions");
    swprintf_s(report,capacity,L"已创建%ls %ls。已核对原生特征、关联参照、单实体和重生成；体积 %.9g → %.9g mm³，未保存。",labels[k],cmd->arg[2],tx.volume/pow(tx.scale,3),volume/pow(tx.scale,3));
done:
    if(e&&tx.errors.error_list)for(int i=0;i<tx.errors.error_number&&i<32;i++){trace("advanced machining element",tx.errors.error_list[i].err_item_id);trace("advanced machining element error",tx.errors.error_list[i].error);}
    am_finish(&tx,e,bodies,report,capacity);
    if(e&&!tx.rollback_ok)am_recovery_uncertain=1;
    return e;
#undef AM
}
