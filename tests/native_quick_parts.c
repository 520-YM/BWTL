#define BWTL_QUICK_FAULT_TEST
#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate

static ProError quick_fixture_run(const wchar_t *protocol,const wchar_t *expected,int reject,PartSnapshot *snapshot,PartCommand *cmd){
    wchar_t result[PART_TEXT_MAX]=L"";
    ProError error=part_command_parse(protocol,cmd)==1?part_execute(cmd,snapshot,result,_countof(result)):PRO_TK_BAD_INPUTS;
    int ok=(reject?error!=0:error==0)&&(!expected||wcsstr(result,expected));
    panel_check(ok,"quick creation via normal native dispatch");
    FILE *file=_wfopen(L"quick-cases.log",L"a, ccs=UTF-8");
    if(file){fwprintf(file,L"%ls\nRESULT=%d PASS=%d\n%ls\n",protocol,error,ok,result);fclose(file);}
    return error;
}

static int quick_fixture_no_file(const wchar_t *name){
    ProPath work,pattern;
    WIN32_FIND_DATAW data;
    if(ProDirectoryCurrentGet(work)||swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.prt*",work,name)<0)return 0;
    HANDLE find=FindFirstFileW(pattern,&data);
    if(find==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
    FindClose(find);
    return 0;
}

static int quick_fixture_run_all(uiCmdCmdId id,uiCmdValue *value,void *data){
    static PartSnapshot snapshot;
    ProPath work;
    PartCommand cmd;
    ProMdl original=NULL;
    ProMdlName name;
    const char *stage="quick warmup";
    Fixture template_fixture;
    model_background_mode=0;
    ProError error=ProDirectoryCurrentGet(snapshot.directory);
    if(error){panel_check(0,"read blank session directory");goto done;}
    ProMdl blank=NULL;
    ProMdlCurrentGet(&blank);
    panel_check(!blank,"blank Creo session has no active model");
    error=quick_fixture_run(L"CREO_OP QUICK_HINGE qp_blank 70 36 4 45 20 5",L"已创建铰链垫板",0,&snapshot,&cmd);
    if(!error)error=ProMdlnameInit(L"qp_blank",PRO_MDL_PART,&blank);
    panel_check(!error&&blank&&!qa_single_body(blank,NULL),"create a native part starting from a blank Creo session");
    int blank_window;
    panel_check(blank&&!ProMdlWindowGet(blank,&blank_window),"new part has a graphics window");
    panel_check(quick_fixture_no_file(L"qp_blank"),"blank session quick part was not saved automatically");
    error=model_create(&(ModelPlan){0,80,60,20,0,1},name,&stage);
    if(!error)error=fixture(&original,name,&template_fixture);
    if(!error)error=panel_activate(original);
    if(!error)error=ProDirectoryCurrentGet(work);
    panel_check(!error,"warm up native model and bind work directory");
    if(error)goto done;
    wcscpy_s(snapshot.directory,_countof(snapshot.directory),work);
    ProWVerstamp before=NULL,after=NULL;
    error=ProMdlVerstampGet(original,&before);
    panel_check(!error,"capture existing model stamp");

    struct QuickSample {const wchar_t *protocol,*receipt;int bends,bores;double volume;} samples[]={
        {L"CREO_OP QUICK_L qp_l 80 60 2 18 2 44 6",L"1 道 90° 折弯",1,2,0},
        {L"CREO_OP QUICK_U qp_u 90 64 2 20 2 48 6",L"2 道 90° 折弯",2,2,0},
        {L"CREO_OP QUICK_POWER qp_power 120 85 2 22 2 80 48 5",L"4 处开孔",2,4,0},
        {L"CREO_OP QUICK_SCREEN qp_screen 120 90 3 70 40 100 68 5",L"5 处开孔",0,4,(120*90-70*40-25*AS_PI)*3},
        {L"CREO_OP QUICK_HINGE qp_hinge 70 36 4 45 20 5",L"4 处开孔",0,4,(70*36-25*AS_PI)*4},
        {L"CREO_OP QUICK_LOCK qp_lock 70 38 3 19 4 12 16",L"3 处开孔",0,2,(70*38-16*12-8*AS_PI)*3},
        {L"CREO_OP QUICK_CLIP qp_clip 54 34 1.5 11 1.5 27 4",L"2 道 90° 折弯",2,2,0},
        {L"CREO_OP QUICK_SPACER qp_spacer 34 24 12 6 10",L"同轴通孔 Ø12 mm",0,1,AS_PI*((289-36)*6+(144-36)*10)},
        {L"CREO_OP QUICK_STOP qp_stop 80 40 8 28 7 16",L"2 处开孔",0,0,(80*40-2*(21*7+12.25*AS_PI))*8},
        {L"CREO_OP QUICK_COVER qp_cover 140 100 3 12 10 5 42 4",L"6 处开孔",0,6,(140*100-33*AS_PI)*3}
    };
    for(int i=0;i<_countof(samples);i++){
        const struct QuickSample *sample=&samples[i];
        if(quick_fixture_run(sample->protocol,sample->receipt,0,&snapshot,&cmd))continue;
        ProMdl model=NULL;
        error=ProMdlnameInit(cmd.arg[1],PRO_MDL_PART,&model);
        panel_check(!error&&model&&model!=original,"created named model distinct from original");
        if(error||!model)continue;
        ProMdlsubtype type=0;
        error=ProMdlSubtypeGet(model,&type);
        panel_check(!error&&(type==PROMDLSTYPE_PART_SHEETMETAL)==(sample->bends>0),"native sheetmetal subtype or ordinary solid subtype");
        panel_check(!qa_single_body(model,NULL)&&!part_health(model),"one healthy native body");
        panel_check(quick_fixture_no_file(cmd.arg[1]),"new model was not saved automatically");
        if(sample->bends){
            ShBendInfo bends={0};
            bends.model=model;
            error=ProSolidSurfaceVisit((ProSolid)model,sh_count_bends,NULL,&bends);
            panel_check(!error&&bends.inside==sample->bends,"exact native formed bend count");
            if(!wcscmp(cmd.arg[1],L"qp_l")){
                FdTxn view={0};view.model=model;view.scale=1;
                int face=0;
                error=quick_top(&view,2,&face);
                PartCommand check={0}; static PartSnapshot sheet_snapshot;
                if(!error)error=panel_activate(model);
                if(!error)error=part_snapshot_take_mode(model,&sheet_snapshot,0);
                wchar_t protocol[250],receipt[PART_TEXT_MAX];
                swprintf_s(protocol,_countof(protocol),L"CREO_OP FAB_DXF_CHECK qp_l SURFACE:%d",face);
                panel_check(!error&&part_command_parse(protocol,&check)==1&&!part_execute(&check,&sheet_snapshot,receipt,_countof(receipt)),
                    "new angle bracket can be checked by native fabrication DXF");
                part_snapshot_clear(&sheet_snapshot);
            }
        }
        if(sample->volume)panel_check(fabs(volume(model)-sample->volume)<0.01,"independent analytic mass volume");
        if(sample->bores){
            OrganizeView view={model,1};
            DeliveryBores *scan=NULL;
            error=org_bores(&view,&scan);
            int holes=0;
            if(!error)for(int j=0;j<scan->count;j++)holes+=!!scan->bores[j].classification;
            panel_check(!error&&holes==sample->bores,"expected count of full cylindrical holes");
            free(scan);
        }
    }
    quick_fixture_run(L"CREO_OP QUICK_HINGE qp_hinge 70 36 4 45 20 5",L"已在会话或工作目录",1,&snapshot,&cmd);
    FILE *disk=_wfopen(L"qp_disk.prt.1",L"wb");
    if(disk){fputs("local collision fixture",disk);fclose(disk);}
    panel_check(disk!=NULL,"set up on-disk collision");
    quick_fixture_run(L"CREO_OP QUICK_HINGE qp_disk 70 36 4 45 20 5",L"已在会话或工作目录",1,&snapshot,&cmd);
    disk=_wfopen(L"qp_disk.prt.1",L"rb");
    panel_check(disk&&fgetc(disk)=='l',"existing file was preserved");
    if(disk)fclose(disk);
    quick_fault_after_stock=1;
    quick_fixture_run(L"CREO_OP QUICK_HINGE qp_fault 70 36 4 45 20 5",L"临时模型已清理",1,&snapshot,&cmd);
    quick_fault_after_stock=0;
    ProMdl fault=NULL;
    panel_check(ProMdlnameInit(L"qp_fault",PRO_MDL_PART,&fault)!=0&&quick_fixture_no_file(L"qp_fault"),
        "failed partially built model was erased without a disk copy");
    ProMdl completed=NULL;
    ProPath after_dir;
    error=ProDirectoryCurrentGet(after_dir);
    panel_check(!error&&!_wcsicmp(after_dir,work),"working directory restored after all quick creation and failure");
    error=ProMdlnameInit(L"qp_hinge",PRO_MDL_PART,&completed);
    if(!error)error=ProMdlSave(completed);
    panel_check(!error&&!quick_fixture_no_file(L"qp_hinge"),"an explicit save writes the completed model to the real Creo work directory");
    wcscpy_s(snapshot.directory,_countof(snapshot.directory),L"C:\\not-the-fixture-directory");
    quick_fixture_run(L"CREO_OP QUICK_HINGE qp_stale 70 36 4 45 20 5",L"工作目录已变化",1,&snapshot,&cmd);
    panel_check(quick_fixture_no_file(L"qp_stale"),"work-directory mismatch creates no output");
    panel_check(before&&!ProMdlVerstampGet(original,&after)&&ProVerstampEqual(before,after),"pre-existing model version stamp preserved");
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);

done:
    trace("QUICK_PARTS_CASES",cases);
    trace("QUICK_PARTS_FAILURES",failures);
    trace("QUICK_PARTS_DONE",1);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    trace("legacy_bridge_loaded",0);
    uiCmdCmdId command;
    ProError error=ProCmdActionAdd("BWTLQuickPartsProbe",quick_fixture_run_all,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLQuickPartsProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
