#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES
#include <ProCore.h>

static int restart_failures;
static int restart_checks;
static int restart_reuse;
static FILE *restart_log;

static void restart_check(int result,const wchar_t *description){
    fwprintf(restart_log,L"%ls [%ls]\n",description,result?L"PASS":L"FAIL");
    fflush(restart_log);
    restart_checks++;
    restart_failures+=!result;
}

static int restart_finish(void){
    fwprintf(restart_log,L"METHOD_DONE=%d CHECKS=%d PID=%lu\n",restart_failures,restart_checks,GetCurrentProcessId());
    fclose(restart_log);
    if(native_backend_window)KillTimer(native_backend_window,6481);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static ProError restart_assembly(int reuse,ProMdl *assembly,int *component,ProMdlName assembly_name){
    const char *stage="";
    model_background_mode=1;
    ProMdl child=NULL;
    ProMdlName child_name=L"BWTL_DEMO_PANEL";
    if(reuse)wcscpy_s(child_name,_countof(child_name),L"BWTL_REUSE_PANEL");
    const ModelPlan block={.a=24,.b=18,.height=6,.ready=1};
    ProError error=quick_seed(child_name,&child);
    if(!error)error=model_build_internal(&block,child_name,&stage,child,NULL,NULL);
    if(!error)error=batch_step(child,restart_log,L"CREO_OP DATUM_CSYS %ls AL_INTERFACE CSYS:PRT_CSYS_DEF %d 0 0 0 0 0",child_name,reuse?5:2);
    model_background_mode=!reuse;
    if(!error)error=model_create_empty(3,assembly_name,&stage);
    if(!error)error=ProMdlnameInit(assembly_name,PRO_MDL_ASSEMBLY,assembly);
    ProFeature datum;
    if(!error)error=ux_named_feature(*assembly,L"ASM_CSYS",&datum);
    if(!error)error=ProModelitemNameSet(&datum,L"AL_LOCATOR");
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{32,0,0,1}};
    ProAsmcomp occurrence;
    if(!error)error=ProAsmcompAssemble((ProAssembly)*assembly,(ProSolid)child,pose,&occurrence);
    if(!error)error=part_regenerate(*assembly);
    if(!error)*component=occurrence.id;
    return error;
}

static ProError restart_place(ProMdl assembly,const wchar_t *name,int id){
    PartCommand command;
    wchar_t line[320],report[PART_TEXT_MAX]=L"";
    swprintf_s(line,_countof(line),L"CREO_OP ASM_CSYS %ls ID:%d CSYS:AL_LOCATOR CSYS:AL_INTERFACE",name,id);
    ProError error=part_command_parse(line,&command)==1?ax_execute(assembly,&command,report,_countof(report)):PRO_TK_BAD_INPUTS;
    fwprintf(restart_log,L"ASM_CSYS=%d %ls\n",error,report);
    return error;
}

static ProError restart_sheet(ProMdl *sheet,ProMdlName name,int length,int width,int gauge,int radius){
    ProError error=batch_empty(2,sheet,name);
    if(!error)error=batch_step(*sheet,restart_log,L"CREO_OP SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 %d %d",name,length,width);
    if(!error)error=batch_step(*sheet,restart_log,L"CREO_OP SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE %d POS",name,gauge);
    if(!error)error=batch_step(*sheet,restart_log,L"CREO_OP SMT_SKETCH %ls PROFILE PLANE:FRONT PLANE:RIGHT CIRCLE 40 25 %d",name,radius);
    if(!error)error=batch_step(*sheet,restart_log,L"CREO_OP SMT_CUT %ls FINISH SKETCH:PROFILE NEG",name);
    return error;
}

static int restart_capture(void){
    ProMdl assembly=NULL,sheet=NULL;
    ProMdlName assembly_name,part_name;
    int component;
    ProError error=restart_assembly(0,&assembly,&component,assembly_name);
    if(!error)error=restart_place(assembly,assembly_name,component);
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText receipt={report,_countof(report),0};
    ProWVerstamp before=NULL,after=NULL;
    if(!error)error=ProMdlVerstampGet(assembly,&before);
    if(!error)error=ax_learning_save(assembly,&receipt);
    if(!error)error=ProMdlVerstampGet(assembly,&after);
    restart_check(!error&&before&&after&&ProVerstampEqual(before,after)&&wcsstr(report,L".bwtlasm.txt"),
        L"装配学习自动写盘并保持原件不变");
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    fwprintf(restart_log,L"ASSEMBLY_SOURCE %ls\n%ls\n",error?L"":assembly_name,report);

    report[0]=0;receipt.truncated=0;
    ProPath delivered=L"";
    error=restart_sheet(&sheet,part_name,80,60,2,3);
    if(!error)error=imitation_run_mode(sheet,delivered,0,1,&receipt);
    restart_check(!error&&delivered[0]&&wcsstr(report,L".bwtlmethod.txt")&&!method_last.saved[0],
        L"钣金严格验收并自动写盘，无需手动点击保存");
    fwprintf(restart_log,L"SHEET_ERROR=%d\n%ls\n",error,report);
    return restart_finish();
}

static ProMdl restart_target;
static ProMdlName restart_target_name;
static int restart_occurrence, restart_chats=-1, restart_queued;
static ULONGLONG restart_started;

static int restart_chat_tick(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    restart_queued=0;
    if(restart_chats<0){
        ProMdl active=NULL;
        if(ProMdlCurrentGet(&active)||active!=restart_target){
            if(GetTickCount64()-restart_started<20000)return 0;
            restart_check(0,L"独立 Creo 的新总装已激活");
            return restart_finish();
        }
        restart_chats=0;
        restart_started=GetTickCount64();
        native_prompt_set(L"使用上次学过的装配方法，把当前总装里的新组件 BWTL_REUSE_PANEL "
            L"按已学坐标系定位法装上。这个组件替代了样件里的板件，请读取已学案例使用的那两个坐标系，"
            L"在当前总装和新组件上核对后直接定位。本次无需保存。");
        run_local(NULL,NULL,NULL);
        return 0;
    }
    if(smart_active||ai_process){
        if(GetTickCount64()-restart_started<180000)return 0;
        restart_check(0,L"Codex 请求在限时内返回");
        return restart_finish();
    }
    if(!restart_chats){
        wchar_t instance[36];
        swprintf_s(instance,_countof(instance),L"ID:%d",restart_occurrence);
        AxComponent component={0};
        restart_check(wcsstr(smart_details,L"ASM_CSYS ")&&wcsstr(smart_details,L"AL_INTERFACE")&&
            !ax_component_resolve(restart_target,instance,&component)&&
            aw_aligned(restart_target,&component,L"CSYS:AL_LOCATOR",L"CSYS:AL_INTERFACE",1),
            L"从旧案例找出两端坐标系，并给换过的组件建立原生约束");
        fwprintf(restart_log,L"%ls\n",smart_details);
        restart_chats++;
        restart_started=GetTickCount64();
        native_prompt_set(L"按你学过的钣金首壁与独立切口建模方法，在空白新钣金件上实际画一块矩形板。"
            L"长110毫米，宽70毫米，板厚3毫米，左下角在草绘(0,0)。圆形贯穿切口直径10毫米，"
            L"圆心在同一草绘坐标(40,25)，请用 FRONT、RIGHT 作为两个草绘的基准，"
            L"首壁 POS，切口 NEG。先做板，再做可单独编辑的切口。直接执行，本次无需保存。");
        run_local(NULL,NULL,NULL);
        return 0;
    }
    ProMdl actual=NULL;
    ProMdlsubtype subtype;
    double length_scale=0, thickness=0, dimensions[3]={0};
    ProMassProperty mass;
    const int good=!ProMdlCurrentGet(&actual)&&actual!=restart_target&&
        !ProMdlSubtypeGet(actual,&subtype)&&subtype==PROMDLSTYPE_PART_SHEETMETAL&&
        !fab_thickness(actual,&length_scale,&thickness)&&!imitation_extents(actual,length_scale,dimensions)&&
        !qa_geometry_mass(actual,&mass)&&fabs(thickness-3)<1e-5&&
        fabs(fmax(dimensions[0],dimensions[1])-110)<1e-5&&
        fabs(fmin(dimensions[0],dimensions[1])-70)<1e-5&&fabs(dimensions[2]-3)<1e-5&&
        fabs(mass.volume*length_scale*length_scale*length_scale-(110*70-25*AS_PI)*3)<1e-3;
    ImitationDiameterRead hole={length_scale,6,10,0,0};
    const int diameter=good&&!ProSolidSurfaceVisit((ProSolid)actual,imitation_cylinder_diameter,NULL,&hole)&&
        hole.new_count>0&&!hole.old_count;
    restart_check(good&&diameter&&smart_progress>=4&&wcsstr(smart_details,L"SMT_FIRST_WALL")&&
        wcsstr(smart_details,L"SMT_CUT"),L"按已学步骤新画 110×70×3 mm 钣金，Ø10 mm 原生贯穿切口");
    fwprintf(restart_log,L"VOLUME=%g STEPS=%d WRITES=%d\n%ls\n",
        good?mass.volume:0,smart_steps,smart_progress,smart_details);
    return restart_finish();
}

static void CALLBACK restart_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!restart_queued)restart_queued=!ProMacroLoad(L"~ Command \x0060" L"BWTLMethodChatTick\x0060;");
}

static int restart_run_reuse(void){
    FILE *prompt=_wfopen(L"memory-preview.txt",L"wt, ccs=UTF-8");
    int assembly=0,part=0;
    if(prompt){
        assembly=ax_learning_prompt(prompt,L"按上次学的装配方法",NULL);
        part=method_prompt_write(prompt,L"按之前学过的钣金建模方法做首壁和切口",L'M');
        fclose(prompt);
    }
    restart_check(assembly==1&&part==1&&!method_last.saved[0],L"空会话从磁盘重新加载两种学习文件");
    if(!assembly||!part)return restart_finish();

    model_background_mode=0;
    ProError error=restart_assembly(1,&restart_target,&restart_occurrence,restart_target_name);
    wchar_t exe[MAX_PATH];
    if(!error&&(!ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
       !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)))
        error=PRO_TK_E_NOT_FOUND;
    restart_check(!error&&native_ui_open_threaded(NULL)&&
        SetTimer(native_backend_window,6481,650,restart_timer),L"新进程准备不同组件和真实 Codex 对话");
    if(restart_failures)return restart_finish();
    restart_started=GetTickCount64();
    return 0;
}

static int restart_start(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    restart_log=_wfopen(L"method-restart.log",L"wt, ccs=UTF-8");
    if(!restart_log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    return restart_reuse?restart_run_reuse():restart_capture();
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    restart_reuse=GetEnvironmentVariableW(L"BWTL_TEST_REUSE",NULL,0)>0;
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLMethodRestart",restart_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLMethodChatTick",restart_chat_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLMethodRestart\x0060;");
}
