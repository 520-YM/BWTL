#ifdef BWTL_QUICK_FAULT_TEST
static int quick_fault_after_stock;
#endif
static ProError quick_execute(const PartCommand *cmd,const PartSnapshot *snapshot,wchar_t *text,size_t capacity){
    QuickPlan plan;
    wchar_t backrest_report[600]=L"";
    int kind=quick_kind(cmd->arg[0]);
    if(quick_plan(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg,&plan)!=1)return PRO_TK_BAD_INPUTS;
    ProPath directory;
    ProError error=ProDirectoryCurrentGet(directory);
    if(error||!snapshot->directory[0]||_wcsicmp(snapshot->directory,directory)){
        wcscpy_s(text,capacity,L"工作目录已变化或不可用。请确认目录后重新发送。");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    if(!part_target_available(NULL,cmd->arg[1],L"prt")){
        swprintf_s(text,capacity,L"零件名 %ls 已在会话或工作目录中使用。请换一个新名称。",cmd->arg[1]);
        return PRO_TK_E_FOUND;
    }
    if(dv_recovery_uncertain){
        wcscpy_s(text,capacity,L"工作目录恢复状态需要核对，请在新会话中生成零件。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(plan.walls&&(cm_recovery_uncertain||sh_recovery_uncertain)){
        wcscpy_s(text,capacity,L"钣金恢复状态需要核对，请在新会话中创建此件。");
        return PRO_TK_BAD_CONTEXT;
    }
    const char *stage="quick seed and datums";

    FdTxn tx={0};
    ProMdlName name;
    wcscpy_s(name,_countof(name),cmd->arg[1]);
    PartText note={text,capacity,0};
    error=quick_seed(name,&tx.model);
    if(!error){
        ModelPlan stock={kind==Q_SPACER,plan.width,plan.depth,plan.thickness,plan.bore,1};
        error=model_build_internal(&stock,name,&stage,tx.model,NULL,NULL);
    }
    if(!error){
        double mm;
        error=qa_mm_per_unit(tx.model,&mm);
        if(!error)tx.scale=1/mm;
    }
    if(!error){
        stage="quick stock outline";
        error=kind==Q_SPACER?quick_spacer_check(&tx,&plan,0):quick_box_check(&tx,plan.width,plan.depth,plan.thickness,0);
    }
#ifdef BWTL_QUICK_FAULT_TEST
    if(!error&&quick_fault_after_stock){stage="injected after stock";error=PRO_TK_GENERAL_ERROR;}
#endif
    if(!error&&plan.walls){
        stage="quick native bends";
        error=quick_bends(&tx,&plan,name);
    }
    if(!error&&plan.count){
        stage="quick cut and verify";
        error=quick_cuts(&tx,&plan);
    }
    if(!error&&plan.step_diameter){
        stage="quick stepped bore";
        error=quick_step(&tx,&plan);
    }
    const int has_stool=kind==Q_STOOL||kind==Q_CHAIR;
    if(!error&&has_stool){
        stage="quick stool legs and verification";
        error=quick_stool(&tx,&plan);
    }
    if(!error){
        stage="quick final verification";
        error=part_regenerate(tx.model);
        if(!error)error=part_health(tx.model);
        if(!error)error=has_stool?quick_stool_outline(&tx,&plan,0):kind==Q_SPACER?quick_spacer_check(&tx,&plan,1):quick_box_check(&tx,plan.width,plan.depth,plan.thickness,plan.walls);
    }
    if(!error&&kind==Q_CHAIR){
        stage="quick chair backrest";
        error=quick_backrest_build(tx.model,plan.back_width,plan.back_thickness,plan.back_height,
            backrest_report,_countof(backrest_report));
        if(!error){
            stage="quick chair final outline";
            backrest_report[0]=0;
            error=quick_stool_outline(&tx,&plan,plan.back_height);
        }
    }
    if(error){
        trace(stage,error);
        ProError erase=tx.model?ProMdlErase(tx.model):PRO_TK_NO_ERROR;
        if(erase)trace("quick incomplete model erase",erase);
        part_text_add(&note,L"%ls创建未通过原生检查（%hs，Creo %d）。%ls。",quick_labels[kind],stage,error,
            erase?L"未保存，临时模型仍在会话中待核对":L"未保存，临时模型已清理");
        if(backrest_report[0])part_text_add(&note,L"\n%ls",backrest_report);
        return error;
    }

    ProError display=dv_display_new(tx.model);
    PartText report={text,capacity,0};
    part_text_add(&report,L"已创建%ls %ls。",quick_labels[kind],name);
    if(has_stool)part_text_add(&report,L"矩形凳面 %.8g × %.8g mm，厚 %.8g mm，凳面离地 %.8g mm，四条 %.8g mm 方腿，距凳面边 %.8g mm。",plan.width,plan.depth,plan.thickness,plan.height,plan.leg_width,plan.leg_inset);
    if(kind==Q_CHAIR)part_text_add(&report,L"后侧直板靠背宽 %.8g mm、厚 %.8g mm、高出凳面 %.8g mm，整件总高 %.8g mm。",
        plan.back_width,plan.back_thickness,plan.back_height,plan.height+plan.back_height);
    if(plan.walls)part_text_add(&report,L"原生钣金，%d 道 90° 折弯，内 R %.8g mm。",plan.walls,plan.radius);
    if(plan.count)part_text_add(&report,L"已核对 %d 处开孔或开口。",plan.count);
    if(plan.bore)part_text_add(&report,L"两段外径 %.8g／%.8g mm，同轴通孔 Ø%.8g mm。",plan.width,plan.step_diameter,plan.bore);
    if(display)part_text_add(&report,L"已在会话生成，显示窗口返回 Creo %d。",display);
    part_text_add(&report,L"单实体重生成通过，未保存。");
    return PRO_TK_NO_ERROR;
}
