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
static int store_ready,queued,busy;static HANDLE ai_process;
static int replies,saved;
static wchar_t shown[PART_TEXT_MAX],logged[PART_TEXT_MAX];
static PartCommand pending_part;static ModelPlan pending_plan;
static int native_images_snapshot(NativeImage *images){return 0;}
static void native_set_busy(int value){busy=value;}
static void append_card(const wchar_t *role,const wchar_t *s){replies++;wcsncpy_s(shown,_countof(shown),s,_TRUNCATE);}
static void save_turn(const wchar_t *role,const wchar_t *s){saved++;wcsncpy_s(logged,_countof(logged),s,_TRUNCATE);}
#include "../src/功能/chat_store/store_tail_add.inc"
static void ai_cleanup(void){ai_process=NULL;}
static void native_stop_poll(void){}
static int store_write(const wchar_t *name,const wchar_t *text){return 1;}
static int ProMacroLoad(const wchar_t *macro){queued++;return 0;}
static int fake_binding=1;
static void smart_binding_clear(void){}
static int smart_binding_valid(void){return fake_binding;}
#include "../src/smart_session.h"
static void ai_send(const wchar_t *text){}
static void test_failure_brief(void){
    PartCommand sketch;
    assert(part_command_parse(L"CREO_OP SMT_SKETCH PRT TestBase PLANE:FRONT PLANE:RIGHT RECT -100 -75 200 150",&sketch)==1);
    const wchar_t *receipt=L"高级加工未完成：regenerate and verify native feature，错误码-1。未新增或已核对移除本次特征，原实体保持；未保存。";
    int count=replies;
    smart_begin(L"试一下钣金草绘");
    smart_after_tool(0,L"已创建空钣金零件",1);
    smart_step(&sketch,sketch.arg[0],-1,receipt,0,0,1);
    const wchar_t *brief=wcsstr(shown,L"\n简单说：");
    assert(replies==count+1&&saved==replies&&!wcscmp(shown,logged));
    assert(brief&&wcsstr(brief,L"画钣金草绘没成功")&&wcsstr(brief,L"重新生成或检查特征")&&
           wcsstr(brief,L"没有留下新特征")&&!wcsstr(brief+1,L"\n简单说："));
    assert(wcsstr(shown,receipt)&&wcsstr(smart_details,receipt)&&!wcsstr(smart_details,L"简单说："));
    assert(shown[wcslen(shown)-1]==L'。');

    smart_begin(L"试一下钣金草绘");
    smart_step(&sketch,sketch.arg[0],-1,L"指定草绘未完成，回滚未能验证。",0,1,0);
    assert(wcsstr(shown,L"没有说明具体原因")&&wcsstr(shown,L"改动是否撤回还没有确认")&&
           !wcsstr(shown,L"没有留下新特征"));

    smart_begin(L"试一下钣金草绘");
    count=replies;
    smart_step(&sketch,sketch.arg[0],-1,L"fd_sketch_setup(tx,cmd->arg[3],cmd->arg[4])",0,0,0);
    assert(replies==count&&smart_active);
    smart_step(&sketch,sketch.arg[0],0,L"更换参照后画好了草绘。",1,1,0);
    assert(replies==count+1&&!wcsstr(shown,L"简单说："));

    smart_begin(L"试一下钣金草绘");
    smart_step(&sketch,sketch.arg[0],-1,L"未执行：活动模型、工作目录或模型版本与请求时不一致。",0,1,0);
    assert(wcsstr(shown,L"与提交时对不上")&&!wcsstr(shown,L"没有说明具体原因"));
    smart_begin(L"试一下钣金草绘");smart_stop();
    assert(!wcsstr(shown,L"简单说："));
}
int main(void){
    test_failure_brief();replies=saved=0;
    int i;PartCommand write={0};assert(part_command_parse(L"CREO_OP EDGE_ROUND PRT1 R1 2 EDGE:1",&write)==1);
    PartCommand copy, read, imitation, replay;
    assert(part_command_parse(L"CREO_OP COPY 222 RESULT",&copy)==1);
    assert(part_command_parse(L"CREO_OP READ 222",&read)==1);
    assert(part_command_parse(L"CREO_OP IMITATE_SIZE 222 600 200 300",&imitation)==1);
    assert(part_command_parse(L"CREO_OP IMITATE 222",&replay)==1);
    smart_begin(L"仿照这个222画一个600*200*300的模型");
    smart_intent=SMART_INTENT_NEW;smart_readonly=0;
    assert(!smart_protocol_allowed(&copy)&&smart_protocol_allowed(&read)&&smart_protocol_allowed(&imitation)&&smart_protocol_allowed(&replay));
    assert(!smart_protocol_allowed(&write));
    smart_intent=SMART_INTENT_CHAT;smart_readonly=1;
    assert(!smart_protocol_allowed(&copy)&&!smart_protocol_allowed(&read)&&!smart_protocol_allowed(&imitation));
    smart_cancel();
    smart_begin(L"把当前零件倒圆角R2");assert(smart_active&&!smart_readonly&&busy);
    smart_target_model=(void *)1;smart_waiting_for_user=1;smart_begin(L"可以");assert(!smart_readonly&&smart_target_model==(void *)1&&wcsstr(smart_goal,L"倒圆角R2"));
    smart_waiting_for_user=1;smart_begin(L"半径3毫米");assert(!smart_readonly&&wcsstr(smart_goal,L"3毫米"));
    smart_waiting_for_user=1;smart_begin(L"只分析3毫米，不执行");assert(smart_readonly);
    smart_cancel();assert(!busy&&!smart_active&&!smart_protocol_allowed(&write)&&!smart_waiting_for_user);
    smart_begin(L"先看看能否修改当前模型，不修改模型");smart_waiting_for_user=1;smart_begin(L"可以");assert(smart_readonly);smart_cancel();
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
    smart_begin(L"先分析水杯方案");smart_reply(L"水杯方案已给出。");
    smart_begin(L"按刚才的方案，帮我建模");
    assert(!smart_readonly&&smart_protocol_allowed(&write));
    smart_cancel();
    smart_begin(L"只分析刚才的方案，先别执行");
    assert(smart_readonly&&!smart_protocol_allowed(&write));
    smart_cancel();
    smart_begin(L"现在可以帮我做水杯了吗？你自己去选择调用功能、自己有想法去做、尺寸你自己给。");
    assert(!smart_readonly&&smart_protocol_allowed(&write));
    smart_cancel();
    queued=replies=saved=0;
    smart_begin(L"刚刚增加了10个功能，演示给我看。");
    assert(smart_active&&!smart_readonly&&smart_protocol_allowed(&write));
    assert(smart_handle_stale_permission(L"当前这轮仍被宿主标记为只读。"));
    assert(smart_active&&smart_followup&&queued==1&&replies==0&&saved==0);
    assert(smart_handle_stale_permission(L"当前这轮仍被宿主标记为只读。"));
    assert(!smart_active&&queued==1&&replies==1&&saved==1);
    smart_cancel();
    PartCommand quick;
    assert(part_command_parse(L"CREO_OP QUICK_HINGE NEW_HINGE 70 36 4 45 20 5",&quick)==1);
    smart_begin(L"新建一个铰链垫板");
    assert(!smart_readonly&&smart_protocol_allowed(&quick)&&!smart_protocol_allowed(&write));
    smart_cancel();
    smart_begin(L"新建一个U形安装支架");
    assert(!smart_protocol_allowed(&quick));
    smart_cancel();
    smart_begin(L"别创建一个铰链垫板");
    assert(!smart_protocol_allowed(&quick));
    smart_cancel();
    smart_begin(L"在当前零件上新建一个铰链垫板");
    assert(!smart_protocol_allowed(&quick));
    smart_cancel();
    smart_begin(L"只分析如何创建铰链垫板");
    assert(!smart_protocol_allowed(&quick));
    smart_cancel();
    smart_begin(L"在当前钣金零件上开第一个按钮孔，然后再开两个按钮孔。");
    assert(smart_chain&&!smart_readonly);
    PartCommand fail,renamed;
    assert(part_command_parse(L"CREO_OP PN_BUTTON PRT FAIL SURFACE:9 CSYS:AT PLANE:RIGHT 12 12 2 6 0 0",&fail)==1);
    assert(part_command_parse(L"CREO_OP PN_BUTTON PRT OTHER SURFACE:9 CSYS:AT PLANE:RIGHT 12 12 2 6 0 0",&renamed)==1);
    smart_step(&write,write.arg[0],0,L"完成第一次建模",1,0,0);
    smart_step(&fail,fail.arg[0],-4,L"错误的入口参照",0,0,0);
    assert(smart_active&&smart_protocol_seen(&renamed)&&smart_total_failures==1);
    smart_step(NULL,L"另一尝试",-4,L"仍无法找到入口",0,0,0);
    assert(!smart_active&&smart_paused&&smart_steps==3&&smart_progress==1);
    fake_binding=0;
    assert(!smart_resume()&&smart_paused);
    fake_binding=1;
    assert(smart_resume()&&smart_active&&smart_steps==3&&smart_failures==0&&smart_total_failures==2);
    assert(smart_protocol_seen(&write)&&smart_protocol_seen(&renamed));
    assert(wcsstr(smart_details,L"第 2 步｜失败")&&wcsstr(smart_details,L"第 3 步｜失败"));
    smart_cancel();
    PartCommand back,save;
    assert(part_command_parse(L"CREO_OP STOOL_BACK PRT 270 20 300",&back)==1);
    assert(part_command_parse(L"CREO_OP SAVE PRT",&save)==1);
    smart_begin(L"在凳子的基础上画个靠背，不保存");
    smart_intent=SMART_INTENT_PENDING;smart_readonly=1;
    assert(!smart_protocol_allowed(&back));
    smart_intent=SMART_INTENT_CURRENT;smart_readonly=0;
    assert(smart_protocol_allowed(&back)&&!smart_protocol_allowed(&quick)&&!smart_protocol_allowed(&save));
    smart_intent=SMART_INTENT_READ;smart_readonly=1;
    assert(!smart_protocol_allowed(&back));
    smart_intent=SMART_INTENT_NEW;smart_readonly=0;
    assert(!smart_protocol_allowed(&back));
    smart_cancel();
    PartCommand chair,stool,import;
    assert(part_command_parse(L"CREO_OP QUICK_CHAIR CHAIR 320 320 25 450 35 25 270 20 300",&chair)==1);
    assert(part_command_parse(L"CREO_OP QUICK_STOOL STOOL 320 320 25 450 35 25",&stool)==1);
    assert(part_command_parse(L"CREO_OP DV_IMPORT IMPORTED source.stp",&import)==1);
    smart_begin(L"画个凳子，尺寸自己给。自己想象。加个靠背");
    smart_intent=SMART_INTENT_NEW;
    assert(smart_protocol_allowed(&chair)&&!smart_protocol_allowed(&stool)&&!smart_protocol_allowed(&import));
    smart_cancel();
    puts("SMART SESSION PASS: contextual confirmation, read-only and cancellation guards, bounded continuation, and one saved reply after multiple receipts or direct creation.");return 0;
}
