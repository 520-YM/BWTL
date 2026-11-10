static void execute_part_command(void){
    ULONGLONG execution_started=GetTickCount64();
    static wchar_t result[PART_TEXT_MAX];
    PartCommand command=pending_part;
    memset(&pending_part,0,sizeof(pending_part));
    if(command.verb==PART_NONE||!smart_active)return;
    if(!smart_protocol_allowed(&command)){
        smart_reply(!smart_readonly&&!wcscmp(command.arg[0],L"FEATURE_DELETE")?smart_delete_block_message:smart_write_block_message);
        return;
    }
    const wchar_t *verb=command.arg[0];
    int foundation=command.verb==PART_FOUNDATION;
    int special=foundation&&(dv_kind(verb)||ax_kind(verb)||pn_kind(verb)||cm_kind(verb)||fx_kind(verb)||
        mp_kind(verb)||vt_kind(verb)||rv_kind(verb)||in_kind(verb)||org_kind(verb)||fab_kind(verb)||quick_kind(verb)||!wcscmp(verb,L"STOOL_BACK"));
    int independent=foundation&&(dv_kind(verb)==DV_IMPORT||quick_kind(verb));
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

    ProError error=part_execute(&command,&request_snapshot,result,PART_TEXT_MAX);
    trace("PERF native_execute_ms",(int)(GetTickCount64()-execution_started));
    if(error&&foundation&&cm_kind(verb)==BWTL_CM_TRAY&&error==PRO_TK_BAD_INPUTS)final=0;
    int progress=0,unsafe=0;
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
    if(quality_report_flow&&!unsafe&&smart_binding_valid())smart_target_model=request_snapshot.model;
    if(!result[0])swprintf_s(result,_countof(result),L"原生操作 %ls 返回 Creo %d。",verb,error);
    smart_step(&command,verb,error,result,progress,final,unsafe);
}
