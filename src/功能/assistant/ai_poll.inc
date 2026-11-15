static int smart_resolve_intent(const wchar_t *answer){
    if(smart_intent!=SMART_INTENT_PENDING)return 1;
    int mode=smart_parse_intent(answer);
    if(mode<=SMART_INTENT_PENDING){
        if(!smart_intent_retry++){
            wcscpy_s(smart_feedback,PART_TEXT_MAX,L"上次未收到唯一的本轮意图标记。请根据最新指令和模型上下文重新判断并返回对应标记。");
            smart_queue_next();
        }else smart_pause(L"无法确认本轮要修改当前模型、另建模型还是只看方案",1);
        return 0;
    }
    if(mode==SMART_INTENT_NEW&&(smart_progress||wcsstr(smart_goal,L"不新建")||wcsstr(smart_goal,L"不要新建")||
       wcsstr(smart_goal,L"不要另建")||wcsstr(smart_goal,L"别另建"))){
        smart_pause(L"本轮要求保留当前任务目标，已拦截另建",1);
        return 0;
    }
    if(mode==SMART_INTENT_CURRENT&&(!request_snapshot.model||!request_snapshot.stamp)){
        smart_pause(L"你要修改现有模型，请先激活对应的 Creo 零件或装配",1);
        return 0;
    }
    smart_intent=mode;
    smart_readonly=!smart_intent_writable(mode);
    if(mode==SMART_INTENT_CURRENT){
        request_requires_current=1;
        smart_target_model=request_snapshot.model;
    }else if(mode==SMART_INTENT_NEW){
        request_requires_current=0;
        smart_target_model=NULL;
    }
    trace("Semantic task intent",mode);
    return 1;
}

static int smart_replan_output(const wchar_t *reason){
    if(smart_readonly&&smart_intent!=SMART_INTENT_PENDING)return 0;
    if(smart_target_model&&smart_steps&&!smart_binding_valid())return 0;
    if(smart_protocol_replan++)return 0;
    wcsncpy_s(smart_feedback,PART_TEXT_MAX,reason,_TRUNCATE);
    smart_queue_next();
    return 1;
}

static void ai_poll(char *dialog,ProUITimerID timer,ProAppData data){
    DWORD code;FILE *f;static wchar_t answer[16000],visible[16000];size_t count=0;wint_t ch;int truncated=0;
    if(!ai_process)return;
    if(WaitForSingleObject(ai_process,0)==WAIT_TIMEOUT){
        if(GetTickCount64()-ai_started>180000){ai_cleanup();smart_pause(L"AI 回复超时，任务暂停",1);return;}
        if(!native_schedule_poll()){ai_cleanup();smart_reply(L"回复刷新失败。");}return;
    }
    GetExitCodeProcess(ai_process,&code);CloseHandle(ai_process);ai_process=NULL;trace("AI exit code",code);
    f=_wfopen(ai_output,L"rt, ccs=UTF-8");
    if(f){while(count<15990&&(ch=fgetwc(f))!=WEOF)answer[count++]=(wchar_t)ch;answer[count]=0;if(count==15990&&fgetwc(f)!=WEOF)truncated=1;fclose(f);}
    if(code||!f||!count||truncated){smart_pause(L"AI 回复失败或超出接收上限，任务暂停",1);return;}
    smart_answer_text(answer,visible,16000);
    int routing=smart_intent==SMART_INTENT_PENDING;
    if(!smart_resolve_intent(answer))return;
    if(smart_intent==SMART_INTENT_CHAT){
        smart_reply(visible[0]?visible:L"这条消息没有收到完整答复，请再发一次。");
        return;
    }
    if(smart_intent==SMART_INTENT_CLARIFY){
        smart_waiting_for_user=1;
        smart_reply(visible[0]?visible:L"请说明要处理的模型和具体改动。");
        return;
    }
    {
        PartCommand action={0};int parsed=part_command_parse(answer,&action);
        if(routing&&!(parsed==1&&(action.verb==PART_IMITATE||action.verb==PART_IMITATE_SIZE||action.verb==PART_IMITATE_LOCAL)&&smart_intent==SMART_INTENT_NEW)){
            smart_queue_next();
            return;
        }
        if(parsed<0){pending_plan.ready=0;memset(&pending_part,0,sizeof(pending_part));save_pending_plan();if(!smart_replan_output(L"上一条协议格式有误或含多条命令。重新根据最新结果给出一条合法的下一步。"))smart_pause(L"AI 返回无效或多条操作，本步未执行",1);return;}
        if(parsed==1){
            ProError queued;
            if(!smart_protocol_allowed(&action)){
                const int delete_block=!smart_readonly&&!wcscmp(action.arg[0],L"FEATURE_DELETE");
                const wchar_t *reason=delete_block?smart_delete_block_message:smart_operation_block();
                if(!delete_block&&smart_replan_output(reason))return;
                smart_reply(reason);return;
            }
            pending_plan.ready=0;save_pending_plan();pending_part=action;
            queued=ProMacroLoad(L"~ Command `CodexNativeBuildPending`;");
            if(queued){memset(&pending_part,0,sizeof(pending_part));smart_pause(L"原生操作调度失败，本步未执行",1);}return;
        }
    }
    {
        int change=model_plan_update(answer,&pending_plan);
        if(change<0){save_pending_plan();smart_pause(L"AI 返回无效的新模型方案，本步未执行",1);return;}
        if(change==1){
            if(quick_intent(smart_goal,quick_topic(smart_goal))){pending_plan.ready=0;save_pending_plan();smart_reply(L"此整件需要对应的快速生成参数。未创建基础替代件。");return;}
            ProError queued;if(smart_readonly||request_requires_current||smart_progress){pending_plan.ready=0;save_pending_plan();smart_reply(smart_readonly?smart_write_block_message:L"本轮要求使用当前模型，已拦截另建模型的指令。");return;}
            queued=ProMacroLoad(L"~ Command `CodexNativeBuildPending`;");if(queued){pending_plan.ready=0;smart_reply(L"无法调度原生建模。");}return;
        }
    }
    if(smart_handle_stale_permission(visible))return;
    smart_waiting_for_user=!smart_readonly&&(smart_answer_state(answer)==1||(!smart_answer_state(answer)&&smart_reply_needs_input(visible)));
    if(smart_failures_without_progress&&!smart_waiting_for_user){if(smart_replan_output(L"原生步骤还没有完成，请核对最近一条失败回执，换用有依据的参照或可用的方法，继续剩余步骤。"))return;smart_pause(L"仍有失败步骤待处理，任务暂停",1);return;}
    smart_reply(visible[0]?visible:smart_feedback[0]?smart_feedback:L"本轮未收到可显示的结果，请重新发送需求。");
}
