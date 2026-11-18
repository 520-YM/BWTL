#define user_initialize inventory_toolkit_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *receipt;
static int failures;
static ProMdl source_part,design;
static ProMdlName source_name,design_name;
typedef struct {
    int count,ids[32];
    ProFeattype types[32];
} FixtureFeatures;
static FixtureFeatures original_features;
static ProMassProperty original_mass;
static ProOutline original_outline;

static ProError collect_features(ProFeature *feature,ProError status,ProAppData data){
    FixtureFeatures *features=data;
    if(status)return status;
    if(features->count>=_countof(features->ids))return PRO_TK_OUT_OF_RANGE;
    int i=features->count++;
    features->ids[i]=feature->id;
    return ProFeatureTypeGet(feature,&features->types[i]);
}
static ProPath directory;

static void verify(int pass,const char *label){
    fprintf(receipt,"%s=%d\n",label,pass?1:0);
    fflush(receipt);
    failures+=!pass;
}

static int finish(void){

    fprintf(receipt,"DEFAULT_ASSEMBLY_ACCEPTANCE_FAILURES=%d\n",failures);
    fclose(receipt);
    ProEngineerEnd();
    return 0;
}

static ProError execute(const wchar_t *protocol,wchar_t *result){
    PartCommand command={0};
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!snapshot)return PRO_TK_OUT_OF_MEMORY;
    result[0]=0;
    ProError error=part_snapshot_take_mode(design,snapshot,0);
    if(!error&&part_command_parse(protocol,&command)!=1)error=PRO_TK_BAD_INPUTS;
    if(!error)error=part_execute(&command,snapshot,result,PART_TEXT_MAX);
    part_snapshot_clear(snapshot);
    free(snapshot);
    return error;
}

static int assembly_probe(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl active=NULL,component=NULL;
    ProFeature occurrence={0};
    ProAsmcompconstraint *rows=NULL;
    ProAsmcompConstrType type=PRO_ASM_UNDEF;
    ProFeatStatus state=PRO_FEAT_UNREGENERATED;
    ProBoolean packaged=PRO_B_TRUE,under=PRO_B_TRUE;
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}},actual;
    AxScan *scan=NULL;
    int count=0;
    double scale;
    wchar_t protocol[320];
    static wchar_t report[PART_TEXT_MAX];
    ProPath leaf;
    FixtureFeatures after={0};
    ProMassProperty mass;
    ProOutline outline;
    verify(!ProMdlCurrentGet(&active)&&active==design,"ACTIVE_EMPTY_ASSEMBLY");
    verify(dv_native_revision(directory,source_name,leaf)==PRO_TK_E_NOT_FOUND,"SOURCE_EXISTS_ONLY_IN_MEMORY");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_INSERT %ls %ls DEFAULT",design_name,source_name);
    ProError error=execute(protocol,report);
    verify(!error&&wcsstr(report,L"原生默认定位"),"DEFAULT_COMMAND_ACCEPTED");
    if(error){
        fprintf(receipt,"INSERT_STATUS=%d\n",error);
        return finish();
    }
    error=ax_scan(design,1,&scan);
    if(!error&&scan->count==1&&scan->occurrences[0].depth==1)
        error=ax_component_feature(design,scan->occurrences[0].ids[0],&occurrence);
    else if(!error)error=PRO_TK_INCOMPLETE;
    verify(!error&&occurrence.id>0,"EXACTLY_ONE_NATIVE_OCCURRENCE");
    free(scan);
    if(error)return finish();
    verify(!ProAsmcompMdlGet(&occurrence,&component)&&component==source_part,"COMPONENT_IS_SAME_LOADED_PART");
    verify(!ax_read_constraints(&occurrence,NULL,&rows,&count)&&count==1&&
        !ProAsmcompconstraintTypeGet(rows[0],&type)&&type==PRO_ASM_DEF_PLACEMENT,"NATIVE_DEFAULT_CONSTRAINT");
    if(rows)ProAsmcompconstraintArrayFree(rows);
    verify(!ProFeatureStatusGet(&occurrence,&state)&&state==PRO_FEAT_ACTIVE&&
        !ProAsmcompIsPackaged(&occurrence,&packaged)&&!packaged&&
        !ProAsmcompIsUnderconstrained(&occurrence,&under)&&!under,"ACTIVE_FULLY_PLACED_COMPONENT");
    verify(!qa_mm_per_unit(design,&scale)&&!ax_actual_pose(design,&occurrence,actual)&&
        ax_same_pose(identity,actual,scale),"DEFAULT_ORIGIN_AND_AXIS_DIRECTIONS");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_PLACEMENT %ls ID:%d",design_name,occurrence.id);
    verify(!execute(protocol,report)&&wcsstr(report,L"默认定位")&&!wcsstr(report,L"参照缺失"),"PLACEMENT_REPORT");
    verify(!ProSolidFeatVisit((ProSolid)source_part,collect_features,NULL,&after)&&
        after.count==original_features.count&&!memcmp(after.ids,original_features.ids,sizeof(after.ids))&&
        !memcmp(after.types,original_features.types,sizeof(after.types)),"SOURCE_FEATURES_PRESERVED");
    int same_geometry=!qa_geometry_mass(source_part,&mass)&&!ProSolidOutlineGet((ProSolid)source_part,outline)&&
        part_close(mass.volume,original_mass.volume)&&part_close(mass.surface_area,original_mass.surface_area);
    for(int i=0;same_geometry&&i<2;i++)for(int j=0;j<3;j++)if(!part_close(outline[i][j],original_outline[i][j]))same_geometry=0;
    verify(same_geometry,"SOURCE_GEOMETRY_PRESERVED");
    verify(dv_native_revision(directory,source_name,leaf)==PRO_TK_E_NOT_FOUND,"SOURCE_NOT_SAVED_BY_ASSEMBLY");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_INSERT %ls NO_SUCH_260111_PART DEFAULT",design_name);
    verify(execute(protocol,report)!=PRO_TK_NO_ERROR,"MISSING_PART_REJECTED");
    scan=NULL;
    error=ax_scan(design,1,&scan);
    verify(!error&&scan->count==1&&scan->occurrences[0].ids[0]==occurrence.id,"FAILED_INSERT_PRESERVES_TREE");
    free(scan);
    return finish();
}

static int setup_probe(uiCmdCmdId id,uiCmdValue *value,void *data){
    receipt=fopen("default-assembly-acceptance.log","w");
    if(!receipt){ProEngineerEnd();return 0;}
    const ModelPlan plan={.a=80,.b=60,.height=6,.hole=6,.ready=1};
    const char *stage="fixture";
    int window;
    ProError error=ProDirectoryCurrentGet(directory);
    wcscpy_s(source_name,_countof(source_name),L"bwtl_default_part_fixture");
    if(!error)error=quick_seed(source_name,&source_part);
    if(!error)error=model_build_internal(&plan,source_name,&stage,source_part,NULL,NULL);
    if(!error)error=ProSolidFeatVisit((ProSolid)source_part,collect_features,NULL,&original_features);
    if(!error)error=qa_geometry_mass(source_part,&original_mass);
    if(!error)error=ProSolidOutlineGet((ProSolid)source_part,original_outline);
    if(!error)error=model_create_empty(3,design_name,&stage);
    if(!error)error=ProMdlnameInit(design_name,PRO_MDL_ASSEMBLY,&design);
    if(!error)error=ProMdlWindowGet(design,&window);
    if(!error)error=ProWindowCurrentSet(window);
    if(!error)error=ProMacroLoad(L"~ Command \x60" L"BWTLDefaultAssemblyRun" L"\x60;");
    fprintf(receipt,"FIXTURE_RESULT=%d\n",error);
    if(error){failures++;return finish();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=inventory_toolkit_init(argc,argv,version,build,buffer);
    uiCmdCmdId command;
    if(!error)error=ProCmdActionAdd("BWTLDefaultAssemblyRun",assembly_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(!error)error=ProCmdActionAdd("BWTLDefaultAssemblySetup",setup_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command \x60" L"BWTLDefaultAssemblySetup" L"\x60;");
}
