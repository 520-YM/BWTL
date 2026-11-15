#define user_initialize workflow_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int failures;
static ProMdl workflow_sheet;
static ProError workflow_sheet_error;
static wchar_t report[9000];
static PartText out={report,_countof(report),0};

static void record(FILE *log,const wchar_t *name,ProError error){
    failures+=error!=PRO_TK_NO_ERROR;
    fwprintf(log,L"%ls=%d\n%ls\n",name,error,report);
    fflush(log);
    report[0]=0;
    out.truncated=0;
}

static ProError apply(ProMdl model,const wchar_t *protocol){
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!snapshot)return PRO_TK_OUT_OF_MEMORY;
    PartCommand command;
    ProError error=part_command_parse(protocol,&command)==1?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    ProMdl current=NULL;
    int window;
    if(!error&&(ProMdlCurrentGet(&current)||current!=model)){
        error=ProMdlWindowGet(model,&window)?dv_display_new(model):ProWindowCurrentSet(window);
        if(!error)error=ProMdlDisplay(model);
    }
    if(error)part_text_add(&out,L"Activate test model: %d\n",error);
    if(!error){
        error=part_snapshot_take_basic(model,snapshot);
        if(error){
            ProMdl current=NULL;
            double unit;
            ProModelitem item;
            ProWVerstamp stamp=NULL;
            ProError get=ProMdlCurrentGet(&current),units=qa_mm_per_unit(model,&unit);
            ProError stamp_error=ProMdlVerstampGet(model,&stamp),item_error=ProMdlToModelitem(model,&item);
            if(stamp)ProVerstampFree(&stamp);
            part_text_add(&out,L"Snapshot: %d current: %d same: %d units: %d stamp: %d item: %d type: %d name: %ls\n",
                error,get,current==model,units,stamp_error,item_error,snapshot->type,snapshot->name);
        }
        if(!error){
            error=ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,part_dimension_visit,NULL,snapshot);
            if(error)part_text_add(&out,L"Visit test dimensions: %d\n",error);
        }
    }
    if(!error)error=command.verb==PART_IMITATE_LOCAL?
        imitation_local_command(&command,snapshot,report,_countof(report)):
        imitation_resize_command(&command,snapshot,report,_countof(report));
    part_snapshot_clear(snapshot);
    free(snapshot);
    return error;
}

static ProError plate(ProMdl *model,ProMdlName name,wchar_t face[32]){
    const char *stage="create test plate";
    ProError error=model_create_empty(2,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,model);
    if(!error)error=imitation_operation(*model,&out,0,
        L"CREO_OP SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT -150 -150 300 300",name);
    if(!error)error=imitation_operation(*model,&out,0,
        L"CREO_OP SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",name);
    ProFeature wall;
    CabinetFace surface;
    if(!error)error=cm_feature_find(*model,L"BASE",&wall);
    if(!error)error=cm_wall_face(&wall,&surface);
    if(!error)swprintf_s(face,32,L"SURFACE:%d",surface.id);
    return error;
}

static int workflow(uiCmdCmdId id,uiCmdValue *v,void *data){
    FILE *log=_wfopen(L"imitation-workflow.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProPath input,output;
    ProMdl fan=NULL,sheet=workflow_sheet,blank=NULL;
    ProMdlName name;
    wchar_t face[32],protocol[512];
    model_background_mode=1;
    ProError error=workflow_sheet_error;
    if(!error)error=apply(sheet,L"CREO_OP IMITATE_SIZE 222 600 200 300 HOLE 100 90");
    record(log,L"RESIZE_XYZ_HOLE",error);

    error=PRO_TK_E_NOT_FOUND;
    if(GetEnvironmentVariableW(L"BWTL_FAN_INPUT",input,_countof(input)))
        error=ProMdlFiletypeLoad(input,PRO_MDLFILE_PART,PRO_B_FALSE,&fan);
    if(!error)error=imitation_diagnose(fan,&out);
    record(log,L"FAN_DIAGNOSE",error);
    error=fan?imitation_run_mode(fan,output,0,1,&out):PRO_TK_E_NOT_FOUND;
    record(log,L"FAN_NATIVE_TRIAL",error);
    if(!error){
        ProMdl copy=NULL;
        error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&copy);
        if(!error)error=imitation_compare(fan,copy,&out);
        if(copy)ProMdlErase(copy);
    }
    record(log,L"FAN_COMPARE",error);
    if(fan)ProMdlErase(fan);

    error=plate(&blank,name,face);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP IMITATE_SIZE %ls 0 0 0 THICK 3",name);
        error=apply(blank,protocol);
    }
    record(log,L"RESIZE_THICKNESS",error);
    error=blank&&sheet?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
    if(!error){
        swprintf_s(protocol,_countof(protocol),
            L"CREO_OP IMITATE_LOCAL %ls 222 444 %ls PLANE:RIGHT 0 0 POS",name,face);
        error=apply(blank,protocol);
    }
    record(log,L"LOCAL_ROUND",error);

    ProMdl window_source=NULL;
    ProMdlName window_name;
    wchar_t window_face[32];
    error=plate(&window_source,window_name,window_face);
    if(!error)error=imitation_operation(window_source,&out,0,
        L"CREO_OP SMT_SKETCH %ls WINDOW_SK %ls PLANE:RIGHT RECT -15 -10 30 20",window_name,window_face);
    if(!error)error=imitation_operation(window_source,&out,0,
        L"CREO_OP SMT_CUT %ls WINDOW_CUT SKETCH:WINDOW_SK POS",window_name);
    ProFeature window_cut;
    if(!error)error=cm_feature_find(window_source,L"WINDOW_CUT",&window_cut);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP IMITATE_LOCAL %ls %ls %d %ls PLANE:RIGHT 0 0 POS",
            name,window_name,window_cut.id,face);
        error=apply(blank,protocol);
    }
    record(log,L"LOCAL_RECTANGLE",error);

    error=PRO_TK_E_NOT_FOUND;
    if(GetEnvironmentVariableW(L"BWTL_FAN_INPUT",input,_countof(input))){
        wchar_t files[8192]={0};
        ProPath directory,second;
        error=ProDirectoryCurrentGet(directory);
        if(!error&&swprintf_s(directory+wcslen(directory),_countof(directory)-wcslen(directory),L"\\batch-input")<0)
            error=PRO_TK_BAD_INPUTS;
        if(!error&&!CreateDirectoryW(directory,NULL))error=PRO_TK_CANT_WRITE;
        if(!error&&swprintf_s(second,_countof(second),L"%ls\\80f-fan.prt.26",directory)<0)error=PRO_TK_BAD_INPUTS;
        if(!error&&!CopyFileW(input,second,TRUE))error=PRO_TK_CANT_WRITE;
        if(!error)error=ProMdlnameBackup(window_source,directory);
        if(!error)error=ProMdlErase(window_source);
        if(!error)error=dv_native_revision(directory,window_name,second);
        if(!error){
            wcscpy_s(files,_countof(files),directory);
            wchar_t *leaf=files+wcslen(files)+1;
            wcscpy_s(leaf,PRO_PATH_SIZE,L"80f-fan.prt.26");
            wcscpy_s(leaf+wcslen(leaf)+1,PRO_PATH_SIZE,second);
            error=imitation_batch_files(files,&out);
        }
    }
    if(!error&&imitation_batch_count!=2)error=PRO_TK_INCOMPLETE;
    record(log,L"BATCH_NATIVE",error);
    for(int i=0;!error&&i<imitation_batch_count;i++)error=imitation_save_library(&imitation_batch[i],&out);
    if(!error){
        ProError duplicate=imitation_save_library(&imitation_batch[0],&out);
        if(duplicate!=PRO_TK_E_FOUND)error=PRO_TK_INCOMPLETE;
    }
    record(log,L"BATCH_LIBRARY",error);
    fwprintf(log,L"FAILURES=%d\n",failures);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

static void CALLBACK workflow_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    KillTimer(window,id);
    ProMacroLoad(L"~ Command `BWTLImitationWorkflow`;");
}

static int workflow_setup(uiCmdCmdId id,uiCmdValue *v,void *data){
    ProPath input;
    workflow_sheet_error=PRO_TK_E_NOT_FOUND;
    if(GetEnvironmentVariableW(L"BWTL_SHEET_INPUT",input,_countof(input)))
        workflow_sheet_error=ProMdlFiletypeLoad(input,PRO_MDLFILE_PART,PRO_B_FALSE,&workflow_sheet);
    if(!workflow_sheet_error)workflow_sheet_error=dv_display_new(workflow_sheet);
    if(!SetTimer(native_backend_window,5735,700,workflow_timer))return workflow(id,v,data);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=workflow_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLImitationWorkflow",workflow,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLImitationSetup",workflow_setup,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLImitationSetup`;");
}
