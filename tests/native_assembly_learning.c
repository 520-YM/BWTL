#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl sample,target,part,subassembly,spacer;
static ProMdlName source_name,target_name,sub_name;
static int source_id,target_id,sub_id,sub_part_id,cases,failures,polling;
static ULONGLONG started;

static void learning_check(int pass,const char *description){
    cases++;
    failures+=!pass;
    FILE *log=fopen("assembly-learning.log","a");
    if(log){fprintf(log,"%s: %d\n",description,pass?0:-1);fclose(log);}
}

static int learning_end(void){
    if(native_backend_window)KillTimer(native_backend_window,6420);
    smart_cancel();
    FILE *log=fopen("assembly-learning.log","a");
    if(log){
        fprintf(log,"LEARNING_CASES=%d\nLEARNING_FAILURES=%d\nLEARNING_DONE=1\n",cases,failures);
        fclose(log);
    }
    ProEngineerEnd();
    return 0;
}

static ProError add_datum(ProMdl model,const wchar_t *name,const wchar_t *base,const wchar_t *datum,double x){
    wchar_t line[360],report[PART_TEXT_MAX];
    PartCommand command;
    swprintf_s(line,_countof(line),L"CREO_OP DATUM_CSYS %ls %ls CSYS:%ls %.8g 0 0 0 0 0",name,datum,base,x);
    return part_command_parse(line,&command)==1?foundation_execute(&command,model,report,_countof(report)):PRO_TK_BAD_INPUTS;
}

static ProError rename_assembly_datum(ProMdl model,wchar_t *name){
    ProFeature feature;
    ProError error=ux_named_feature(model,L"ASM_CSYS",&feature);
    return error?error:ProModelitemNameSet(&feature,name);
}

static ProError create_assembly(ProMdlName name,ProMdl *model,int background){
    const char *stage="";
    model_background_mode=background;
    ProError error=model_create_empty(3,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,model);
    if(error)trace(stage,error);
    return error;
}

static ProError add_component(ProMdl assembly,ProMdl child,double x,int *id){
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,0,0,1}};
    ProAsmcomp feature;
    ProError error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)child,pose,&feature);
    if(!error)error=part_regenerate(assembly);
    if(!error)*id=feature.id;
    return error;
}

static ProError place_sample(ProMdl assembly,const wchar_t *name,int id,const wchar_t *base){
    wchar_t line[360],report[PART_TEXT_MAX];
    PartCommand command;
    swprintf_s(line,_countof(line),L"CREO_OP ASM_CSYS %ls ID:%d CSYS:%ls CSYS:AL_INTERFACE",name,id,base);
    return part_command_parse(line,&command)==1?ax_execute(assembly,&command,report,_countof(report)):PRO_TK_BAD_INPUTS;
}

static int learning_verify_chat(uiCmdCmdId id,uiCmdValue *value,void *data){
    polling=0;
    if(smart_active||ai_process){
        if(GetTickCount64()-started<105000)return 0;
        learning_check(0,"chat deadline");return learning_end();
    }
    wchar_t path[36];
    swprintf_s(path,_countof(path),L"ID:%d",target_id);
    AxComponent actual={0};
    learning_check(wcsstr(smart_details,L"ASM_CSYS ")&&wcsstr(smart_details,L"AL_LOCATOR")&&
        wcsstr(smart_details,L"AL_INTERFACE"),"chat selected the saved reference names");
    learning_check(target_id!=source_id&&!ax_component_resolve(target,path,&actual)&&
        aw_aligned(target,&actual,L"CSYS:AL_LOCATOR",L"CSYS:AL_INTERFACE",1),
        "native constraint locates a different target assembly");
    return learning_end();
}

static void CALLBACK learning_tick(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!polling)polling=!ProMacroLoad(L"~ Command \x0060" L"BWTLLearningVerifyChat\x0060;");
}

static int learning_dispatch_chat(uiCmdCmdId id,uiCmdValue *value,void *data){
    wchar_t exe[MAX_PATH],request[560];
    if(!ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
       !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)||
       !SetTimer(native_backend_window,6420,400,learning_tick)){
        learning_check(0,"chat configuration");
        return learning_end();
    }
    swprintf_s(request,_countof(request),L"按你从 %ls 学到的装配方法，直接把当前装配的未定位零件 AL_PANEL "
        L"按样件顶层同型号件的原生坐标系约束定位。参照名称请用已学案例里的名称和本轮实际组件核对，做好后不要保存。",source_name);
    started=GetTickCount64();
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
    return 0;
}

static int learning_capture_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(!native_ui_open_threaded(NULL)){learning_check(0,"private chat window");return learning_end();}
    HMENU menu=native_imitation_menu_create();
    wchar_t label[60]=L"";
    if(menu){
        GetMenuStringW(GetSubMenu(menu,6),IMITATION_ASSEMBLY_LEARN,label,_countof(label),MF_BYCOMMAND);
        DestroyMenu(menu);
    }
    learning_check(!wcscmp(label,L"装配学习"),"imitation menu action is visible");
    ProMdl active=NULL;
    ProWVerstamp before=NULL,after=NULL;
    ProMdlCurrentGet(&active);
    learning_check(active==sample&&!ProMdlVerstampGet(sample,&before),"sample active before capture");
    imitation_pending=IMITATION_ASSEMBLY_LEARN;
    imitation_ui_command(0,NULL,NULL);
    learning_check(before&&!ProMdlVerstampGet(sample,&after)&&ProVerstampEqual(before,after),"learning keeps sample modification stamp");
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);

    wchar_t directory[MAX_PATH],glob[MAX_PATH],file_path[MAX_PATH];
    WIN32_FIND_DATAW item;
    HANDLE found=INVALID_HANDLE_VALUE;
    if(ax_learning_directory(directory,0)&&swprintf_s(glob,MAX_PATH,L"%ls\\*%ls",directory,AX_LEARNING_SUFFIX)>=0)
        found=FindFirstFileW(glob,&item);
    int file_ok=found!=INVALID_HANDLE_VALUE&&
        swprintf_s(file_path,MAX_PATH,L"%ls\\%ls",directory,item.cFileName)>=0;
    if(found!=INVALID_HANDLE_VALUE)FindClose(found);
    learning_check(file_ok&&wcsstr(history,L"已将")&&wcsstr(history,L"3 个组件实例"),"menu saved one complete read receipt");
    if(!file_ok)return learning_end();
    FILE *lesson=ax_learning_open(file_path);
    wchar_t content[16000]=L"",line[1400],name[PRO_NAME_SIZE];
    learning_check(lesson&&ax_learning_header(lesson,name)&&!_wcsicmp(name,source_name),"UTF-8 sample case reopened from disk");
    if(lesson){
        while(fgetws(line,_countof(line),lesson)&&wcslen(content)+wcslen(line)<_countof(content))
            wcscat_s(content,_countof(content),line);
        fclose(lesson);
    }
    swprintf_s(line,_countof(line),L"ID:%d/%d",sub_id,sub_part_id);
    learning_check(wcsstr(content,line)&&wcsstr(content,L"AL_NESTED")&&
        wcsstr(content,L"AL_LOCATOR")&&wcsstr(content,L"AL_INTERFACE")&&wcsstr(content,L"坐标系重合"),
        "disk case includes hierarchical instance and its native references");
    learning_check(wcsstr(content,L"总装实测原点")&&wcsstr(content,L"模型树序号 3"),"disk case includes measured pose and tree order");

    FILE *prompt=_wfopen(L"learning-preview.txt",L"w+t, ccs=UTF-8");
    wchar_t request[300];
    swprintf_s(request,_countof(request),L"按 %ls 学到的装配方法定位 AL_PANEL。",source_name);
    int have=prompt&&ax_learning_prompt(prompt,request,L"OTHER_TARGET");
    if(prompt){
        rewind(prompt);
        content[0]=0;
        while(fgetws(line,_countof(line),prompt)&&wcslen(content)+wcslen(line)<_countof(content))
            wcscat_s(content,_countof(content),line);
        fclose(prompt);
    }
    learning_check(have&&wcsstr(content,L"装配学习案例")&&wcsstr(content,L"AL_INTERFACE"),
        "unrelated target can retrieve a named learned assembly");
    prompt=_wfopen(L"learning-subassembly-preview.txt",L"w+t, ccs=UTF-8");
    have=prompt&&ax_learning_prompt(prompt,L"读取当前子装配的定位",sub_name);
    if(prompt){
        rewind(prompt);
        content[0]=0;
        while(fgetws(line,_countof(line),prompt)&&wcslen(content)+wcslen(line)<_countof(content))
            wcscat_s(content,_countof(content),line);
        fclose(prompt);
    }
    const wchar_t *nested=wcsstr(content,L"AL_NESTED");
    const wchar_t *top=wcsstr(content,L"AL_LOCATOR");
    learning_check(have&&nested&&top&&nested<top,"current subassembly automatically brings its own relations first");
    ProMdlName drawing_name=L"LEARNING_TEST_DRW",template_name=L"a4_drawing";
    ProMdlnameShortdata drawing_data={0};
    wcscpy_s(drawing_data.name,_countof(drawing_data.name),source_name);
    wcscpy_s(drawing_data.type,_countof(drawing_data.type),L"asm");
    ProDrawing drawing=NULL;
    ProDwgcreateErrs diagnostics=NULL;
    ProError error=ProDrawingFromTemplateCreate(drawing_name,template_name,&drawing_data,0,&drawing,&diagnostics);
    if(diagnostics)ProDwgcreateErrsFree(&diagnostics);
    trace("learn drawing fixture",error);
    learning_check(!error&&drawing&&ax_learning_drawing_model(drawing)==sample,"Creo drawing resolves its assembly model");
    if(drawing)ProMdlErase((ProMdl)drawing);

    error=create_assembly(target_name,&target,0);
    if(!error)error=rename_assembly_datum(target,L"AL_LOCATOR");
    int spacer_id=0;
    if(!error)error=add_component(target,spacer,-40,&spacer_id);
    if(!error)error=add_component(target,part,40,&target_id);
    if(!error)error=ProMacroLoad(L"~ Command \x0060" L"BWTLLearningDispatchChat\x0060;");
    if(error){trace("learning target fixture",error);learning_check(0,"prepared separate target");return learning_end();}
    return 0;
}

static int learning_prepare(uiCmdCmdId id,uiCmdValue *value,void *data){
    const char *stage="";
    ProMdlName part_name=L"AL_PANEL";
    ModelPlan block={0,20,20,6,0,1};
    model_background_mode=1;
    ProError error=quick_seed(part_name,&part);
    if(!error)error=model_build_internal(&block,part_name,&stage,part,NULL,NULL);
    if(!error)error=add_datum(part,part_name,L"PRT_CSYS_DEF",L"AL_INTERFACE",2);
    ProMdlName spacer_name=L"AL_SPACER";
    if(!error)error=quick_seed(spacer_name,&spacer);
    if(!error)error=model_build_internal(&block,spacer_name,&stage,spacer,NULL,NULL);
    if(!error)error=create_assembly(sub_name,&subassembly,1);
    if(!error)error=rename_assembly_datum(subassembly,L"AL_NESTED");
    if(!error)error=add_component(subassembly,part,16,&sub_part_id);
    if(!error)error=place_sample(subassembly,sub_name,sub_part_id,L"AL_NESTED");
    if(!error)error=create_assembly(source_name,&sample,0);
    if(!error)error=rename_assembly_datum(sample,L"AL_LOCATOR");
    if(!error)error=add_component(sample,part,24,&source_id);
    if(!error)error=place_sample(sample,source_name,source_id,L"AL_LOCATOR");
    if(!error)error=add_component(sample,subassembly,50,&sub_id);
    if(!error)error=ProMacroLoad(L"~ Command \x0060" L"BWTLLearningCaptureTest\x0060;");
    if(error){trace(stage,error);learning_check(0,"prepared assembly learning source");return learning_end();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLLearningPrepare",learning_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLLearningCaptureTest",learning_capture_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLLearningDispatchChat",learning_dispatch_chat,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLLearningVerifyChat",learning_verify_chat,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLLearningPrepare\x0060;");
}
