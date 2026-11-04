static void ai_poll(char *dialog, ProUITimerID timer, ProAppData data) {
    DWORD code;
    FILE *f;
    static wchar_t answer[16000];
    size_t count = 0;
    wint_t ch;
    if (!ai_process) return;
    if (WaitForSingleObject(ai_process, 0) == WAIT_TIMEOUT) {
        if (GetTickCount64() - ai_started > 180000) { ai_cleanup(); append(L"AI 请求超时，可重新发送。"); return; }
        if ((native_schedule_poll() ? PRO_TK_NO_ERROR : PRO_TK_GENERAL_ERROR) != PRO_TK_NO_ERROR) { ai_cleanup(); append(L"回复刷新失败。"); }
        return;
    }
    GetExitCodeProcess(ai_process, &code); CloseHandle(ai_process); ai_process = NULL;
    trace("PERF engine_until_observed_exit_ms",(int)(GetTickCount64()-ai_started));
    trace("PERF request_until_reply_ms",(int)(GetTickCount64()-ai_requested));
    trace("AI exit code", code);
    f = _wfopen(ai_output, L"rt, ccs=UTF-8");
    if (f) {
        while (count < 15990 && (ch = fgetwc(f)) != WEOF) answer[count++] = (wchar_t)ch;
        answer[count] = 0; fclose(f);
    }
    if (code != 0 || !f || !count) { append(L"AI 请求失败，请检查 临时目录 CreoCodex-进程号 下的 engine.log（可能是网络、登录或额度问题）。"); return; }
    append_card(L"AI",answer);
    save_turn(L"助手", answer);
    {
        PartCommand action={0};int parsed=part_command_parse(answer,&action);
        if(parsed<0){pending_plan.ready=0;memset(&pending_part,0,sizeof(pending_part));save_pending_plan();append(L"操作协议无效或包含多个操作，未执行。请一次说明一个明确操作。");return;}
        if(parsed==1){
            ProError queued;pending_plan.ready=0;save_pending_plan();pending_part=action;
            queued=ProMacroLoad(L"~ Command `CodexNativeBuildPending`;");
            if(queued){memset(&pending_part,0,sizeof(pending_part));append(L"原生零件操作调度失败，未执行。");}
            return;
        }
    }
    {
        int change = model_plan_update(answer, &pending_plan);
        if (change) trace("Pending model plan changed", change);
        if (change < 0) append(L"新方案格式或尺寸无效，旧方案已停用，请重新说明尺寸。");
        if (change < 0) save_pending_plan();
        if (change == 1) {
            ProError queued;
            if(request_requires_current){pending_plan.ready=0;save_pending_plan();append(L"本次要求操作当前模型，但AI返回了新建模型指令，已拦截，未另建零件。请给出当前拉伸的完整毫米尺寸。");return;}
            show_pending_plan();
            queued = ProMacroLoad(L"~ Command `CodexNativeBuildPending`;");
            trace("Queue native modeling command", queued);
            if (queued != PRO_TK_NO_ERROR) { pending_plan.ready = 0; append(L"无法调度原生建模命令，请查看日志。"); }
        }
    }
}
