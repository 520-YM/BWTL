#ifndef BWTL_SMART_SESSION_H
#define BWTL_SMART_SESSION_H
#include "smart_policy.h"
static int smart_active,smart_followup,smart_readonly,smart_steps,smart_failures,smart_failures_without_progress,smart_repeats;
static wchar_t smart_goal[6001],smart_feedback[PART_TEXT_MAX],smart_details[PART_TEXT_MAX],smart_last_protocol[8192],smart_new_model_result[1024];
static NativeImage smart_images[NATIVE_IMAGE_MAX];static int smart_image_count;
static ULONGLONG smart_started;
static ProMdl smart_target_model;
static int smart_waiting_for_user;
static int smart_request_mode;
static int smart_has_details,smart_local_plan;
static void ai_send(const wchar_t *value);
static void smart_finish(void){smart_active=smart_followup=0;native_set_busy(0);}
static void smart_reply(const wchar_t *text){
    append_card(L"AI",text);
    save_turn(L"助手",text);
    smart_finish();
}
static void smart_begin(const wchar_t *value){
    wchar_t combined[6001];ProMdl prior_target=smart_target_model;
    int supplement;
    smart_request_mode=(int)native_mode;
    supplement=smart_waiting_for_user&&!smart_readonly&&smart_request_mode==0&&!smart_readonly_override(value)&&smart_short_reply(value)&&wcslen(smart_goal)<4000&&wcslen(value)<1500;
    if(supplement){swprintf_s(combined,6001,L"原任务：%ls\n用户补充/确认（以此为准）：%ls",smart_goal,value);value=combined;}
    smart_active=1;smart_followup=0;smart_steps=smart_failures=smart_failures_without_progress=smart_repeats=0;smart_started=GetTickCount64();smart_target_model=NULL;
    smart_has_details=smart_local_plan=0;
    smart_waiting_for_user=0;if(supplement)smart_target_model=prior_target;
    wcsncpy_s(smart_goal,6001,value,_TRUNCATE);smart_feedback[0]=smart_last_protocol[0]=smart_new_model_result[0]=0;
    smart_readonly=supplement?0:smart_analysis_request(value,smart_request_mode);smart_image_count=native_images_snapshot(smart_images);
    native_set_busy(1);
}
static void smart_cancel(void){
    smart_waiting_for_user=0;
    smart_finish();ai_cleanup();native_stop_poll();
    memset(&pending_part,0,sizeof(pending_part));pending_plan.ready=0;
}
static void smart_record_tool(const wchar_t *receipt){
    wcsncpy_s(smart_feedback,PART_TEXT_MAX,receipt,_TRUNCATE);
    if(!smart_has_details)smart_details[0]=0;
    else store_tail_add(smart_details,PART_TEXT_MAX,L"\n\n");
    smart_has_details=1;
    store_tail_add(smart_details,PART_TEXT_MAX,receipt);
    if(store_ready)store_write(L"last-native-details.txt",smart_details);
}
static void smart_after_tool(ProError error,const wchar_t *receipt,int model_progress){
    const wchar_t *stop_reason=NULL;
    if(!smart_active)return;
    smart_record_tool(receipt);
    if(smart_local_plan){smart_reply(receipt);return;}
    smart_steps++;
    if(error){smart_failures++;smart_failures_without_progress++;}
    else if(model_progress)smart_failures_without_progress=0;
    if(wcsstr(receipt,L"恢复未能核实")||wcsstr(receipt,L"恢复状态未能核实")||wcsstr(receipt,L"本次会话已停止加工写入")||wcsstr(receipt,L"回滚未能")||wcsstr(receipt,L"未能完整验证回滚")||wcsstr(receipt,L"恢复原名未成功")){
        smart_reply(receipt);return;
    }
    if(smart_failures_without_progress>=SMART_MAX_FAILURES_WITHOUT_PROGRESS)stop_reason=L"尚未取得新的建模进展，失败次数已达上限";
    else if(smart_failures>=SMART_MAX_FAILURES)stop_reason=L"本轮累计失败次数已达上限";
    else if(smart_steps>=SMART_MAX_STEPS)stop_reason=L"已达到本轮步骤上限";
    else if(GetTickCount64()-smart_started>1200000)stop_reason=L"本轮任务已超时";
    if(stop_reason){
        wchar_t message[1000];
        swprintf_s(message,_countof(message),L"%ls（已执行%d/%d步，累计失败%d次）。\n最近一步：%.600ls",
            stop_reason,smart_steps,SMART_MAX_STEPS,smart_failures,receipt);
        smart_reply(message);return;
    }
    smart_followup=1;
    if(ProMacroLoad(L"~ Command `BWTLSmartContinue`;"))smart_reply(L"无法继续下一步。请查看执行明细核对已完成的操作，再重新发送需求。");
}
static int smart_continue_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    if(smart_active&&smart_followup&&!ai_process)ai_send(smart_goal);return 0;
}
static int smart_protocol_allowed(const PartCommand *command){
    if(!smart_active)return 0;
    if(smart_readonly&&!smart_read_command(command))return 0;
    return 1;
}
static const wchar_t *smart_write_block_message(void){
    return smart_request_mode==1
        ?L"窗口当前为「只分析」，本轮未写入模型。需要建模时，请选择「智能协作」后发送建模要求。"
        :L"窗口当前为「智能协作」，本轮按只读处理，未写入模型。需要建模时，请明确要求创建或修改。";
}
#endif
