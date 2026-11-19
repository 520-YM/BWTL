#ifndef BWTL_SMART_SESSION_H
#define BWTL_SMART_SESSION_H
#include "smart_policy.h"
#include "smart_ledger.h"
#define SMART_DETAILS_CAP 160000
static int smart_active,smart_followup,smart_readonly,smart_steps,smart_failures,smart_failures_without_progress;
static int smart_total_failures,smart_progress,smart_paused,smart_chain,smart_duplicate_replan;
static wchar_t smart_goal[6001],smart_feedback[PART_TEXT_MAX],smart_details[SMART_DETAILS_CAP],smart_new_model_result[1024];
static wchar_t smart_latest_request[6001],smart_prior_goal[6001];
static wchar_t smart_last_failure[1530],smart_last_failure_brief[360];
static NativeImage smart_images[NATIVE_IMAGE_MAX];static int smart_image_count;
static ULONGLONG smart_started;
static ProMdl smart_target_model;
#ifdef BWTL_COMPONENT_OPEN_HOST
static AxReturnLink smart_parent_link;
#endif

static void smart_forget_parent_link(void){
#ifdef BWTL_COMPONENT_OPEN_HOST
    smart_parent_link=(AxReturnLink){0};
#endif
}
static int smart_waiting_for_user;
static int smart_stale_permission_retry;
static int smart_has_details,smart_local_plan,smart_sheetmetal_demo,smart_protocol_replan;
static int smart_intent,smart_intent_retry;
static void ai_send(const wchar_t *value);
static void smart_queue_next(void);
static int smart_unsafe_receipt(const wchar_t *receipt);
#include "功能/assistant/smart_failure_brief.inc"
#ifdef BWTL_TASK_CONTEXT_HOST
#include "功能/assistant/chat_task_evidence.inc"
#endif

static int smart_sequence_request(const wchar_t *text){
    static const wchar_t *const words[]={L"然后",L"接着",L"随后",L"之后",L"再开",L"再加",L"再做",L"再建",L"再倒",L"再切",L"分步完成",L"连续执行",L"做到哪步算哪步",L"能做多少做多少"};
    return smart_has_text(text,words,_countof(words));
}

static const wchar_t *smart_details_tail(size_t maximum){
    size_t length=wcslen(smart_details);
    if(length<=maximum)return smart_details;
    const wchar_t *line=wcsstr(smart_details+length-maximum,L"\n\n第 ");
    return line?line+2:smart_details+length-maximum;
}

static int smart_new_plan_allowed(void){
    return smart_intent==SMART_INTENT_NEW&&!smart_readonly&&!smart_progress&&!smart_target_model;
}

static void smart_finish(void){smart_active=smart_followup=smart_sheetmetal_demo=0;native_set_busy(0);}
static void smart_reply(const wchar_t *text){
#ifdef BWTL_RESIDENT_CHANNEL
    if(resident_tool_running()){resident_tool_result(-1,text);return;}
#endif
    wchar_t *combined=NULL;
    if(smart_failures_without_progress&&smart_last_failure_brief[0]){
        size_t capacity=wcslen(text)+wcslen(smart_last_failure_brief)+2;
        combined=malloc(capacity*sizeof(*combined));
        if(combined){
            if(swprintf_s(combined,capacity,L"%ls\n%ls",text,smart_last_failure_brief)>=0)text=combined;
        }
    }
    append_card(L"AI",text);
    save_turn(L"助手",text);
    free(combined);
    smart_finish();
    if(!smart_paused&&!smart_waiting_for_user){
        smart_binding_clear();
        smart_forget_parent_link();
    }
}

static void smart_begin(const wchar_t *value){
    wchar_t combined[6001];ProMdl prior_target=smart_target_model;
    static const wchar_t *const new_task[]={L"另外",L"换个",L"换一个",L"另建",L"另画",L"新建",L"再画一个",L"重新画"};
    wcsncpy_s(smart_latest_request,_countof(smart_latest_request),value,_TRUNCATE);
    int supplement=smart_waiting_for_user&&(!smart_readonly||smart_intent==SMART_INTENT_CLARIFY)&&
        !smart_readonly_override(value)&&!model_request_explicit_new(value)&&
        !smart_has_text(value,new_task,_countof(new_task))&&
        smart_short_reply(value)&&wcslen(smart_goal)<4000&&wcslen(value)<1500;
    int retain=supplement&&smart_steps>0;
    smart_prior_goal[0]=0;
    if(supplement){
        wcscpy_s(smart_prior_goal,_countof(smart_prior_goal),smart_goal);
        swprintf_s(combined,6001,L"原任务：%ls\n用户补充或确认（以本次为准）：%ls",smart_prior_goal,value);
        value=combined;
    }
    smart_active=1;smart_followup=0;smart_started=GetTickCount64();
    smart_paused=smart_waiting_for_user=smart_local_plan=smart_stale_permission_retry=smart_duplicate_replan=smart_protocol_replan=smart_sheetmetal_demo=0;
    if(!retain){
        smart_binding_clear();
        smart_forget_parent_link();
        smart_steps=smart_failures=smart_total_failures=smart_failures_without_progress=smart_progress=0;
        smart_attempt_count=smart_generation=smart_has_details=0;
        smart_feedback[0]=smart_new_model_result[0]=smart_last_failure[0]=smart_last_failure_brief[0]=0;
    }
    smart_target_model=supplement?prior_target:NULL;
    wcsncpy_s(smart_goal,6001,value,_TRUNCATE);
    smart_readonly=supplement?0:smart_analysis_request(value);
    smart_intent=SMART_INTENT_LOCAL;smart_intent_retry=0;
    smart_chain=!smart_readonly&&smart_sequence_request(value);
    smart_image_count=native_images_snapshot(smart_images);
    native_set_busy(1);
}
static void smart_cancel(void){
    smart_waiting_for_user=smart_paused=0;
    smart_binding_clear();
    smart_forget_parent_link();
    smart_finish();ai_cleanup();native_stop_poll();
    memset(&pending_part,0,sizeof(pending_part));pending_plan.ready=0;
}

static void smart_record_tool(const wchar_t *receipt){
    wcsncpy_s(smart_feedback,PART_TEXT_MAX,receipt,_TRUNCATE);
    if(!smart_has_details)smart_details[0]=0;
    else store_tail_add(smart_details,SMART_DETAILS_CAP,L"\n\n");
    smart_has_details=1;
    store_tail_add(smart_details,SMART_DETAILS_CAP,receipt);
    if(store_ready)store_write(L"last-native-details.txt",smart_details);
}

static void smart_pause(const wchar_t *reason,int allow_resume){
    wchar_t message[2000];
    smart_waiting_for_user=0;
    smart_paused=allow_resume&&!smart_readonly&&smart_steps>0&&smart_steps<SMART_MAX_STEPS&&smart_binding_valid();
    swprintf_s(message,_countof(message),L"%ls。已执行 %d 步，完成 %d 个模型改动，累计失败 %d 次。\n%.780ls\n%ls",
        reason,smart_steps,smart_progress,smart_total_failures,smart_failures_without_progress&&smart_last_failure[0]?smart_last_failure:smart_feedback,
        smart_paused?L"核对「执行明细」后，可发送「继续上次任务」。":L"可在「执行明细」查看原生回执。");
    smart_reply(message);
}

static void smart_stop(void){
    if(!smart_active)return;
    ai_cleanup();native_stop_poll();
    memset(&pending_part,0,sizeof(pending_part));pending_plan.ready=0;
    smart_pause(L"已按你的要求停止，已经完成的操作保留",1);
}

/* A user-initiated resume resets only the automatic failure allowance. */
static int smart_resume(void){
    if(smart_active||ai_process||!smart_paused||!smart_binding_valid())return 0;
    smart_paused=smart_waiting_for_user=0;
    smart_failures=smart_failures_without_progress=smart_duplicate_replan=smart_stale_permission_retry=0;
    smart_started=GetTickCount64();
    smart_active=smart_followup=1;
    native_set_busy(1);
    smart_queue_next();
    return 1;
}

static int smart_unsafe_receipt(const wchar_t *receipt){
    static const wchar_t *const terms[]={L"恢复未能核实",L"恢复状态未能核实",L"本次会话已停止加工写入",
        L"回滚未能",L"未能完整验证回滚",L"恢复原名未成功",L"恢复状态未能完整核实",L"恢复不确定"};
    return smart_has_text(receipt,terms,_countof(terms));
}

static void smart_queue_next(void){
    smart_followup=1;
    if(ProMacroLoad(L"~ Command `BWTLSmartContinue`;"))smart_pause(L"无法调度下一步，任务暂停",1);
}

static int smart_block_duplicate(const PartCommand *cmd){
    if(!smart_protocol_seen(cmd))return 0;
    if(!smart_duplicate_replan++){
        swprintf_s(smart_feedback,PART_TEXT_MAX,L"宿主拦截重复操作 %.150ls。请根据已完成项和当前快照换参照或选择不同方法。",cmd->arg[0]);
        smart_queue_next();
    }else smart_pause(L"已拦截反复提交的相同操作，任务暂停",1);
    return 1;
}

static void smart_step(const PartCommand *cmd,const wchar_t *operation,ProError error,
                       const wchar_t *receipt,int model_progress,int final,int unsafe){
    const wchar_t *stop_reason=NULL;
    if(!smart_active)return;
    if(cmd)smart_protocol_remember(cmd,error);
    smart_steps++;
    if(error){smart_failures++;smart_total_failures++;smart_failures_without_progress++;}
    else if(model_progress){smart_progress++;smart_generation++;smart_failures_without_progress=0;smart_duplicate_replan=smart_protocol_replan=0;}
    wchar_t label[550];
    static wchar_t entry[7600];
    if(cmd)smart_protocol_display(cmd,label,_countof(label));
    else wcsncpy_s(label,_countof(label),operation,_TRUNCATE);
    int receipt_limit=!error&&cmd&&cmd->verb==PART_FOUNDATION&&ax_read_diagnostic(ax_kind(cmd->arg[0]))?6800:880;
    swprintf_s(entry,_countof(entry),L"第 %d 步｜%ls｜%ls｜Creo %d\n%.*ls",
        smart_steps,error?L"失败":L"成功",label,error,receipt_limit,receipt);
    if(error){
        wcscpy_s(smart_last_failure,_countof(smart_last_failure),entry);
        smart_describe_failure(cmd,receipt,smart_last_failure_brief,_countof(smart_last_failure_brief));
    }
    smart_record_tool(entry);
#ifdef BWTL_TASK_CONTEXT_HOST
    chat_task_record(cmd,label,error,receipt,unsafe||smart_unsafe_receipt(receipt));
#endif
    int tool=0;
#ifdef BWTL_RESIDENT_CHANNEL
    tool=resident_tool_running();
#endif
    if(unsafe||smart_unsafe_receipt(receipt)){
#ifdef BWTL_RESIDENT_CHANNEL
        if(tool){resident_tool_result(error?error:-1,receipt);ai_cleanup();}
#endif
        smart_pause(L"原生回执或模型版本提示状态需要核对，已停止自动操作",0);
        return;
    }
    if(!tool&&(final||smart_local_plan)){smart_reply(receipt);return;}
    if(smart_failures_without_progress>=SMART_MAX_FAILURES_WITHOUT_PROGRESS)stop_reason=L"尚未取得新的建模进展，失败次数已达上限";
    else if(smart_failures>=SMART_MAX_FAILURES)stop_reason=L"本轮累计失败次数已达上限";
    else if(smart_steps>=SMART_MAX_STEPS)stop_reason=L"已达到本任务步骤上限";
    else if(GetTickCount64()-smart_started>1200000)stop_reason=L"本轮任务已超时";
    if(stop_reason){
#ifdef BWTL_RESIDENT_CHANNEL
        if(tool){resident_tool_result(error?error:-1,receipt);ai_cleanup();}
#endif
        smart_pause(stop_reason,1);
        return;
    }
#ifdef BWTL_RESIDENT_CHANNEL
    if(tool){resident_tool_result(error,receipt);return;}
#endif
    smart_queue_next();
}

static void smart_after_tool(ProError error,const wchar_t *receipt,int model_progress){
    smart_step(NULL,L"原生建模",error,receipt,model_progress,0,0);
}

static int smart_continue_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    if(smart_active&&smart_followup&&!ai_process)ai_send(smart_goal);return 0;
}
static int smart_protocol_allowed(const PartCommand *command){
    if(!smart_active)return 0;
    if(smart_readonly&&!smart_read_command(command))return 0;
    int quick=command->verb==PART_FOUNDATION?quick_kind(command->arg[0]):Q_NONE;
    int independent=command->verb==PART_COPY||command->verb==PART_IMITATE||command->verb==PART_IMITATE_SIZE||command->verb==PART_IMITATE_LOCAL||
        (command->verb==PART_FOUNDATION&&(quick||dv_kind(command->arg[0])==DV_IMPORT||smx_independent(smx_kind(command->arg[0]))));
    if(smart_intent==SMART_INTENT_PENDING||smart_intent==SMART_INTENT_CLARIFY||smart_intent==SMART_INTENT_CHAT)return 0;
    if(smart_intent==SMART_INTENT_CURRENT&&independent)return 0;
    if(command->verb==PART_COPY&&(wcsstr(smart_goal,L"仿照")||wcsstr(smart_goal,L"重新画")))return 0;
    if(smart_intent==SMART_INTENT_NEW){
        if(!smart_progress&&!independent&&!smart_read_command(command))return 0;
        if(smart_progress&&independent)return 0;
    }
#ifdef BWTL_COMPONENT_OPEN_HOST
    if(command->verb==PART_DISPLAY||(command->verb==PART_FOUNDATION&&ax_kind(command->arg[0])==AX_OPEN)){
        const wchar_t *const no_open[]={L"不要打开",L"别打开",L"不打开",L"如何打开",L"怎么打开",L"是不是不能打开"};
        const wchar_t *const opening[]={L"打开",L"显示",L"切换到"};
        if(smart_has_text(smart_latest_request,no_open,_countof(no_open)))return 0;
        if(smart_intent!=SMART_INTENT_LOCAL&&!smart_has_text(smart_goal,opening,_countof(opening))&&
           !(smart_progress&&smart_steps&&smart_intent==SMART_INTENT_NEW&&
             request_snapshot.type==PRO_MDL_ASSEMBLY&&
             (wcsstr(smart_details,L"ASM_NEW_PART")||wcsstr(smart_details,L"ASM_NEW_ASM"))))return 0;
    }
#endif
    if(command->verb==PART_SAVE&&smart_intent!=SMART_INTENT_LOCAL&&
       (!wcsstr(smart_goal,L"保存")||wcsstr(smart_goal,L"不保存")||wcsstr(smart_goal,L"别保存")||
        wcsstr(smart_goal,L"不要保存")||wcsstr(smart_goal,L"暂不保存")))return 0;
    if(independent&&quick!=Q_CHAIR&&quick_intent(smart_goal,Q_CHAIR))return 0;
    if(quick||(smart_intent==SMART_INTENT_LOCAL&&quick_intent(smart_goal,quick_topic(smart_goal))))
        return quick_intent(smart_goal,quick);
    if(command->verb==PART_FOUNDATION&&ax_kind(command->arg[0])==AX_DELETE_COMPONENT&&!ax_delete_preview(command))
        return smart_explicit_component_delete(smart_goal,command->arg[2]);
    if(command->verb==PART_FOUNDATION&&!wcscmp(command->arg[0],L"FEATURE_DELETE"))
        return smart_explicit_feature_delete(smart_goal,command->arg[2]);
    return 1;
}
static const wchar_t *smart_delete_block_message=L"删除特征需要在本轮明确写出“删除”和精确特征名称。当前零件未提交删除。";
static const wchar_t *smart_write_block_message=L"本轮仅执行你要求的查看或分析。";
static const wchar_t *smart_operation_block(void){
    return smart_readonly?(smart_explicit_readonly(smart_goal)?smart_write_block_message:
        L"AI 返回的操作与本轮目标判定冲突，Creo 未执行写入。"):
        smart_intent==SMART_INTENT_NEW?L"另建任务需要先创建独立结果。当前指令会写入源件或重复另建，已拦截。":
        L"这条操作与当前模型任务的目标不一致，尚未执行。";
}
static int smart_handle_stale_permission(const wchar_t *reply){
    const wchar_t *stale[]={L"宿主标记为只读",L"宿主仍标记",L"宿主仍将本轮标记为只读",L"本轮写入权限为只读",L"本轮仍被宿主标记为只读"};
    if(!smart_has_text(reply,stale,_countof(stale)))return 0;
    if(smart_readonly){smart_reply(smart_write_block_message);return 1;}
    if(!smart_stale_permission_retry){
        smart_stale_permission_retry=1;
        smart_queue_next();
        return 1;
    }
    smart_pause(L"AI 没有返回可执行的 Creo 操作，建模任务暂停",1);
    return 1;
}
#endif
