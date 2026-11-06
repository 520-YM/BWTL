/* Run only in the host's independent disposable Creo test session.
   Creates an ordinary temporary part; never saves or accesses user models. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <ProToolkit.h>
#include <ProCore.h>
#include <ProUtil.h>
#include <ProUICmd.h>
#include <ProMenu.h>
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){return _wfopen(name,mode);}
static void trace(const char *label,int value){FILE *f=fopen("development-result.log","a");if(f){fprintf(f,"%s: %d\n",label,value);fclose(f);}}
#include "../src/native_model.h"
#include "../src/part_command.h"
#define PART_TEXT_MAX 28000
#define NATIVE_IMAGE_MAX 4
typedef struct {wchar_t path[MAX_PATH],name[100];HBITMAP thumbnail;} NativeImage;
static int native_mode,store_ready,queued,busy,replies;
static HANDLE ai_process;
static PartCommand pending_part;
static ModelPlan pending_plan;
static int native_images_snapshot(NativeImage *images){return 0;}
static void native_set_busy(int value){busy=value;}
static void append_card(const wchar_t *role,const wchar_t *text){replies++;}
static void save_turn(const wchar_t *role,const wchar_t *text){}
#include "../src/功能/chat_store/store_tail_add.inc"
static void ai_cleanup(void){ai_process=NULL;}
static void native_stop_poll(void){}
static int store_write(const wchar_t *name,const wchar_t *text){return 1;}
/* Intercept only continuation scheduling, so the boundary probe never starts AI. */
static ProError development_queue(const wchar_t *macro){queued++;return PRO_TK_NO_ERROR;}
#define ProMacroLoad development_queue
#include "../src/smart_session.h"
#undef ProMacroLoad
static void ai_send(const wchar_t *text){}
static int failures;
static void check(const char *label,int ok){trace(label,ok?0:1);if(!ok)failures++;}
static int development_run(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdlName name;const char *stage="create disposable ordinary part";ProError error;int i;
    model_background_mode=1;
    error=model_create_empty(1,name,&stage);
    trace(stage,error);check("independent ordinary test model created",error==PRO_TK_NO_ERROR);
    check("configured step limit is 100",SMART_MAX_STEPS==100);
    native_mode=0;smart_begin(L"新建独立测试零件，不保存");queued=replies=0;
    for(i=0;i<99&&smart_active;i++)smart_after_tool(PRO_TK_NO_ERROR,L"测试回执：检查通过",0);
    check("step 99 still active",smart_active&&busy&&smart_steps==99&&queued==99&&replies==0);
    if(smart_active)smart_after_tool(PRO_TK_NO_ERROR,L"测试回执：检查通过",0);
    check("step 100 stops without scheduling 101",!smart_active&&!busy&&smart_steps==100&&queued==99&&replies==1);
    smart_begin(L"修改测试零件，不保存");queued=0;
    smart_after_tool(-1,L"测试失败，已恢复",0);
    check("first failure permits continuation",smart_active&&smart_failures==1&&queued==1);
    smart_after_tool(-1,L"测试失败，已恢复",0);
    check("second failure without progress stops",!smart_active&&smart_failures==2&&queued==1);
    smart_begin(L"创建独立测试零件，不保存");queued=0;
    smart_after_tool(-1,L"测试基准失败，已恢复",0);
    smart_after_tool(PRO_TK_NO_ERROR,L"读取实际参照",0);
    check("read does not reset failure budget",smart_active&&smart_failures_without_progress==1);
    smart_after_tool(PRO_TK_NO_ERROR,L"替代基准创建成功",1);
    smart_after_tool(-1,L"测试草绘失败，已恢复",0);
    check("verified model progress permits alternative sketch",smart_active&&smart_failures==2&&smart_failures_without_progress==1&&queued==4);
    smart_after_tool(PRO_TK_NO_ERROR,L"替代草绘成功",1);
    smart_after_tool(-1,L"测试造型失败，已恢复",0);
    smart_after_tool(PRO_TK_NO_ERROR,L"替代造型成功",1);
    smart_after_tool(-1,L"后续测试操作失败，已恢复",0);
    check("total failure limit stops despite progress",!smart_active&&smart_failures==SMART_MAX_FAILURES&&queued==7);
    smart_begin(L"修改测试零件，不保存");queued=0;smart_started=GetTickCount64()-1200001;
    smart_after_tool(PRO_TK_NO_ERROR,L"测试回执：检查通过",0);
    check("existing task timeout retained",!smart_active&&queued==0);
    smart_begin(L"修改测试零件，不保存");queued=0;
    smart_after_tool(-1,L"恢复未能核实",0);
    check("uncertain recovery stops immediately",!smart_active&&queued==0);
    smart_cancel();trace("DEVELOPMENT_FAILURES",failures);trace("DEVELOPMENT_DONE",1);ProEngineerEnd();return 0;
}
static uiCmdAccessState development_access(uiCmdAccessMode mode){return ACCESS_AVAILABLE;}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId command;ProError error;FILE *f=fopen("development-result.log","w");if(f)fclose(f);
    error=ProCmdActionAdd("BWTLDevelopment100Steps",development_run,uiProeImmediate,development_access,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLDevelopment100Steps`;");
    if(error){trace("initialization error",error);trace("DEVELOPMENT_FAILURES",1);trace("DEVELOPMENT_DONE",1);ProEngineerEnd();}
    return error;
}
__declspec(dllexport) void user_terminate(void){}
