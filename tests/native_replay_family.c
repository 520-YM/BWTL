#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

static ProError family_trial(FILE *log)
{
    ProMdl generic=NULL,instance=NULL,result=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&generic,name,&faces);
    if(!error)error=batch_step(generic,log,L"CREO_OP HOLE %ls DRILL SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 6 THROUGH 0",
        name,faces.top,faces.x,faces.y);
    if(!error)error=batch_step(generic,log,L"CREO_OP HOLE %ls OPTIONAL SURFACE:%d SURFACE:%d 45 SURFACE:%d 25 5 THROUGH 0",
        name,faces.top,faces.x,faces.y);
    ProFeature hole,optional;
    ReplayDimensionMatch diameter={.type=PRODIMTYPE_DIAMETER,.value=6};
    if(!error)error=ux_named_feature(generic,L"DRILL",&hole);
    if(!error)error=ProFeatureDimensionVisit(&hole,replay_match_dimension,NULL,&diameter);
    if(!error&&diameter.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=ux_named_feature(generic,L"OPTIONAL",&optional);
    ProModelitem owner;
    ProParameter parameter;
    ProParamvalue default_value={.type=PRO_PARAM_INTEGER,.value.i_val=10};
    if(!error)error=ProMdlToModelitem(generic,&owner);
    if(!error)error=ProParameterWithUnitsCreate(&owner,L"BATCH_GRADE",&default_value,NULL,&parameter);
    ProFamtable table;
    ProFamtableItem items[3];
    if(!error)error=ProFamtableInit(generic,&table);
    if(!error)error=ProModelitemToFamtableItem(&diameter.target,&items[0]);
    if(!error)error=ProModelitemToFamtableItem(&optional,&items[1]);
    if(!error)error=ProParameterToFamtableItem(&parameter,&items[2]);
    for(int i=0;!error&&i<3;i++)error=ProFamtableItemAdd(&table,&items[i]);
    ProFaminstance row;
    if(!error)error=ProFaminstanceInit(L"TEST_FAMILY_9",&table,&row);
    if(!error)error=ProFaminstanceAdd(&row);
    ProParamvalue values[]={{.type=PRO_PARAM_DOUBLE,.value.d_val=9},
        {.type=PRO_PARAM_STRING,.value.s_val=L"N"},
        {.type=PRO_PARAM_INTEGER,.value.i_val=20}};
    for(int i=0;!error&&i<3;i++)error=ProFaminstanceValueSet(&row,&items[i],&values[i]);
    if(!error)error=ProFaminstanceRetrieve(&row,&instance);
    if(!error)error=part_regenerate(instance);
    ProParamvalue switch_read;
    if(!error)error=ProFaminstanceValueGet(&row,&items[1],&switch_read);
    if(!error){
        ProFeature state_feature;
        ProFeatStatus state=0;
        error=ProFeatureInit((ProSolid)instance,optional.id,&state_feature);
        if(!error)error=ProFeatureStatusGet(&state_feature,&state);
        fwprintf(log,L"INSTANCE FEATURE STATUS=%d VALUE TYPE=%d\n",state,switch_read.type);
        if(!error&&state!=PRO_FEAT_FAMTAB_SUPPRESSED)error=PRO_TK_INCOMPLETE;
    }
    fwprintf(log,L"NATIVE INSTANCE=%d\n",error);
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=replay_family_read(instance,NULL,&out);
    if(!error&&(!wcsstr(report,L"族表参数 BATCH_GRADE = 20")||!wcsstr(report,L" = 9。")||
        !wcsstr(report,L" = N。")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"INSTANCE EFFECTIVE VALUES=%d\n%ls\n",error,report);
    report[0]=0;
    out=(PartText){report,_countof(report),0};
    ProPath delivered=L"";
    if(!error)error=imitation_run_mode(instance,delivered,0,1,&out);
    fwprintf(log,L"INSTANCE REBUILD=%d\n%ls\n",error,report);
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=dv_no_external_dependencies(result);
    if(!error&&ux_named_feature(result,L"OPTIONAL",&optional)!=PRO_TK_E_NOT_FOUND)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProMdlToModelitem(result,&owner);
    if(!error)error=ProParameterInit(&owner,L"BATCH_GRADE",&parameter);
    if(!error)error=ProParameterValueWithUnitsGet(&parameter,&default_value,NULL);
    if(!error&&(default_value.type!=PRO_PARAM_INTEGER||default_value.value.i_val!=20))error=PRO_TK_INCOMPLETE;
    if(!error)error=batch_edit(result,L"DRILL",PRODIMTYPE_DIAMETER,9,9.5,log);
    return error;
}

static int family_case(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-family.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    const ProError error=family_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayFamily",family_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLReplayFamily`;");
}
