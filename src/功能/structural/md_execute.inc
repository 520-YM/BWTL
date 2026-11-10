static int md_recovery_uncertain=0;
static ProError md_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};int bodies=0,k=md_kind(cmd->arg[0]);ProError e;double volume;
    ProFeattype type=k<=MD_SHELL_LOCAL?PRO_FEAT_SHELL:k<=MD_DRAFT_VARIABLE?PRO_FEAT_DRAFT:k>=MD_WINDOW?PRO_FEAT_CUT:PRO_FEAT_PROTRUSION;
    const wchar_t *labels[]={L"",L"多面开口抽壳",L"局部变壁厚抽壳",L"多面同步拔模",L"分割拔模",L"可变角拔模",L"带拔模轨迹筋",L"螺钉柱",L"卡扣",L"配合窗口",L"端面密封圈槽",L"径向密封圈槽",L"毛坯加工余量"};
    if(structural_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(md_recovery_uncertain){wcscpy_s(report,capacity,L"上次结构特征的恢复未能核实，请检查模型后重新启动此独立 BWTL 会话。");return PRO_TK_BAD_CONTEXT;}
    if(k==MD_STOCK)return md_stock(cmd,model,report,capacity);
#define MD(call,label) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    MD(fd_begin(&tx,model,cmd->arg[2],type),"prepare structural feature");
    MD(sd_body_count(model,&bodies),"read structural body count");
    if(bodies!=1||!tx.has_volume||tx.volume<=0){e=PRO_TK_BAD_CONTEXT;goto done;}
    switch(k){
    case MD_SHELL_MULTI:MD(md_shell_multi(&tx,cmd),"create multiple opening shell");break;
    case MD_SHELL_LOCAL:MD(md_shell_local(&tx,cmd),"create local thickness shell");break;
    case MD_DRAFT_MULTI:MD(md_draft_multi(&tx,cmd),"create multiple surface draft");break;
    case MD_DRAFT_SPLIT:MD(md_draft_split(&tx,cmd),"create split neutral draft");break;
    case MD_DRAFT_VARIABLE:MD(md_draft_variable(&tx,cmd),"create variable angle draft");break;
    case MD_RIB:MD(md_rib(&tx,cmd),"create tapered path rib");break;
    case MD_BOSS:MD(md_boss(&tx,cmd),"create screw boss and linked bore");break;
    case MD_SNAP:MD(md_snap(&tx,cmd),"create cantilever snap hook");break;
    case MD_WINDOW:MD(md_window(&tx,cmd),"create snap mating window");break;
    case MD_SEAL_FACE:MD(md_seal_face(&tx,cmd),"create face seal groove");break;
    case MD_SEAL_RADIAL:MD(md_seal_radial(&tx,cmd),"create radial seal groove");break;
    default:e=PRO_TK_BAD_INPUTS;goto done;
    }
    if(k>=MD_BOSS){MD(ProModelitemByNameInit(model,PRO_FEATURE,tx.name,(ProModelitem *)&tx.feature),"find actual structural material feature");tx.created=1;}
    MD(mc_verify(&tx,type,&volume),"verify structural feature, references and material");
    MD(part_health(model),"verify structural regeneration");
    if(k<=MD_SHELL_LOCAL)MD(md_verify_shell(&tx,cmd,k==MD_SHELL_LOCAL),"verify all shell openings and local thicknesses");
    if(k>=MD_DRAFT_MULTI&&k<=MD_DRAFT_VARIABLE)MD(md_verify_draft(&tx,cmd,k),"verify native draft sides, angles and surface references");
    swprintf_s(report,capacity,L"已创建%ls %ls。原生特征、关联参照、单实体和重生成核对通过；体积 %.9g → %.9g mm³，未保存。",labels[k],cmd->arg[2],tx.volume/pow(tx.scale,3),volume/pow(tx.scale,3));
done:
    if(e&&tx.errors.error_list)for(int i=0;i<tx.errors.error_number&&i<32;i++){trace("structural element id",tx.errors.error_list[i].err_item_id);trace("structural element error",tx.errors.error_list[i].error);}
    am_finish(&tx,e,bodies,report,capacity);if(e&&!tx.rollback_ok)md_recovery_uncertain=1;return e;
#undef MD
}
