static void run_local(char *dialog, char *component, ProAppData data) {
    wchar_t *value = NULL;
    ModelPlan centered_plan={0};int centered;
    ProError error = native_prompt_get(&value) ? PRO_TK_NO_ERROR : PRO_TK_GENERAL_ERROR;
    trace("ProUITextareaValueGet", error);
    if (error != PRO_TK_NO_ERROR || !value) return;
    {
        size_t length = wcslen(value);
        while (length && iswspace(value[length - 1])) value[--length] = 0;
        if (!length) {if(native_image_count){free(value);value=_wcsdup(L"请分析这些图片，说明你能看出的内容、设计需求和需要补充的信息，先不要修改模型。");if(!value)return;}else{free(value);return;}}
    }
    {
        UINT command=0;
        if(!wcscmp(value,L"添加图片"))command=CHAT_ATTACH;
        else if(!wcscmp(value,L"清除图片"))command=CHAT_CLEAR_IMAGES;
        else if(!wcscmp(value,L"执行明细"))command=CHAT_DETAILS;
        else if(!wcscmp(value,L"插件功能"))command=CHAT_FEATURES;
        else if(!wcscmp(value,L"公司标准"))command=CHAT_COMPANY;
        if(command){
            native_prompt_set(L"");
            PostMessageW(native_window,WM_COMMAND,command,0);
            free(value);
            return;
        }
    }
    if(!wcscmp(value,L"查看学习记忆")||!wcscmp(value,L"装配学习")||!wcscmp(value,L"学习当前装配")){
        if(native_busy||smart_active||ai_process)append(L"请等待当前任务完成。");
        else{
            imitation_queue_action(!wcscmp(value,L"查看学习记忆")?IMITATION_MEMORY:IMITATION_ASSEMBLY_LEARN);
            native_prompt_set(L"");
        }
        free(value);
        return;
    }
    if(!wcscmp(value,L"继续上次任务")){
        if(smart_active)append(L"当前任务仍在运行。");
        else if(!smart_resume())append(L"本会话没有可继续的任务，或活动模型、版本、工作目录与暂停时不同。");
        native_prompt_set(L"");free(value);return;
    }
    if(wcsstr(value,L"〔待填：")){
        append(L"请先补齐指令中标为「待填」的内容，再发送。");
        free(value);
        return;
    }
    if(!wcscmp(value,L"应用已验升级")||!wcscmp(value,L"功能开发")||!wcscmp(value,L"功能开发（升级插件）")){
        append(L"BWTL 更新在 Codex 中处理。");native_prompt_set(L"");free(value);return;
    }
    centered=model_centered_request(value,&centered_plan);
    if(centered){
        if(native_busy)append(L"请等待当前任务完成。");
        else if(centered<0)append(L"请输入「新建原点居中圆柱 直径 高度」，两个数值均为 0.1～10000 毫米，例如「新建原点居中圆柱 150 150」。");
        else{
            smart_begin(value);smart_local_plan=1;pending_plan=centered_plan;
            append_card(L"你",value);save_turn(L"用户",value);native_prompt_set(L"");
            execute_model_plan();
        }
    }
    else if(model_empty_request(value)||(!native_image_count&&smart_sheetmetal_demo_request(value))){
        if(ai_process)append(L"请等待当前 AI 请求完成，再创建空模型。");
        else {
            smart_begin(value);
            smart_local_plan=1;
            smart_sheetmetal_demo=!native_image_count&&smart_sheetmetal_demo_request(value);
            if(smart_sheetmetal_demo)smart_readonly=0;
            memset(&pending_plan,0,sizeof(pending_plan));
            pending_plan.empty_kind=smart_sheetmetal_demo?2:model_empty_request(value);
            pending_plan.ready=1;
            append_card(L"你",value);save_turn(L"用户",value);
            error=ProMacroLoad(L"~ Command `CodexNativeBuildPending`;");
            trace("Queue empty template command",error);
            if(error){pending_plan.ready=0;smart_reply(L"无法调度原生建模。");}
            native_prompt_set(L"");
        }
    }
    else if (wcscmp(value, L"查看方案") == 0) { show_pending_plan(); native_prompt_set(L""); }
    else if (wcscmp(value, L"执行建模") == 0) { execute_model_plan(); native_prompt_set(L""); }
    else if (wcscmp(value,L"取消建模")==0 || wcscmp(value,L"取消操作")==0) {
        smart_cancel();foundation_reset();
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
