#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static const wchar_t *const requests[]={L"加个靠背",L"在凳子的基础上画个靠背",L"只给方案，看看靠背改高一点会怎样，别修改模型。",L"上回说的那个面加上吧"};
static int phase,checks,failures,tick_queued,launched;
static ULONGLONG phase_started;
static ProMdl fixture;
static ProWVerstamp old_stamp;
static double stock_volume;
static void check(int ok,const char *label){
    checks++;if(!ok)failures++;
    FILE *f=fopen("semantic-native.log","a");
    if(f){fprintf(f,"%s: %d\n",label,ok?0:-1);fclose(f);}
}
static int finish(void){
    KillTimer(native_backend_window,5410);
    if(old_stamp)ProVerstampFree(&old_stamp);
    smart_cancel();
    FILE *f=fopen("semantic-native.log","a");
    if(f){fprintf(f,"SEMANTIC_CASES: %d\nSEMANTIC_FAILURES: %d\nSEMANTIC_DONE: 1\n",checks,failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}
static void CALLBACK timer(HWND window,UINT message,UINT_PTR id,DWORD time){
    if(!tick_queued)tick_queued=ProMacroLoad(L"~ Command `BWTLSemanticTick`;")==PRO_TK_NO_ERROR;
}
static int prepare_stool(void){
    PartCommand cmd;
    static PartSnapshot snap;
    wchar_t protocol[230],result[PART_TEXT_MAX];
    swprintf_s(protocol,_countof(protocol),L"CREO_OP QUICK_STOOL SEM_STOOL_%d 320 320 25 450 35 25",phase);
    if(ProDirectoryCurrentGet(snap.directory)||part_command_parse(protocol,&cmd)!=1||
       part_execute(&cmd,&snap,result,_countof(result)))return 0;
    if(ProMdlnameInit(cmd.arg[1],PRO_MDL_PART,&fixture))return 0;
    int window;
    if(ProMdlWindowGet(fixture,&window)||ProWindowCurrentSet(window))return 0;
    ProMassProperty mass;
    if(qa_geometry_mass(fixture,&mass))return 0;
    stock_volume=mass.volume;
    if(phase==1){
        wcscpy_s(conversation,_countof(conversation),L"用户：画个凳子，尺寸自己给。助手：刚已在 Creo 建好四方腿凳，凳面320×320 mm，厚25 mm。用户：靠背我想用宽270 mm、厚20 mm、高出凳面300 mm的直板，后侧居中。助手：已记录这个靠背方案，尚未添加。\n");
    }else{
        wcscpy_s(conversation,_countof(conversation),L"用户：加个靠背。助手：已约定宽270 mm、厚20 mm、高出凳面300 mm的直板，凳面后侧居中。上一轮只给了方案，靠背还没有添加。\n");
    }
    return 1;
}
static void launch_request(void){
    history[0]=0;
    native_prompt_set(requests[phase-1]);
    phase_started=GetTickCount64();
    run_local(NULL,NULL,NULL);
}
static int tick(uiCmdCmdId id,uiCmdValue *value,void *data){
    tick_queued=0;
    if(!launched){
        ProMdl active=NULL;
        if(ProMdlCurrentGet(&active)||active!=fixture){
            if(GetTickCount64()-phase_started<12000)return 0;
            check(0,"wait for fixture to become the active model");
            return finish();
        }
        launched=1;
        launch_request();
        if(phase==1)check(smart_intent==SMART_INTENT_PENDING&&request_snapshot.model==fixture,
            "the host uses full-sentence intent and the active Creo model");
        return 0;
    }
    if(smart_active||ai_process){
        if(GetTickCount64()-phase_started<185000)return 0;
        check(0,"real semantic model timed out");
        return finish();
    }
    ProMdl now=NULL;
    ProMdlCurrentGet(&now);
    if(phase==4){
        ProFeature feature;
        ProFeattype type;
        int plane=now==fixture&&!ux_named_feature(now,L"EXTRA_PLANE",&feature)&&
            !ProFeatureTypeGet(&feature,&type)&&type==PRO_FEAT_DATUM&&!part_health(now);
        check(smart_intent==SMART_INTENT_CURRENT,"an indirect reference to a prior plan is understood as modification");
        check(plane,"generic native datum command created the agreed feature on the same model");
        const wchar_t *card=wcsstr(history,L"【AI】");
        check(card&&!wcsstr(card+4,L"【AI】"),"generic contextual modeling also returns one result card");
        return finish();
    }
    if(phase<=2){
        ProFeature feature;
        ProMassProperty mass;
        ProSolidBody body;
        int built=now==fixture&&!ux_named_feature(now,L"BWTL_STOOL_BACK",&feature)&&
            !qa_geometry_mass(now,&mass)&&fabs(mass.volume-stock_volume-270*20*300)<0.1;
        check(smart_intent==SMART_INTENT_CURRENT&&!smart_readonly,"whole follow-up is classified as modification");
        check(built,"backrest added to same native model with prior dimensions");
        check(built&&!qa_single_body(now,&body)&&!part_health(now)&&!ux_named_feature(now,L"QP_STOOL_LEGS",&feature),
            "original legs and healthy single solid are retained");
        const wchar_t *card=wcsstr(history,L"【AI】");
        check(card&&!wcsstr(card+4,L"【AI】")&&wcsstr(card,L"已在当前凳子"),"one final result card for actual modification");
        if(!built)return finish();
        if(++phase==2){
            if(!prepare_stool()){check(0,"prepare second independent fixture");return finish();}
            launched=0;phase_started=GetTickCount64();
            return 0;
        }
        if(ProMdlVerstampGet(now,&old_stamp)){check(0,"capture pre-discussion model stamp");return finish();}
        launch_request();
        return 0;
    }
    ProWVerstamp after=NULL;
    int same=now==fixture&&!ProMdlVerstampGet(now,&after)&&ProVerstampEqual(old_stamp,after);
    check(smart_intent==SMART_INTENT_READ&&smart_readonly&&same,"explicit analysis request preserves actual model version");
    if(after)ProVerstampFree(&after);
    if(!same)return finish();
    phase=4;
    wcscpy_s(conversation,_countof(conversation),L"用户：准备在当前零件创建一个名为 EXTRA_PLANE 的基准面，平行 FRONT，沿基准法向偏移15毫米。助手：已确定这个方案，尚未创建。你可以让我按此方案创建。\n");
    launch_request();
    return 0;
}
static int start(uiCmdCmdId id,uiCmdValue *value,void *data){
    wchar_t codex[MAX_PATH];
    phase=1;
    int ready=native_ui_open_threaded(NULL)&&ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",codex,_countof(codex))&&
        runtime_exe_exists(codex)&&WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings);
    check(ready,"start isolated real Codex and native chat");
    if(!ready)return finish();
    int prepared=prepare_stool();
    check(prepared,"create disposable four-leg stool for follow-up tests");
    if(!prepared||!SetTimer(native_backend_window,5410,1000,timer))return finish();
    phase_started=GetTickCount64();
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError e=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(e)return e;
    uiCmdCmdId cmd;
    e=ProCmdActionAdd("BWTLSemanticStart",start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!e)e=ProCmdActionAdd("BWTLSemanticTick",tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    return e?e:ProMacroLoad(L"~ Command `BWTLSemanticStart`;");
}
