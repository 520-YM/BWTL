#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "../src/part_command.h"
#include "../src/model_plan.h"
#define PART_TEXT_MAX 28000
typedef void *ProMdl;typedef int ProError;typedef int uiCmdCmdId;typedef int uiCmdValue;
typedef struct {wchar_t path[MAX_PATH],name[100];HBITMAP thumbnail;} NativeImage;
#define NATIVE_IMAGE_MAX 4
static int native_mode,store_ready,queued,busy;static HANDLE ai_process;
static int replies,saved;
static PartCommand pending_part;static ModelPlan pending_plan;
static int native_images_snapshot(NativeImage *images){return 0;}
static void native_set_busy(int value){busy=value;}
static void append_card(const wchar_t *role,const wchar_t *s){replies++;}
static void save_turn(const wchar_t *role,const wchar_t *s){saved++;}
#include "../src/功能/chat_store/store_tail_add.inc"
static void ai_cleanup(void){ai_process=NULL;}
static void native_stop_poll(void){}
static int store_write(const wchar_t *name,const wchar_t *text){return 1;}
static int ProMacroLoad(const wchar_t *macro){queued++;return 0;}
#include "../src/smart_session.h"
static void ai_send(const wchar_t *text){}
int main(void){
    int i;PartCommand write={0};assert(part_command_parse(L"CREO_OP EDGE_ROUND PRT1 R1 2 EDGE:1",&write)==1);
    native_mode=0;smart_begin(L"把当前零件倒圆角R2");assert(smart_active&&!smart_readonly&&busy);
    smart_target_model=(void *)1;smart_waiting_for_user=1;smart_begin(L"可以");assert(!smart_readonly&&smart_target_model==(void *)1&&wcsstr(smart_goal,L"倒圆角R2"));
    smart_waiting_for_user=1;smart_begin(L"半径3毫米");assert(!smart_readonly&&wcsstr(smart_goal,L"3毫米"));
    smart_waiting_for_user=1;smart_begin(L"只分析3毫米，不执行");assert(smart_readonly);
    smart_cancel();assert(!busy&&!smart_active&&!smart_protocol_allowed(&write)&&!smart_waiting_for_user);
    native_mode=1;smart_begin(L"请修改当前模型");smart_waiting_for_user=1;native_mode=0;smart_begin(L"可以");assert(smart_readonly);smart_cancel();
    smart_begin(L"新建一个方块");queued=0;for(i=0;i<SMART_MAX_STEPS;i++)smart_after_tool(0,L"原生检查通过",0);assert(!smart_active&&queued==SMART_MAX_STEPS-1);
    smart_begin(L"倒圆角R2");smart_after_tool(-1,L"失败并已恢复",0);assert(smart_active);smart_after_tool(-1,L"失败并已恢复",0);assert(!smart_active);
    smart_begin(L"倒圆角R2");smart_after_tool(-1,L"恢复未能核实",0);assert(!smart_active);
    smart_begin(L"创建测试件并加把手");
    smart_after_tool(-1,L"基准操作失败，已恢复",0);
    smart_after_tool(0,L"读取参照",0);
    assert(smart_active&&smart_failures_without_progress==1);
    smart_after_tool(0,L"改用新参照后建成基准面",1);
    smart_after_tool(-1,L"草绘失败，已恢复",0);
    assert(smart_active&&smart_failures==2&&smart_failures_without_progress==1);
    smart_after_tool(0,L"替代草绘成功",1);
    smart_after_tool(-1,L"第一次造型失败，已恢复",0);
    smart_after_tool(0,L"替代造型成功",1);
    smart_after_tool(-1,L"下一步失败，已恢复",0);
    assert(!smart_active&&smart_failures==SMART_MAX_FAILURES&&smart_failures_without_progress==1);
    smart_begin(L"修改测试件");smart_after_tool(-1,L"失败，回滚未能验证",0);assert(!smart_active);
    assert(smart_reply_needs_input(L"请确认是否使用正向？"));assert(!smart_reply_needs_input(L"已完成，需要其他帮助吗？"));
    assert(smart_answer_state(L"请告诉我孔径。\nBWTL_STATE WAIT")==1);assert(smart_answer_state(L"已创建。还需帮助吗？\nBWTL_STATE DONE")==2);
    replies=saved=0;smart_begin(L"新建方块，再倒圆角R2");
    smart_after_tool(0,L"已创建方块",0);
    smart_after_tool(0,L"已完成圆角",0);
    assert(!replies&&!saved&&smart_active);
    assert(wcsstr(smart_details,L"已创建方块")&&wcsstr(smart_details,L"已完成圆角"));
    smart_reply(L"方块及圆角已完成。");
    assert(replies==1&&saved==1&&!smart_active);
    smart_begin(L"创建空零件");smart_local_plan=1;smart_after_tool(0,L"已创建空零件",0);
    assert(replies==2&&saved==2&&!smart_active);
    native_mode=1;smart_begin(L"先分析水杯方案");smart_reply(L"水杯方案已给出。");
    native_mode=0;smart_begin(L"切换到智能协作模式，帮我建模");
    assert(smart_request_mode==0&&!smart_readonly&&smart_protocol_allowed(&write));
    smart_cancel();
    native_mode=1;smart_begin(L"切换到智能协作模式，帮我建模");
    assert(smart_request_mode==1&&smart_readonly&&!smart_protocol_allowed(&write));
    smart_cancel();
    native_mode=0;
    smart_begin(L"现在可以帮我做水杯了吗？你自己去选择调用功能、自己有想法去做、尺寸你自己给。");
    assert(smart_request_mode==0&&!smart_readonly&&smart_protocol_allowed(&write));
    smart_cancel();
    puts("SMART SESSION PASS: contextual confirmation, read-only and cancellation guards, bounded continuation, and one saved reply after multiple receipts or direct creation.");return 0;
}
