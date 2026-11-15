#include "traceback_risk.inc"
static ProError imitation_run_chat(ProMdl source,ProPath delivered,int *partial,PartText *out);
static ProError imitation_resize_command(const PartCommand *command,const PartSnapshot *snapshot,
                                         wchar_t *text,size_t capacity);
static ProError imitation_local_command(const PartCommand *command,const PartSnapshot *snapshot,
                                         wchar_t *text,size_t capacity);

static ProError imitation_find_wall(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProFeattype type;
    if(ProFeatureTypeGet(feature,&type))return PRO_TK_NO_ERROR;
    if(type==PRO_FEAT_WALL)*(int *)data|=1;
    if(type==PRO_FEAT_DRAFT)*(int *)data|=2;
    if(type==PRO_FEAT_CUT||type==PRO_FEAT_PROTRUSION){
        ProElement tree=NULL,depth;
        int kind;
        if(!ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)&&
           !mc_tree_element(tree,(ProElemId[]){PRO_E_STD_EXT_DEPTH,PRO_E_EXT_DEPTH_TO,PRO_E_EXT_DEPTH_TO_TYPE},3,&depth)&&
           !ProElementIntegerGet(depth,NULL,&kind)&&kind==PRO_EXT_DEPTH_TO_NEXT)*(int *)data|=4;
        if(tree)ProFeatureElemtreeFree(feature,tree);
    }
    return PRO_TK_NO_ERROR;
}

static int imitation_warn_wall(ProMdl model){
    int found=0;
    ProMdlType type;
    if(model&&!ProMdlTypeGet(model,&type)&&type==PRO_MDL_PART)
        ProSolidFeatVisit((ProSolid)model,imitation_find_wall,NULL,&found);
    if(found&1){
        const wchar_t *message=L"【回溯风险重点提醒】本次含钣金凸缘。边法兰在不完整附加壁或失败回滚、相邻翻边留缝在直接重定义已有壁时曾触发 Creo 回溯。此次将在新建零件上逐步重建。";
        append_card(L"提示",message);
        save_turn(L"回溯风险",message);
        trace("Traceback risk notice before replay",93);
    }
    if(found&2){
        const wchar_t *message=L"【回溯风险重点提醒】本次包含拔模。旧版读取或映射拔模面参照时曾触发 Creo 回溯。";
        append_card(L"提示",message);
        save_turn(L"回溯风险",message);
        trace("Traceback risk notice before replay",26);
    }
    if(found&4){
        const wchar_t *message=L"【回溯风险重点提醒】本次包含拉伸到下一面。旧版释放射线选取结果时曾触发 Creo 回溯。";
        append_card(L"提示",message);
        save_turn(L"回溯风险",message);
        trace("Traceback risk notice before replay",52);
    }
    return found;
}

static void execute_part_command(void){
    ULONGLONG execution_started=GetTickCount64();
    static wchar_t result[PART_TEXT_MAX];
    PartCommand command=pending_part;
    memset(&pending_part,0,sizeof(pending_part));
    if(command.verb==PART_NONE||!smart_active)return;
    if(!smart_protocol_allowed(&command)){
        smart_reply(!smart_readonly&&!wcscmp(command.arg[0],L"FEATURE_DELETE")?smart_delete_block_message:smart_operation_block());
        return;
    }
    if(command.verb!=PART_READ&&!storage_prepare_output()){smart_reply(storage_output_message);return;}
    if(command.verb==PART_IMITATE){
        PartText receipt={result,_countof(result),0};
        ProPath delivered;
        int partial=0;
        ProMdl model=NULL;
        result[0]=0;
        ProError error=part_check_binding(&command,&request_snapshot,&model);
        if(!error&&request_snapshot.in_assembly)error=PRO_TK_BAD_CONTEXT;
        if(!error){
            imitation_warn_wall(model);
            error=imitation_run_chat(model,delivered,&partial,&receipt);
        }
        if(error&&!result[0])part_text_add(&receipt,L"请先激活要按原特征树重建的零件（Creo %d）。",error);
        smart_step(&command,L"按特征树仿照",error,result,!error||partial,1,0);
        return;
    }
    if(command.verb==PART_IMITATE_SIZE){
        imitation_warn_wall(request_snapshot.model);
        ProError error=imitation_resize_command(&command,&request_snapshot,result,_countof(result));
        smart_step(&command,L"仿照并改尺寸",error,result,!error,1,0);
        return;
    }
    if(command.verb==PART_IMITATE_LOCAL){
        ProError error=imitation_local_command(&command,&request_snapshot,result,_countof(result));
        smart_step(&command,L"局部仿照",error,result,!error,1,0);
        return;
    }
    const wchar_t *verb=command.arg[0];
    int foundation=command.verb==PART_FOUNDATION;
    int special=foundation&&(dv_kind(verb)||ax_kind(verb)||pn_kind(verb)||cm_kind(verb)||fx_kind(verb)||
        mp_kind(verb)||vt_kind(verb)||rv_kind(verb)||in_kind(verb)||org_kind(verb)||fab_kind(verb)||smx_kind(verb)||quick_kind(verb)||!wcscmp(verb,L"STOOL_BACK"));
    int independent=command.verb==PART_COPY||(foundation&&(dv_kind(verb)==DV_IMPORT||quick_kind(verb)||smx_independent(smx_kind(verb))));
    int quality=foundation?ax_kind(verb):AX_NONE;
    int quality_report_flow=quality>=AX_Q_REGEN&&quality<AX_Q_REPORT&&
        (wcsstr(smart_goal,L"检查报告")||wcsstr(smart_goal,L"质量报告")||
         wcsstr(smart_goal,L"检查结果汇总"));
    int assembly_lookup=quality_report_flow||(foundation&&((smart_intent==SMART_INTENT_CURRENT&&
        (ax_read_diagnostic(ax_kind(verb))||ax_delete_preview(&command)))||
        (smart_intent==SMART_INTENT_READ&&ax_motion_inspection_request(smart_goal)&&ax_read_diagnostic(ax_kind(verb)))));
    int final=quality==AX_Q_REPORT||independent||(special&&!smart_chain&&!assembly_lookup)||command.verb==PART_DISPLAY;
    int write=!smart_read_command(&command);
    if(smart_block_duplicate(&command))return;
    if((command.verb==PART_DIMS||command.verb==PART_PARAM||command.verb==PART_SUPPRESS||command.verb==PART_RESUME)&&!part_selfcheck_passed){
        ProError check=part_selfcheck(result,PART_TEXT_MAX);
        if(check){smart_step(NULL,L"零件工具自检",check,result,0,1,1);return;}
    }

    bwtl_warn_traceback(&command);
    ProError error=part_execute(&command,&request_snapshot,result,PART_TEXT_MAX);
    trace("PERF native_execute_ms",(int)(GetTickCount64()-execution_started));
    if(error&&foundation&&cm_kind(verb)==BWTL_CM_TRAY&&error==PRO_TK_BAD_INPUTS)final=0;
    int progress=command.verb==PART_COPY&&!error,unsafe=0;
    if(!independent&&command.verb!=PART_DISPLAY&&request_snapshot.stamp){
        if(error){
            ProMdl target=NULL;
            unsafe=part_check_binding(&command,&request_snapshot,&target)!=PRO_TK_NO_ERROR;
            if(!unsafe&&!final&&(!smart_readonly||assembly_lookup))
                unsafe=!smart_binding_set(target,request_snapshot.window_model,request_snapshot.directory);
        }else{
            ProMdl target=request_snapshot.model;
            ProWVerstamp now=NULL;
            if(write&&command.verb!=PART_SAVE&&command.verb!=PART_STEP){
                if(ProMdlVerstampGet(target,&now))unsafe=1;
                else{progress=!ProVerstampEqual(request_snapshot.stamp,now);ProVerstampFree(&now);}
            }
            if(!unsafe&&!final&&(!smart_readonly||assembly_lookup))
                unsafe=!smart_binding_set(target,request_snapshot.window_model,request_snapshot.directory);
        }
    }
    if(!error&&progress&&!unsafe&&!final&&request_snapshot.type==PRO_MDL_PART&&
       request_snapshot.model==smart_target_model&&request_snapshot.model==request_snapshot.window_model)
        smart_committed_part=smart_part_signature(request_snapshot.model);
    if(quality_report_flow&&!unsafe&&smart_binding_valid())smart_target_model=request_snapshot.model;
    if(!result[0])swprintf_s(result,_countof(result),L"原生操作 %ls 返回 Creo %d。",verb,error);
    smart_step(&command,verb,error,result,progress,final,unsafe);
}
