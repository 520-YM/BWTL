#define BWTL_ORGANIZE_FAULT_TEST
#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate

static ProError organize_read(ProMdl model,const wchar_t *protocol,int reject,const wchar_t *expected){
    ProWVerstamp before=NULL,after=NULL;
    ProError error=ProMdlVerstampGet(model,&before);
    ProError result=panel_command(model,protocol,reject,expected);
    panel_check(!error&&!ProMdlVerstampGet(model,&after)&&ProVerstampEqual(before,after),
        "organize read kept model version stamp");
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    return result;
}

static ProError organize_find_import(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProFeattype type;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error&&type==PRO_FEAT_IMPORT)*(int *)data=feature->id;
    return error;
}

static int organize_feature_id(ProMdl model,const wchar_t *name){
    ProFeature feature;
    return ux_named_feature(model,name,&feature)?0:feature.id;
}

static ProError organize_fixture_feature(ProMdl model,int id,int suppress){
    int *ids=NULL;
    ProFeatureDeleteOptions *options=NULL;
    ProError error=ProArrayAlloc(1,sizeof(*ids),1,(ProArray *)&ids);
    if(!error)error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        ids[0]=id;options[0]=PRO_FEAT_DELETE_NO_OPTS;
        error=suppress?ProFeatureWithoptionsSuppress((ProSolid)model,ids,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL):
            ProFeatureWithoptionsDelete((ProSolid)model,ids,options,PRO_REGEN_NO_RESOLVE_MODE);
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(options)ProArrayFree((ProArray *)&options);
    return error;
}

static ProError organize_cut(ProMdl model,int face,const wchar_t *name,PanelProfile p){
    PartCommand context={0};
    ShopSetup setup;
    FdTxn tx={0};
    swprintf_s(context.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",face);
    wcscpy_s(context.arg[4],PART_TOKEN_MAX,L"CSYS:ORG_FACE");
    wcscpy_s(context.arg[5],PART_TOKEN_MAX,L"PLANE:RIGHT");
    ProError error=fd_begin(&tx,model,name,PRO_FEAT_CUT);
    if(!error)error=mp_setup(&tx,&context,3,&setup);
    if(!error)error=mp_blind(&tx,&setup,L"SK",&p,1,20);
    fd_cleanup(&tx,0);
    return error;
}

static ProError organize_unit_parameter(ProMdl model,wchar_t *unit,double number){
    ProModelitem owner;
    ProUnititem units;
    ProParameter parameter;
    ProParamvalue value={0};
    value.type=PRO_PARAM_DOUBLE;value.value.d_val=number;
    ProError error=ProMdlToModelitem(model,&owner);
    if(!error)error=ProUnitInit(model,unit,&units);
    return error?error:ProParameterWithUnitsCreate(&owner,L"CROSS_UNIT_LENGTH",&value,&units,&parameter);
}

static int organize_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL,other=NULL;
    ProMdlName name,other_name;
    Fixture f,other_f;
    ProError error;
    wchar_t text[1200];
    model_background_mode=0;
    if(fixture(&model,name,&f)){panel_check(0,"warm up Creo model window");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),panel_command(model,text,reject,expected))
#define READ(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP ORG_" format,__VA_ARGS__),organize_read(model,text,reject,expected))
#define MAKE() do{error=fixture(&model,name,&f);if(!error)error=panel_activate(model); \
    if(error){trace("organize fixture error",error);panel_check(0,"create organize fixture");goto done;}}while(0)
#define HOLE(feature,hx,hy,d,mode,deep) CMD(0,NULL,L"HOLE %ls " feature L" SURFACE:%d SURFACE:%d %d SURFACE:%d %d %d " mode L" %d",name,f.top,f.x,hx,f.y,hy,d,deep)
    MAKE();
    CMD(0,NULL,L"DATUM_CSYS %ls CHECK_AT CSYS:PRT_CSYS_DEF 10 5 0 0 0 0",name);
    HOLE(L"HOLE_A",20,20,8,L"THROUGH",0);
    HOLE(L"HOLE_B",54,22,8,L"THROUGH",0);
    HOLE(L"BLIND_A",40,42,6,L"BLIND",6);
    READ(0,L"(10, 15, 0)｜(10, 15, 20)｜Ø8",L"HOLES %ls CSYS:CHECK_AT 100",name);
    READ(0,L"共核实 3 个完整圆柱孔段，列出 2 个",L"HOLES %ls MODEL 2",name);
    READ(0,L"分为 2 组",L"SPECS %ls 0.01 0.01",name);
    READ(0,L"直径 8–8 mm｜轴向跨度 20–20 mm｜2 个",L"SPECS %ls 0.01 0.01",name);
    READ(0,L"安装平面候选 1 个",L"PLANES %ls +Z 3000 6",name);
    READ(0,L"重叠候选 0 对",L"OVERLAP %ls 0.001 20",name);
    READ(1,L"未完成",L"SLOTS %ls HOLE_A SURFACE:%d SURFACE:%d 1.5",name,f.top,f.bottom);

    MAKE();
    CMD(0,NULL,L"DATUM_PLANE %ls PARENT_A PLANE:RIGHT 5",name);
    CMD(0,NULL,L"DATUM_PLANE %ls CHILD_A PLANE:PARENT_A 4",name);
    CMD(0,NULL,L"DATUM_PLANE %ls FREE_B PLANE:RIGHT 15",name);
    CMD(0,NULL,L"DATUM_PLANE %ls UNTOUCHED PLANE:RIGHT 20",name);
    int a=organize_feature_id(model,L"PARENT_A"),b=organize_feature_id(model,L"FREE_B");
    READ(0,L"FREE_B｜本零件原生树未查到下游引用",L"UNUSED %ls",name);
    READ(0,L"直接依赖 1 个",L"IMPACT %ls ID:%d DELETE 3",name,a);
    CMD(1,L"未完成",L"ORG_RENAME %ls 1 ID:%d PARENT_A UNTOUCHED",name,a);
    CMD(1,L"未完成",L"ORG_RENAME %ls 1 ID:%d WRONG_NAME MOVED",name,a);
    org_test_rename_fault=1;
    CMD(1,L"整组原名称已恢复",L"ORG_RENAME %ls 2 ID:%d PARENT_A FREE_B ID:%d FREE_B PARENT_A",name,a,b);
    org_test_rename_fault=0;
    panel_check(org_test_rename_fault_hit&&organize_feature_id(model,L"PARENT_A")==a&&organize_feature_id(model,L"FREE_B")==b,
        "swap failure rolled back both original names");
    CMD(0,L"已重命名 2 个特征",L"ORG_RENAME %ls 2 ID:%d PARENT_A FREE_B ID:%d FREE_B PARENT_A",name,a,b);
    panel_check(organize_feature_id(model,L"FREE_B")==a&&organize_feature_id(model,L"PARENT_A")==b,
        "name swap preserved both feature identities");
    READ(0,L"CHILD_A",L"IMPACT %ls FREE_B EDIT 2",name);

    MAKE();
    HOLE(L"SUPPRESSED_HOLE",40,30,8,L"THROUGH",0);
    error=organize_fixture_feature(model,organize_feature_id(model,L"SUPPRESSED_HOLE"),1);
    panel_check(!error,"create actual suppressed feature");
    READ(0,L"SUPPRESSED_HOLE｜已抑制",L"STATUS %ls",name);
    CMD(0,NULL,L"DATUM_PLANE %ls LOSE_PARENT PLANE:RIGHT 5",name);
    CMD(0,NULL,L"DATUM_PLANE %ls BROKEN_PLANE PLANE:LOSE_PARENT 4",name);
    organize_fixture_feature(model,organize_feature_id(model,L"LOSE_PARENT"),0);
    panel_check(part_health(model)!=PRO_TK_NO_ERROR,"create actual failed datum fixture");
    READ(0,L"BROKEN_PLANE｜失败",L"STATUS %ls",name);

    MAKE();
    CMD(0,NULL,L"DATUM_CSYS %ls ORG_FACE CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name);
    error=organize_cut(model,f.top,L"CUT_A",mp_circle(as_point(35,28),12));
    if(!error)error=organize_cut(model,f.top,L"CUT_B",mp_circle(as_point(43,28),12));
    panel_check(!error,"create two intersecting cylindrical openings");
    if(!error)READ(0,L"圆柱孔段重叠候选 1 对",L"OVERLAP %ls 0.001 20",name);

    MAKE();
    CMD(0,NULL,L"REUSE_PARAM %ls SPEC_CODE INTEGER 6",name);
    CMD(0,NULL,L"REUSE_PARAM %ls OLD_ONLY STRING AA",name);
    CMD(0,NULL,L"REUSE_PARAM %ls LENGTH_SPEC REAL 20 MM",name);
    panel_check(!organize_unit_parameter(model,L"mm",25.4),"create comparison parameter in millimetres");
    ProMdl base=model;
    error=fixture(&other,other_name,&other_f);
    if(!error)error=panel_activate(other);
    if(error){panel_check(0,"make loaded comparison part");goto done;}
    model=other;
    CMD(0,NULL,L"REUSE_PARAM %ls SPEC_CODE INTEGER 8",other_name);
    CMD(0,NULL,L"REUSE_PARAM %ls NEW_ONLY STRING BB",other_name);
    CMD(0,NULL,L"REUSE_PARAM %ls LENGTH_SPEC REAL 20 MM",other_name);
    error=organize_unit_parameter(model,L"in",1);
    if(error==PRO_TK_E_NOT_FOUND)error=organize_unit_parameter(model,L"cm",2.54);
    panel_check(!error,"create equivalent comparison parameter in another unit");
    model=base;
    if(panel_activate(model)){panel_check(0,"activate parameter source");goto done;}
    READ(0,L"SPEC_CODE｜6 → 8",L"PARAMS %ls %ls",name,other_name);
    READ(0,L"OLD_ONLY｜AA → 缺少",L"PARAMS %ls %ls",name,other_name);
    READ(0,L"NEW_ONLY｜缺少 → BB",L"PARAMS %ls %ls",name,other_name);
    READ(0,L"修改 2，新增 1，缺少 1，待复核 0",L"PARAMS %ls %ls",name,other_name);
    READ(1,L"未完成",L"PARAMS %ls NEVER_LOADED",name);

    MAKE();
    CMD(0,NULL,L"DATUM_CSYS %ls ORG_FACE CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name);
    CMD(0,NULL,L"VENT_SLOT %ls SLOT_ROUND SURFACE:%d CSYS:ORG_FACE PLANE:RIGHT 25 30 20 1 1 1 1 18 6",name,f.top);
    CMD(0,NULL,L"VENT_GRID %ls SLOT_RECT SURFACE:%d CSYS:ORG_FACE PLANE:RIGHT 55 30 20 1 1 1 1 20 5",name,f.top);
    HOLE(L"ROUND_IGNORE",55,45,6,L"THROUGH",0);
    if(CMD(0,L"STEP导出成功",L"STEP %ls organization_slots",name))goto done;
    if(CMD(0,L"导入独立零件",L"DV_IMPORT %ls organization_slots.stp",L"ORG_IMPORTED"))goto done;
    error=ProMdlnameInit(L"ORG_IMPORTED",PRO_MDL_PART,&model);
    if(!error)error=panel_activate(model);
    if(!error)error=ProMdlMdlnameGet(model,name);
    if(error){panel_check(0,"activate actual STEP import");goto done;}
    int import_id=0;
    memset(&f,0,sizeof(f));
    PickFinal surfaces={model,&f};
    error=ProSolidFeatVisit((ProSolid)model,organize_find_import,NULL,&import_id);
    if(!error)error=ProSolidSurfaceVisit((ProSolid)model,pick_final_surface,NULL,&surfaces);
    panel_check(!error&&import_id&&f.top&&f.bottom,"find actual imported front, back and feature");
    if(!error&&import_id&&f.top&&f.bottom){
        READ(0,L"识别到 2 个贯穿槽",L"SLOTS %ls ID:%d SURFACE:%d SURFACE:%d 1.5",name,import_id,f.top,f.bottom);
        READ(0,L"圆端贯穿槽｜18 × 6 mm",L"SLOTS %ls ID:%d SURFACE:%d SURFACE:%d 1.5",name,import_id,f.top,f.bottom);
        READ(0,L"矩形贯穿槽｜20 × 5 mm",L"SLOTS %ls ID:%d SURFACE:%d SURFACE:%d 1.5",name,import_id,f.top,f.bottom);
    }
#undef HOLE
#undef MAKE
#undef READ
#undef CMD
done:
    trace("ORGANIZE_CASES",cases);
    trace("ORGANIZE_FAILURES",failures);
    trace("ORGANIZE_DONE",1);
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
    ProError error=ProCmdActionAdd("BWTLOrganizeProbe",organize_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLOrganizeProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
