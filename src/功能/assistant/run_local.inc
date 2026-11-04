static void run_local(char *dialog, char *component, ProAppData data) {
    wchar_t *value = NULL;
    ProError error = native_prompt_get(&value) ? PRO_TK_NO_ERROR : PRO_TK_GENERAL_ERROR;
    trace("ProUITextareaValueGet", error);
    if (error != PRO_TK_NO_ERROR || !value) return;
    {
        size_t length = wcslen(value);
        while (length && iswspace(value[length - 1])) value[--length] = 0;
        if (!length) { free(value); return; }
    }
    if(model_empty_request(value)){
        if(ai_process)append(L"请等待当前 AI 请求完成，再创建空模型。");
        else {
            memset(&pending_plan,0,sizeof(pending_plan));pending_plan.empty_kind=model_empty_request(value);pending_plan.ready=1;
            append_card(L"你",value);save_turn(L"用户",value);show_pending_plan();
            trace("Queue empty template command",ProMacroLoad(L"~ Command `CodexNativeBuildPending`;"));
            native_prompt_set(L"");
        }
    }
    else if (wcscmp(value, L"查看方案") == 0) { show_pending_plan(); native_prompt_set(L""); }
    else if (wcscmp(value, L"执行建模") == 0) { execute_model_plan(); native_prompt_set(L""); }
    else if (wcscmp(value,L"取消建模")==0 || wcscmp(value,L"取消操作")==0) {
        foundation_reset();
        memset(&pending_part,0,sizeof(pending_part));
        pending_plan.ready = 0;
        save_pending_plan();
        if (ai_process) { ai_cleanup(); native_stop_poll(); }
        append(L"已取消待执行的建模方案及正在等待的 AI 请求。"); native_prompt_set(L"");
    }
    else if(wcscmp(value,L"自检零件工具")==0){
        static wchar_t check_result[PART_TEXT_MAX];
        if(ai_process)append(L"请等待当前AI请求结束再自检。");
        else {part_selfcheck(check_result,PART_TEXT_MAX);append_card(L"执行结果",check_result);save_turn(L"自检结果",check_result);native_prompt_set(L"");}
    }
    else if (memory_command(value)) native_prompt_set(L"");
    else if(!wcscmp(value,L"读取详细信息")||!wcscmp(value,L"读取全部信息"))
        {read_model(dialog,component,(ProAppData)(UINT_PTR)15);native_prompt_set(L"");}
    else if(!wcscmp(value,L"读取详细尺寸"))
        {read_model(dialog,component,(ProAppData)(UINT_PTR)2);native_prompt_set(L"");}
    else if(!wcscmp(value,L"读取特征")||!wcscmp(value,L"读取参数")||!wcscmp(value,L"读取几何"))
        {unsigned mode=!wcscmp(value,L"读取特征")?1:!wcscmp(value,L"读取参数")?4:8;read_model(dialog,component,(ProAppData)(UINT_PTR)mode);native_prompt_set(L"");}
    else if (wcscmp(value,L"读取当前模型")==0 || wcscmp(value,L"读取当前零件")==0 || wcscmp(value,L"查看模型信息")==0 || wcscmp(value, L"model") == 0 || wcscmp(value,L"读取模型")==0 || wcscmp(value,L"读取基本信息")==0 || wcscmp(value,L"读取尺寸")==0 || wcscmp(value,L"读取质量")==0 || wcscmp(value, L"当前模型") == 0)
        {read_model(dialog,component,data);native_prompt_set(L"");}
    else
        ai_send(value);
    free(value);
}
