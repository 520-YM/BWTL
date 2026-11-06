static void ai_poll(char *dialog,ProUITimerID timer,ProAppData data){
    DWORD code;FILE *f;static wchar_t answer[16000],visible[16000],protocol[8192];size_t count=0;wint_t ch;int truncated=0;
    if(!ai_process)return;
    if(WaitForSingleObject(ai_process,0)==WAIT_TIMEOUT){
        if(GetTickCount64()-ai_started>180000){ai_cleanup();smart_reply(L"本轮回复超时。已完成的步骤可在执行明细查看。");return;}
        if(!native_schedule_poll()){ai_cleanup();smart_reply(L"回复刷新失败。");}return;
    }
    GetExitCodeProcess(ai_process,&code);CloseHandle(ai_process);ai_process=NULL;trace("AI exit code",code);
    f=_wfopen(ai_output,L"rt, ccs=UTF-8");
    if(f){while(count<15990&&(ch=fgetwc(f))!=WEOF)answer[count++]=(wchar_t)ch;answer[count]=0;if(count==15990&&fgetwc(f)!=WEOF)truncated=1;fclose(f);}
    if(code||!f||!count||truncated){smart_reply(L"AI 回复失败或超出接收上限。已完成的步骤可在执行明细查看。");return;}
    smart_answer_text(answer,visible,16000);
    {
        PartCommand action={0};int parsed=part_command_parse(answer,&action),i;
        if(parsed<0){pending_plan.ready=0;memset(&pending_part,0,sizeof(pending_part));save_pending_plan();smart_reply(L"回复中包含无效或多条操作，本步未执行。");return;}
        if(parsed==1){
            ProError queued;protocol[0]=0;for(i=0;i<action.count;i++){wcscat_s(protocol,8192,action.arg[i]);wcscat_s(protocol,8192,L" ");}
            if(!wcscmp(protocol,smart_last_protocol))smart_repeats++;else smart_repeats=0;wcscpy_s(smart_last_protocol,8192,protocol);
            if(smart_repeats>=2){smart_reply(L"检测到相同操作反复出现，任务已暂停。请补充目标或查看执行明细。");return;}
            if(!smart_protocol_allowed(&action)){smart_reply(smart_write_block_message());return;}
            pending_plan.ready=0;save_pending_plan();pending_part=action;
            queued=ProMacroLoad(L"~ Command `CodexNativeBuildPending`;");
            if(queued){memset(&pending_part,0,sizeof(pending_part));smart_reply(L"原生操作调度失败，本步未执行。");}return;
        }
    }
    {
        int change=model_plan_update(answer,&pending_plan);
        if(change<0){save_pending_plan();smart_reply(L"新模型方案格式无效，本步未执行。");return;}
        if(change==1){
            ProError queued;if(smart_readonly||request_requires_current){pending_plan.ready=0;save_pending_plan();smart_reply(smart_readonly?smart_write_block_message():L"本轮要求使用当前模型，已拦截另建模型的指令。");return;}
            queued=ProMacroLoad(L"~ Command `CodexNativeBuildPending`;");if(queued){pending_plan.ready=0;smart_reply(L"无法调度原生建模。");}return;
        }
    }
    smart_waiting_for_user=!smart_readonly&&(smart_answer_state(answer)==1||(!smart_answer_state(answer)&&smart_reply_needs_input(visible)));
    smart_reply(visible[0]?visible:smart_feedback[0]?smart_feedback:L"本轮未收到可显示的结果，请重新发送需求。");
}
