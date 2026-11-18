#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl assembly,component,plate;
static ProWVerstamp assembly_before,component_before;
static ProAsmcomp occurrence;
static FILE *log_file;
static int failures,phase,tick_scheduled;
static ULONGLONG deadline;

static void sheet_check(int passed,const char *label){
    if(!passed)failures++;
    fprintf(log_file,"%s=%d\n",label,passed);
    fflush(log_file);
}

static int sheet_finish(void){
    if(native_backend_window)KillTimer(native_backend_window,5451);
    if(assembly_before)ProVerstampFree(&assembly_before);
    if(component_before)ProVerstampFree(&component_before);
    if(log_file){
        fprintf(log_file,"NEW_SHEET_CHAT_PASS=%d\n",failures==0);
        fclose(log_file);
    }
    ProEngineerEnd();
    return 0;
}

static void CALLBACK sheet_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!tick_scheduled)tick_scheduled=ProMacroLoad(L"~ Command \x0060" L"BWTLNewSheetTick\x0060;")==PRO_TK_NO_ERROR;
}

static int sheet_stamps_match(void){
    ProWVerstamp after_root=NULL,after_child=NULL;
    int equal=!ProMdlVerstampGet(assembly,&after_root)&&!ProMdlVerstampGet(component,&after_child)&&
        ProVerstampEqual(assembly_before,after_root)&&ProVerstampEqual(component_before,after_child);
    if(after_root)ProVerstampFree(&after_root);
    if(after_child)ProVerstampFree(&after_child);
    return equal;
}

/* Native template copy can create a blank initial revision before modeling. */
static int sheet_initial_revision_only(ProMdl model){
    ProMdlName name;
    ProPath folder,leaf;
    if(ProDirectoryCurrentGet(folder)||ProMdlMdlnameGet(model,name))return 0;
    ProError status=dv_native_revision(folder,name,leaf);
    return status==PRO_TK_E_NOT_FOUND||(!status&&!wcscmp(wcsrchr(leaf,L'.'),L".1"));
}

static int sheet_plate_readback(ProMdl model){
    ProFeature wall,sketch;
    ProFeattype kind;
    ProSolidBody body;
    ProOutline bound;
    ProMassProperty properties;
    double mm,thickness,lengths[3];
    if(!model||model==assembly||model==component||
       part_health(model)||qa_single_body(model,&body)||ProSolidBodyOutlineGet(&body,bound)||
       fab_thickness(model,&mm,&thickness)||qa_geometry_mass(model,&properties)||
       ux_named_feature(model,L"BWTL_PLATE",&wall)||ProFeatureTypeGet(&wall,&kind)||kind!=PRO_FEAT_DATUM_SURF||
       ux_named_feature(model,L"BWTL_PLATE_SK",&sketch)||ProFeatureTypeGet(&sketch,&kind)||kind!=PRO_FEAT_CURVE)
        return 0;
    for(int i=0;i<3;i++)lengths[i]=(bound[1][i]-bound[0][i])*mm;
    for(int i=0;i<3;i++)for(int j=i+1;j<3;j++)if(lengths[j]<lengths[i]){
        double value=lengths[i];lengths[i]=lengths[j];lengths[j]=value;
    }
    fprintf(log_file,"PLATE_DIMENSIONS_MM=%.10g,%.10g,%.10g\n",lengths[2],lengths[1],thickness);
    return part_close(lengths[2],500)&&part_close(lengths[1],200)&&part_close(lengths[0],1)&&
        part_close(thickness,1)&&part_close(properties.volume*mm*mm*mm,100000);
}

static void sheet_submit(const wchar_t *request){
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
}

static int sheet_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active=NULL;
    tick_scheduled=0;
    ProMdlCurrentGet(&active);
    if(GetTickCount64()>deadline){sheet_check(0,"TIMELY_LOCAL_COMPLETION");return sheet_finish();}
    if(smart_active||ai_process)return 0;
    if(phase==0){
        if(active!=assembly)return 0;
        phase=1;
        sheet_submit(L"创建一个钣金平面500*200*1mm");
        return 0;
    }
    if(phase==1){
        if(active==assembly)return 0;
        plate=active;
        sheet_check(sheet_plate_readback(plate)&&sheet_initial_revision_only(plate),"FIRST_NATIVE_PLATE");
        sheet_check(wcsstr(smart_details,L"500 × 200 × 1 mm")&&sheet_stamps_match(),"FIRST_ASSEMBLY_AND_COMPONENT_UNCHANGED");
        int window;
        if(ProMdlWindowGet(assembly,&window)||ProWindowCurrentSet(window)||ProWindowActivate(window)){
            sheet_check(0,"REACTIVATE_ASSEMBLY_FOR_SECOND_REQUEST");return sheet_finish();
        }
        phase=2;
        return 0;
    }
    if(phase==2){
        if(active!=assembly)return 0;
        phase=3;
        const wchar_t *empty=L"另外给我创建一个钣金件。";
        if(model_empty_request(empty)!=2){sheet_check(0,"SECOND_LOCAL_ROUTE");return sheet_finish();}
        sheet_submit(empty);
        return 0;
    }
    if(active==assembly||active==plate)return 0;
    int bodies,children,constraints;
    ProMdlsubtype subtype;
    sheet_check(active&&active!=component&&!ProMdlSubtypeGet(active,&subtype)&&subtype==PROMDLSTYPE_PART_SHEETMETAL&&
        !sd_body_count(active,&bodies)&&bodies==0&&sheet_initial_revision_only(active),"SECOND_INDEPENDENT_EMPTY_SHEET");
    TemplateDatums datums={0};
    children=ProSolidFeatVisit((ProSolid)assembly,template_feature,NULL,&datums)==PRO_TK_NO_ERROR?datums.components:-1;
    sheet_check(sheet_stamps_match()&&children==1&&!ax_constraint_count(&occurrence,&constraints)&&constraints==1,
        "FINAL_ASSEMBLY_AND_COMPONENT_UNCHANGED");
    return sheet_finish();
}

static int sheet_start(uiCmdCmdId command,uiCmdValue *value,void *data){
    log_file=fopen("new-sheet-chat.log","w");
    if(!log_file){ProEngineerEnd();return 0;}
    ShPlateSize dims={0};
    sheet_check(sh_plate_request(L"请创建一块 500 mm × 200 mm × 1 mm 的独立钣金平板。",&dims)&&
        dims.length==500&&dims.width==200&&dims.thickness==1&&
        sh_plate_request(L"新建钣金平板，尺寸：120x45x0.8毫米，别动当前装配。",&dims)&&dims.thickness==0.8,
        "SUPPORTED_DIMENSION_SPELLINGS");
    sheet_check(!sh_plate_request(L"在当前装配里创建一个钣金平面500*200*1mm",&dims)&&
        !sh_plate_request(L"创建一个钣金平面500*200*1mm后在上面开孔",&dims)&&
        !sh_plate_request(L"创建一个钣金平面500*200*1mm？",&dims)&&
        !sh_plate_request(L"创建一个钣金平面500*200*1mm。别建模",&dims)&&
        !sh_plate_request(L"\"创建一个钣金平面500*200*1mm\"",&dims)&&
        !sh_plate_request(L"创建一个钣金平面500*200*NaNmm",&dims),"NON_SIMPLE_REQUESTS_FALL_THROUGH");
    ProMdlName name;
    const char *stage="New sheet chat fixture";
    wchar_t detail[1000]=L"";
    PartText out={detail,_countof(detail),0};
    ProError error=model_create_empty(3,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&assembly);
    if(!error)error=ax_new_part_in(assembly,L"SHEET_CHAT_EXISTING_COMPONENT",0,&component,&occurrence,&out);
    if(!error)error=ProMdlVerstampGet(assembly,&assembly_before);
    if(!error)error=ProMdlVerstampGet(component,&component_before);
    if(error){trace(stage,error);sheet_check(0,"ASSEMBLY_FIXTURE");return sheet_finish();}
    if(!native_ui_open_threaded(NULL)||!SetTimer(native_backend_window,5451,300,sheet_timer)){
        sheet_check(0,"PRIVATE_CHAT_WINDOW");return sheet_finish();
    }
    deadline=GetTickCount64()+24000;
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLNewSheetStart",sheet_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLNewSheetTick",sheet_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLNewSheetStart\x0060;");
}
