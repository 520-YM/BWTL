/* Full production assistant in a separate Creo session and separate data.
   Tests real image input, an AI/native multi-step task and host write denial.
   No desktop input, no legacy bridge and no user models. */
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
static int probe_phase,probe_failures,probe_steps,probe_development_cards;
static ULONGLONG probe_started;
static int probe_tick_queued;
static int probe_card_count(const wchar_t *label){
    const wchar_t *p=history;int count=0;
    while((p=wcsstr(p,label))!=NULL){count++;p+=wcslen(label);}
    return count;
}
static void probe_log(const char *label,int value){FILE *f=fopen("smart-native-result.log","a");if(f){fprintf(f,"%s: %d\n",label,value);fclose(f);}}
static void CALLBACK probe_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!probe_tick_queued){probe_tick_queued=1;ProMacroLoad(L"~ Command `BWTLSmartProbeTick`;");}
}
static ProError probe_round_count(ProFeature *f,ProError status,ProAppData data){ProFeattype type;if(status)return status;if(!ProFeatureTypeGet(f,&type)&&type==PRO_FEAT_ROUND)(*(int *)data)++;return 0;}
static int probe_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model;ProMassProperty mass;int rounds=0;wchar_t text[16000];probe_tick_queued=0;
    if(GetTickCount64()-probe_started>720000){probe_failures++;probe_log("probe timeout",1);goto done;}
    if(smart_active||ai_process||development_process)return 0;
    if(probe_phase==1){
        {wchar_t visible[16000];if(!development_read(ai_output,text,16000))probe_failures++;smart_answer_text(text,visible,16000);if(!wcsstr(visible,L"BWTL"))probe_failures++;}
        probe_log("real attached image recognized",wcsstr(text,L"BWTL")?0:-1);
        {int single=probe_card_count(L"【你】")==1&&probe_card_count(L"【AI】")==1;
         if(!single)probe_failures++;probe_log("image turn has one user card and one reply",single?0:-1);}
        native_images_clear();native_mode=0;probe_phase=2;
        ai_send(L"新建一个独立的80×60×20毫米实体长方体，无孔；然后在这个新零件上把任意一条长20毫米的竖直直棱做R2圆角。四条竖棱中任选一条即可，我授权你自行选边，不需要询问。必须完成这两项实际建模任务，不要只读取参照就停止。不保存。");return 0;
    }
    if(probe_phase==2){
        probe_steps=smart_steps;probe_log("continuous task completed steps",probe_steps);
        {int single=probe_card_count(L"【AI】")==2&&!probe_card_count(L"【执行结果】")&&!probe_card_count(L"【提示】");
         if(!single)probe_failures++;probe_log("multi-step turn has one reply and no progress cards",single?0:-1);}
        if(ProMdlCurrentGet(&model)||ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)){probe_failures++;goto done;}
        ProSolidFeatVisit((ProSolid)model,probe_round_count,NULL,&rounds);
        if(rounds!=1||fabs(mass.volume-(96000-20*(4-3.141592653589793)))>0.05||probe_steps<2)probe_failures++;
        probe_log("one actual round feature",rounds==1?0:-1);probe_log("real multi-step final volume",fabs(mass.volume-(96000-20*(4-3.141592653589793)))<=0.05?0:-1);
        {ProWVerstamp before=NULL,after=NULL;PartCommand c;ProMdlName name;wchar_t protocol[256];
            ProMdlMdlnameGet(model,name);ProMdlVerstampGet(model,&before);native_mode=1;smart_begin(L"把当前零件倒角1毫米");
            swprintf_s(protocol,256,L"CREO_OP EDGE_CHAMFER %ls BLOCKED 1 EDGE:102",name);part_command_parse(protocol,&c);pending_part=c;execute_part_command();
            ProMdlVerstampGet(model,&after);if(!before||!after||!ProVerstampEqual(before,after)||smart_active)probe_failures++;
            probe_log("analysis host guard leaves model stamp unchanged",before&&after&&ProVerstampEqual(before,after)?0:-1);
            if(before)ProVerstampFree(&before);if(after)ProVerstampFree(&after);
        }
        {ProMdlName other;const char *stage="target-switch fixture";ProError e;model_background_mode=0;
            e=model_create_empty(1,other,&stage);if(e)probe_failures++;
            smart_begin(L"把当前零件倒圆角R1");smart_target_model=model;
            /* A new window is not activated until this command returns; the
               host guard itself is also covered by stamped native writes. */
            smart_cancel();if(smart_active||smart_protocol_allowed(&pending_part))probe_failures++;
            probe_log("cancelled task refuses stale protocol",smart_active? -1:0);
        }
        if(GetFileAttributesW(L"..\\scripts\\Start-BWTLDevelopment.ps1")!=INVALID_FILE_ATTRIBUTES){
            probe_phase=3;probe_development_cards=probe_card_count(L"【AI】");native_mode=2;
            development_start(L"在BWTL的功能开发模式可以升级哪些内容？请用两句话回答，第一句包含“独立源码副本”。这是咨询，不要修改任何文件。");
            return 0;
        }
        goto done;
    }
    if(probe_phase==3){
        int single=probe_card_count(L"【AI】")==probe_development_cards+1&&
            !wcsstr(history,L"正在建立独立源码副本")&&wcsstr(wcsrchr(history,L'【'),L"独立源码副本");
        if(!single)probe_failures++;
        probe_log("development consultation has one actual answer and no progress cards",single?0:-1);
        goto done;
    }
    return 0;
done:
    KillTimer(native_backend_window,5001);smart_cancel();probe_log("SMART_NATIVE_FAILURES",probe_failures);probe_log("SMART_NATIVE_DONE",1);ProEngineerEnd();return 0;
}
static int probe_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    wchar_t error[256];
    native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);
    if(!native_ui_thread||WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0){probe_failures++;probe_phase=3;return probe_tick(0,NULL,NULL);}
    CloseHandle(native_ui_ready);native_ui_ready=NULL;
    if(!native_image_import(L"C:\\Users\\ADMINI~1\\AppData\\Local\\Temp\\codex-clipboard-847aa8be-600e-481d-adff-a9e0ddf91a1b.png",error,256)){
        probe_failures++;probe_log("image import failed",1);probe_phase=3;return probe_tick(0,NULL,NULL);
    }
    native_mode=1;probe_phase=1;probe_started=GetTickCount64();SetTimer(native_backend_window,5001,1000,probe_timer);
    ai_send(L"请分析已附图片。用中文简短说明窗口标题、有哪些按钮，以及里面的人希望做什么建模任务。只分析图片，不修改模型，也不要调用任何模型操作。");return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError e;uiCmdCmdId command;char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,16)||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    e=bwtl_production_initialize(argc,argv,version,build,error_buffer);if(e)return e;
    if((e=ProCmdActionAdd("BWTLSmartProbeBegin",probe_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command))||
       (e=ProCmdActionAdd("BWTLSmartProbeTick",probe_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command)))return e;
    return ProMacroLoad(L"~ Command `BWTLSmartProbeBegin`;");
}
