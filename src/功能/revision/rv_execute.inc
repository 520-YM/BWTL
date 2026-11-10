static ProError rv_execute(const PartCommand *cmd,ProMdl model,int sheet,wchar_t *report,size_t capacity){
    static const wchar_t *const labels[]={L"",L"按最终外形改长宽高",L"孔组整体平移",L"孔组换孔距",L"开口换规格",
        L"同规格孔批量改径",L"盲孔批量改深",L"沉孔批量换规格",L"圆角半径批量统一",L"钣金换板厚",L"一键生成改型副本"};
    int kind=rv_kind(cmd->arg[0]);
    Revision tx;
    PartText out={report,capacity,0};
    if(revision_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(rv_recovery_uncertain){
        wcscpy_s(report,capacity,L"上次改规格恢复未能核实，本会话已停止此类写入。");
        return PRO_TK_BAD_CONTEXT;
    }
    if((sheet&&kind!=RV_THICK&&kind!=RV_COPY&&kind!=RV_SIZE)||(!sheet&&kind==RV_THICK)){
        wcscpy_s(report,capacity,L"请核对零件类型。换板厚适用于钣金，孔和圆角改规格适用于普通零件。");
        return PRO_TK_BAD_CONTEXT;
    }
    ProError error=rv_begin(&tx,model);
    if(!error)switch(kind){
    case RV_SIZE:
        error=rv_outline(&tx,cmd,2);
        if(!error)part_text_add(&out,L"实体实测长宽高已达到 %.8g × %.8g × %.8g mm（模型默认 XYZ）。",
            as_arg(cmd,5),as_arg(cmd,6),as_arg(cmd,7));
        break;
    case RV_SHIFT:error=rv_shift(&tx,cmd,&out);break;
    case RV_PITCH:error=rv_pitch(&tx,cmd,&out);break;
    case RV_OPEN:error=rv_open(&tx,cmd,&out);break;
    case RV_DIAM:error=rv_diam(&tx,cmd,&out);break;
    case RV_DEPTH:error=rv_depth(&tx,cmd,&out);break;
    case RV_RECESS:error=rv_recess(&tx,cmd,&out);break;
    case RV_ROUND:error=rv_round(&tx,cmd,&out);break;
    case RV_THICK:error=rv_thick(&tx,cmd,&out);break;
    case RV_COPY:error=rv_copy(&tx,cmd,&out);break;
    default:error=PRO_TK_BAD_INPUTS;break;
    }
    trace("revision operation result",error);
    if(!error&&kind!=RV_COPY){
        ProOutline bounds;
        ProMassProperty mass;
        error=rv_geometry(model,bounds,&mass);
        if(error)report[0]=0;
    }
    if(error){
        if(tx.applied){
            int ok=rv_restore(&tx);
            part_text_add(&out,L"%ls未完成（Creo %d）。%ls。",labels[kind],error,
                ok?L"已恢复并核对整组原尺寸与实体状态":L"恢复未能核实，请检查当前模型");
        }else if(!report[0])part_text_add(&out,L"%ls未完成（Creo %d）。请核对当前实际尺寸、特征类型和驱动参照。",labels[kind],error);
    }
    part_text_add(&out,L"未保存。");
    return error;
}
