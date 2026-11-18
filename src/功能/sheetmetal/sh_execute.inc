static ProMdl sh_cut_rolled_back_model;
static ProError sh_cut_rolled_back_error;

static ProError sh_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    int k=sh_kind(cmd->arg[0]),bodies=0;FdTxn tx={0};ProError e;ProMdlsubtype subtype,original_subtype;ProMassProperty mass;
    ProFeattype type=k==SH_SKETCH?PRO_FEAT_CURVE:k==SH_FLANGE||k==SH_HEM?PRO_FEAT_WALL:k==SH_BEND?PRO_FEAT_BEND:k==SH_UNBEND?PRO_FEAT_UNBEND:k==SH_REBEND?PRO_FEAT_BEND_BACK:k==SH_RELIEF?PRO_FEAT_EDIT_BEND_RELIEF:k==SH_CUT?PRO_FEAT_CUT:k==SH_FORM?PRO_FEAT_SMT_SKETCH_FORM:PRO_FEAT_DATUM_SURF;
    if(sheetmetal_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(k==SH_INFO)return sh_info(model,report,capacity);
    if(sh_recovery_uncertain)return PRO_TK_BAD_CONTEXT;if((e=ProMdlSubtypeGet(model,&subtype)))return e;
    original_subtype=subtype;
    wchar_t verification[950]=L"";
    PartText diagnosis={verification,_countof(verification),0};
    if(k==SH_CONVERT&&subtype==PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_BAD_CONTEXT;
    if(k!=SH_CONVERT&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_BAD_CONTEXT;
    if(k==SH_ALLOWANCE)return sh_allowance(cmd,model,report,capacity);
    if(k==SH_RELIEF&&!wcscmp(cmd->arg[3],L"ALL"))return sh_relief_update(cmd,model,report,capacity);
    if(k==SH_DXF)return sh_export(cmd,model,report,capacity);
#define SH(call,label) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    SH(sh_begin(&tx,model,cmd->arg[2],type),"prepare native sheetmetal transaction");SH(sd_body_count(model,&bodies),"read sheetmetal bodies");
    if(k==SH_FIRST){if(!tx.was_empty){e=PRO_TK_BAD_CONTEXT;goto done;}}
    else if(k!=SH_SKETCH&&(bodies!=1||!tx.has_volume||tx.volume<=0)){e=PRO_TK_BAD_CONTEXT;goto done;}
    switch(k){
    case SH_SKETCH:SH(sh_sketch(&tx,cmd),"create sheetmetal associative sketch");break;
    case SH_CONVERT:SH(sh_convert(&tx,cmd),"convert native sheetmetal driving surface");break;
    case SH_FIRST:SH(sh_first(&tx,cmd),"create native first planar wall");break;
    case SH_FLANGE:SH(sh_flange(&tx,cmd),"create attached native flat flange");break;
    case SH_HEM:SH(sh_hem(&tx,cmd),"create native hem wall");break;
    case SH_BEND:SH(sh_bend(&tx,cmd),"create native sheetmetal bend");break;
    case SH_UNBEND:case SH_REBEND:SH(sh_flatten(&tx,cmd->arg[3],k==SH_REBEND),"create native unbend or bend back");break;
    case SH_CUT:SH(sh_cut(&tx,cmd),"create native sheetmetal normal cut");break;
    case SH_FORM:SH(sh_form(&tx,cmd),"create native sketch punch form");break;
    default:e=PRO_TK_NOT_IMPLEMENTED;goto done;
    }
    SH(part_regenerate(model),"regenerate sheetmetal feature");SH(part_health(model),"verify healthy sheetmetal feature");SH(ProMdlSubtypeGet(model,&subtype),"verify actual sheetmetal subtype");
    if(subtype!=PROMDLSTYPE_PART_SHEETMETAL){e=PRO_TK_BAD_CONTEXT;goto done;}
    if(k!=SH_SKETCH){SH(ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass),"verify actual sheetmetal material");if(mass.volume<=0){e=PRO_TK_GENERAL_ERROR;goto done;}}
    tx.stage="verify native sheetmetal type, parents, thickness and dimensions";
    e=sh_verify_detail(&tx,cmd,k,&diagnosis);
    trace(tx.stage,e);
    if(e)goto done;
    if(k==SH_SKETCH)
        swprintf_s(report,capacity,L"已创建关联草绘 %ls，参照和重生成已核对。创建首个板壁后才有钣金实体。未保存。",tx.name);
    else swprintf_s(report,capacity,L"钣金特征 %ls 已创建，原生钣金类型、重生成和实体检查通过，未保存。",tx.name);
done:
    if(e&&tx.errors.error_list)for(int i=0;i<tx.errors.error_number&&i<32;i++){trace("sheetmetal element id",tx.errors.error_list[i].err_item_id);trace("sheetmetal element error",tx.errors.error_list[i].error);}
    int cut_rollback_tested=e&&k==SH_CUT&&tx.root&&tx.before_count&&bodies==1&&tx.has_volume;
    am_finish(&tx,e,bodies,report,capacity);
    if(e&&!verification[0]&&k==SH_CUT){
        const wchar_t *cause=!strcmp(tx.stage,"create native sheetmetal normal cut")?
            L"Creo 没能从指定草绘生成法向贯穿切口":
            !strcmp(tx.stage,"regenerate sheetmetal feature")?L"切口生成后，Creo 重生成失败":
            !strcmp(tx.stage,"verify healthy sheetmetal feature")?L"切口已重生成，模型仍有异常特征":
            L"切口未通过当前阶段的原生读取或建模前检查";
        swprintf_s(verification,_countof(verification),L"%ls（草绘 %ls，方向 %ls）",cause,cmd->arg[3]+7,cmd->arg[4]);
    }
    if(e&&(ProMdlSubtypeGet(model,&subtype)||subtype!=original_subtype))tx.rollback_ok=0;
    if(cut_rollback_tested&&tx.rollback_ok){
        sh_cut_rolled_back_model=model;
        sh_cut_rolled_back_error=e;
    }
    if(e&&!tx.rollback_ok){sh_recovery_uncertain=1;wcscpy_s(report,capacity,L"本次钣金操作失败，恢复状态未能完整核实。请检查当前模型；该独立会话已停止后续钣金写入。");}
    if(e&&verification[0]){
        PartText explained={report,capacity,0};
        part_text_add(&explained,L"\n失败原因：%ls。",verification);
    }
    return e;
#undef SH
}
