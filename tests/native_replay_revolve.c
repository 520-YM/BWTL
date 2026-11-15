#define BWTL_BATCH_REPORTING_HELPER
#include "native_imitation_batch_reporting.c"
#undef BWTL_BATCH_REPORTING_HELPER

static ProError revolve_source(ProMdl source, const wchar_t *operation, const wchar_t *name,
                               const wchar_t *args,FILE *log)
{
    wchar_t protocol[512],message[4096]=L"";
    swprintf_s(protocol,_countof(protocol),L"CREO_OP %ls %ls %ls",operation,name,args);
    PartCommand cmd={0};
    ProError error=part_command_parse(protocol,&cmd)==1?
        foundation_execute(&cmd,source,message,_countof(message)):PRO_TK_BAD_INPUTS;
    fwprintf(log,L"%ls = %d\n%ls\n",operation,error,message);
    return error;
}

typedef struct {ProDimension selected;int count;} RevolveAngle;

static ProError revolve_find_angle(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    RevolveAngle *search=data;
    double value=0;
    ProDimensiontype type;
    if(!ProDimensionTypeGet(dimension,&type)&&type==PRODIMTYPE_ANGLE&&
       !ProDimensionValueGet(dimension,&value)&&fabs(value-90)<1e-7){
        search->selected=*dimension;
        search->count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError revolve_case(FILE *log)
{
    ProMdlName name;
    ProMdl source=NULL,result=NULL;
    const char *stage=NULL;
    ProError error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    const wchar_t *operation[][2]={
        {L"DATUM_AXIS",L"REV_AXIS PLANES PLANE:TOP PLANE:RIGHT"},
        {L"SKETCH_CREATE",L"REV_SK PLANE:TOP PLANE:RIGHT RECT 10 0 10 30"},
        {L"SOLID_REVOLVE",L"REV_ADD REV_SK AXIS:REV_AXIS 360 POS"},
        {L"SKETCH_CREATE",L"SECTOR_SK PLANE:TOP PLANE:RIGHT RECT 17 20 4 5"},
        {L"SOLID_REVOLVE_CUT",L"NEG_GROOVE SECTOR_SK AXIS:REV_AXIS 90 NEG"}
    };
    for(int i=0;!error&&i<_countof(operation);i++)
        error=revolve_source(source,operation[i][0],name,operation[i][1],log);
    wchar_t report[24000]=L"";
    PartText out={report,_countof(report),0};
    ProPath delivered=L"";
    if(!error)error=imitation_run_mode(source,delivered,0,1,&out);
    fwprintf(log,L"REBUILD=%d\n%ls\n",error,report);
    fflush(log);
    if(!error&&(!wcsstr(report,L"旋转加料")||!wcsstr(report,L"旋转去料")))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ProFeature cut;
    RevolveAngle angle={0};
    if(!error)error=ux_named_feature(result,L"NEG_GROOVE",&cut);
    if(!error)error=ProFeatureDimensionVisit(&cut,revolve_find_angle,NULL,&angle);
    if(!error&&angle.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&angle.selected,75);
    if(!error)error=part_regenerate(result);
    ProMassProperty actual;
    if(!error)error=qa_geometry_mass(result,&actual);
    if(!error&&!part_close(actual.volume,(9000.0-555.0*75.0/360.0)*3.141592653589793))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&angle.selected,90);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&actual);
    if(!error&&!part_close(actual.volume,(9000.0-555.0/4.0)*3.141592653589793))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"ANGLE 90 TO 75 AND RESTORE=%d\n",error);
    return error;
}

static int revolve_replay_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-revolve.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError error=revolve_case(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=imitation_batch_reporting_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayRevolve",revolve_replay_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayRevolve\x0060;");
}
