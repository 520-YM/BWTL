#define BWTL_REUSE_MACHINING_FIXTURE
#define BWTL_DELIVERY_FAULT_TEST
#include "native_machining.c"
#include <ProUICmd.h>

static void dv_test_check(int passed,const char *label){
    cases++;
    if(!passed)failures++;
    trace(label,passed?0:-1);
}

static ProError dv_test_run(ProMdl model,const wchar_t *protocol,int reject,const wchar_t *expected){
    PartCommand command;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    wchar_t report[PART_TEXT_MAX]=L"";
    ProError error=PRO_TK_OUT_OF_MEMORY;
    if(snapshot){
        error=model?part_snapshot_take_mode(model,snapshot,0):ProDirectoryCurrentGet(snapshot->directory);
        if(!error){
            if(part_command_parse(protocol,&command)!=1)error=PRO_TK_BAD_INPUTS;
            else error=part_execute(&command,snapshot,report,_countof(report));
        }
    }
    int pass=(reject?error!=0:error==0)&&(!expected||wcsstr(report,expected));
    cases++;
    if(!pass)failures++;
    FILE *log=_wfopen(L"delivery-cases.log",L"a, ccs=UTF-8");
    if(log){
        fwprintf(log,L"%ls\nRESULT=%d PASS=%d EXPECTED=%ls\n%ls\n",
            protocol,error,pass,expected?expected:L"",report);
        fclose(log);
    }
    if(snapshot){part_snapshot_clear(snapshot);free(snapshot);}
    return error;
}

static ProError dv_test_active(ProMdl model){
    int window;
    ProError error=ProMdlWindowGet(model,&window);
    if(!error)error=ProWindowCurrentSet(window);
    return error;
}

static ProError dv_test_break_parent(ProMdl model,ProFeature *feature){
    int *ids=NULL;
    ProFeatureDeleteOptions *options=NULL;
    ProError error=ProArrayAlloc(1,sizeof(*ids),1,(ProArray *)&ids);
    if(!error)error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        ids[0]=feature->id;
        options[0]=PRO_FEAT_DELETE_NO_OPTS;
        error=ProFeatureWithoptionsDelete((ProSolid)model,ids,options,PRO_REGEN_NO_RESOLVE_MODE);
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(options)ProArrayFree((ProArray *)&options);
    return error;
}

static int dv_test_parameter(ProMdl model,wchar_t *key,const wchar_t *expected){
    ProModelitem owner;
    ProParameter parameter;
    ProParamvalue value;
    if(ProMdlToModelitem(model,&owner))return 0;
    ProError error=ProParameterInit(&owner,key,&parameter);
    if(!expected)return error==PRO_TK_E_NOT_FOUND;
    return !error&&!ProParameterValueWithUnitsGet(&parameter,&value,NULL)&&value.type==PRO_PARAM_STRING&&
        !wcscmp(value.value.s_val,expected);
}

static ProError dv_test_find_import(ProFeature *feature,ProError status,ProAppData context){
    if(status)return status;
    ProFeattype type;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(error)return error;
    if(type==PRO_FEAT_IMPORT)*(ProFeature *)context=*feature;
    return PRO_TK_NO_ERROR;
}

static int dv_test_native_file(ProMdl source,const wchar_t *folder){
    ProPath working,path,leaf;
    ProMdlName original,copy;
    ProMdl reopened=NULL;
    if(ProDirectoryCurrentGet(working)||ProMdlMdlnameGet(source,original))return 0;
    swprintf_s(copy,_countof(copy),L"%.22ls_DELIVERY",original);
    swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",working,folder);
    if(dv_native_revision(path,copy,leaf))return 0;
    swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls\\%ls",working,folder,leaf);
    int pass=!ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&reopened)&&
        reopened&&reopened!=source&&!part_health(reopened)&&part_close(volume(reopened),volume(source));
    if(reopened)ProMdlErase(reopened);
    return pass;
}

static int dv_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL,other=NULL,imported=NULL,surface_only=NULL;
    ProMdlName name,other_name;
    Fixture f;
    wchar_t text[1300];
    model_background_mode=0;
#define NEW() do{ProError e=fixture(&model,name,&f);if(!e)e=dv_test_active(model);if(e){trace("delivery fixture",e);failures++;goto done;}}while(0)
#define BUILD(format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),run(model,text,0))
#define CHECK(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),dv_test_run(model,text,reject,expected))
    NEW();
    NEW();
    BUILD(L"DATUM_PLANE %ls REF_A PLANE:RIGHT 5",name);
    BUILD(L"DATUM_PLANE %ls REF_B PLANE:RIGHT 10",name);
    BUILD(L"DATUM_PLANE %ls DEPENDENT PLANE:REF_A 3",name);
    CHECK(0,L"实际顺序",L"DV_REORDER %ls REF_B BEFORE REF_A",name);
    CHECK(1,L"未完成",L"DV_REORDER %ls REF_A AFTER DEPENDENT",name);
    CHECK(0,L"偏移从 5 改为 7",L"DV_REDEFINE %ls REF_A OFFSET 5 7",name);
    CHECK(1,L"未完成",L"DV_REDEFINE %ls REF_A OFFSET 5 9",name);
    FdRef a={0};
    if(fd_ref_resolve(model,L"PLANE:REF_A",PRO_SURFACE,&a)){failures++;goto done;}
    CHECK(1,L"未完成",L"DV_REF %ls DEPENDENT SURFACE:%d PLANE:DEPENDENT",name,a.item.id);
    CHECK(0,L"保持 3 mm 偏移",L"DV_REF %ls DEPENDENT SURFACE:%d PLANE:REF_B",name,a.item.id);
    fd_ref_free(&a);
    CHECK(1,L"未完成",L"DV_REF %ls DEPENDENT MISSING PLANE:REF_A",name);
    BUILD(L"DATUM_PLANE %ls BOSS_Z PLANE:FRONT 20",name);
    BUILD(L"SKETCH_CREATE %ls BOSS_SK PLANE:BOSS_Z PLANE:RIGHT RECT 20 20 12 14",name);
    BUILD(L"SOLID_EXTRUDE %ls BOSS BOSS_SK ADD BLIND 8 NEG",name);
    double before=volume(model);
    CHECK(0,L"盲深从 8 改为 12",L"DV_REDEFINE %ls BOSS DEPTH 8 12",name);
    dv_test_check(part_close(volume(model)-before,12*14*4),"native blind redefinition actual volume");

    other=model;
    wcscpy_s(other_name,_countof(other_name),name);
    NEW();
    BUILD(L"HOLE %ls SOURCE_HOLE SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 8 THROUGH 0",name,f.top,f.x,f.y);
    CHECK(0,L"填写 4 项",L"DV_ATTR %ls 1 %ls PART_CODE BWTL_TEST ORDER_NOTE \"Sample part\"",name,other_name);
    dv_test_check(dv_test_parameter(model,L"PART_CODE",L"BWTL_TEST")&&dv_test_parameter(other,L"ORDER_NOTE",L"Sample part"),
        "batch attribute readback in both parts");
    CHECK(0,L"同值保持 2 项",L"DV_ATTR %ls 1 %ls PART_CODE BWTL_TEST",name,other_name);
    CHECK(1,L"预检未通过",L"DV_ATTR %ls 1 %ls NEW_FIELD NEW PART_CODE OTHER",name,other_name);
    dv_test_check(dv_test_parameter(model,L"NEW_FIELD",NULL)&&dv_test_parameter(other,L"NEW_FIELD",NULL),
        "attribute conflict leaves entire batch unchanged");
    dv_test_attribute_fail_at=2;
    CHECK(1,L"全部回滚",L"DV_ATTR %ls 1 %ls TX_FIRST X TX_SECOND Y",name,other_name);
    dv_test_attribute_fail_at=-1;
    dv_test_check(dv_test_parameter(model,L"TX_FIRST",NULL)&&dv_test_parameter(model,L"TX_SECOND",NULL)&&
        dv_test_parameter(other,L"TX_FIRST",NULL)&&dv_test_parameter(other,L"TX_SECOND",NULL),
        "partial attribute write rolled back across entire batch");

    ProWVerstamp stamp=NULL,after=NULL;
    ProMdlVerstampGet(model,&stamp);
    CHECK(0,L"2 个零件的 4 个",L"DV_EXPORT %ls BATCH_OUT BOTH %ls",name,other_name);
    CHECK(1,L"未完成",L"DV_EXPORT %ls BATCH_OUT STEP",name);
    CHECK(1,L"未完成",L"DV_EXPORT %ls NEVER_PUBLISHED STEP UNKNOWN_DV_PART",name);
    CHECK(0,L"加工信息表",L"DV_REPORT %ls REPORT_OUT",name);
    CHECK(0,L"SHA-256 清单",L"DV_PACKAGE %ls DELIVERY_OUT",name);
    CHECK(1,L"未完成",L"DV_PACKAGE %ls DELIVERY_OUT",name);
    dv_test_check(!ProMdlVerstampGet(model,&after)&&stamp&&ProVerstampEqual(stamp,after),"file delivery preserves source model stamp");
    if(stamp)ProVerstampFree(&stamp);
    if(after)ProVerstampFree(&after);
    dv_test_check(dv_test_native_file(model,L"DELIVERY_OUT"),"delivered native PRT reopens and matches source volume");
    ProPath step,iges;
    swprintf_s(step,PRO_PATH_SIZE,L"BATCH_OUT\\%ls.stp",name);
    swprintf_s(iges,PRO_PATH_SIZE,L"BATCH_OUT\\%ls.igs",name);
    if(!CopyFileW(step,L"source.stp",TRUE)||!CopyFileW(iges,L"source.igs",TRUE)){
        trace("copy neutral fixtures",(int)GetLastError());failures++;goto done;
    }

    double expected_volume=volume(model);
    CHECK(0,L"导入独立零件",L"DV_IMPORT %ls %ls",L"IMPORTED_STEP",L"source.stp");
    if(ProMdlnameInit(L"IMPORTED_STEP",PRO_MDL_PART,&imported)||dv_test_active(imported)){failures++;goto done;}
    model=imported;
    ProMdlMdlnameGet(model,name);
    double scale=0;
    qa_mm_per_unit(model,&scale);
    dv_test_check(part_close(volume(model)*scale*scale*scale,expected_volume),"STEP round trip real solid volume");
    ProFeature import_feature={0};
    ProName import_name;
    ProSolidFeatVisit((ProSolid)model,dv_test_find_import,NULL,&import_feature);
    qa_feature_label(&import_feature,import_name);
    CHECK(0,L"通孔候选",L"DV_RECOGNIZE %ls %ls",name,import_name);
    CHECK(1,L"已有有效实体",L"DV_HEAL %ls %ls %ls",name,L"NO_HEAL",L"source.stp");

    CHECK(0,L"导入独立零件",L"DV_IMPORT %ls %ls",L"IMPORTED_IGES",L"source.igs");
    if(ProMdlnameInit(L"IMPORTED_IGES",PRO_MDL_PART,&imported)||dv_test_active(imported)){failures++;goto done;}
    model=imported;
    ProMdlMdlnameGet(model,name);
    CHECK(1,L"未完成",L"DV_IMPORT %ls %ls",L"IMPORTED_IGES",L"source.igs");
    CHECK(1,L"未完成",L"DV_IMPORT %ls %ls",L"MISSING_FILE",L"absent.stp");

    ProPath path;
    if(dv_work_file(L"source.stp",path)||dv_import_feature(path,L"QUILT_IMPORT",0,&surface_only)||
       dv_display_new(surface_only)||dv_test_active(surface_only)){
        trace("create native quilt-only import fixture",-1);failures++;goto done;
    }
    model=surface_only;
    ProMdlMdlnameGet(model,name);
    int count=-1;
    dv_test_check(!sd_body_count(model,&count)&&count==0,"repair source is a surface-only imported part");
    CHECK(0,L"生成修复副本",L"DV_HEAL %ls %ls %ls",name,L"HEALED_SOLID",L"source.stp");
    dv_test_check(!sd_body_count(model,&count)&&count==0,"heal leaves surface-only source unchanged");

    NEW();
    BUILD(L"DATUM_PLANE %ls KEEP_PARENT PLANE:RIGHT 9",name);
    BUILD(L"DATUM_PLANE %ls LOSE_PARENT PLANE:RIGHT 5",name);
    BUILD(L"DATUM_PLANE %ls BROKEN_PLANE PLANE:LOSE_PARENT 4",name);
    ProFeature lost,broken;
    unsigned flags=0;
    if(!ux_named_feature(model,L"LOSE_PARENT",&lost)&&!ux_named_feature(model,L"BROKEN_PLANE",&broken)){
        trace("remove fixture parent without dependent clipping",dv_test_break_parent(model,&lost));
        ProFeatureStatusflagsGet(&broken,&flags);
        trace("broken fixture feature flags",(int)flags);
    }
    dv_test_check((flags&PRO_FEAT_STAT_FAILED)!=0,"fixture has an actual failed datum feature");
    CHECK(0,L"保持 4 mm 偏移",L"DV_REF %ls BROKEN_PLANE MISSING PLANE:KEEP_PARENT",name);
    dv_test_check(!part_health(model),"repaired model regenerates and has no failed features");

done:
    trace("DELIVERY_CASES",cases);
    trace("DELIVERY_FAILURES",failures);
    trace("DELIVERY_DONE",1);
    ProEngineerEnd();
    return 0;
#undef NEW
#undef BUILD
#undef CHECK
}

static int dv_test_blank(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl current=NULL,imported=NULL;
    ProMdlCurrentGet(&current);
    dv_test_check(!current,"blank import starts without active model");
    dv_test_run(NULL,L"CREO_OP DV_IMPORT BLANK_IMPORTED source.stp",0,L"导入独立零件");
    ProMassProperty mass;
    dv_test_check(!ProMdlnameInit(L"BLANK_IMPORTED",PRO_MDL_PART,&imported)&&!qa_geometry_mass(imported,&mass),
        "blank import produces real solid geometry");
    trace("DELIVERY_BLANK_CASES",cases);
    trace("DELIVERY_BLANK_FAILURES",failures);
    trace("DELIVERY_BLANK_DONE",1);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))
        return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))
        return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    trace("legacy_bridge_loaded",0);
    uiCmdCmdId id;
    int blank=GetEnvironmentVariableA("BWTL_DELIVERY_BLANK",marker,sizeof(marker))==3&&!strcmp(marker,"yes");
    ProError error=ProCmdActionAdd("BWTLDeliveryProbe",blank?dv_test_blank:dv_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLDeliveryProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
