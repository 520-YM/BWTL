#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError chamfer_fixture_corner(ProMdl model,int edge_id,const wchar_t *name)
{
    FdTxn tx={0};
    ProError error=sh_begin(&tx,model,name,PRO_FEAT_CORN_CHAMF);
    FdRef corner={0};
    if(!error)error=ProModelitemInit(model,edge_id,PRO_EDGE,&corner.item);
    ProReference reference=NULL;
    ProEdge edge;
    ProVector point;
    if(!error)error=ProGeomitemToEdge(&corner.item,&edge);
    if(!error)error=ProEdgeXyzdataEval(edge,0,point,NULL,NULL,NULL);
    ProModelitem endpoint=corner.item;
    endpoint.type=PRO_EDGE_START;
    if(!error)error=ProSelectionAlloc(NULL,&endpoint,&corner.selection);
    if(!error)error=ProSelectionToReference(corner.selection,&reference);
    ProUvParam parameter={0.0,0.0};
    if(!error)error=ProReferenceParamsSet(reference,parameter);
    if(!error)error=ProReferencePointSet(reference,point);
    ProElement element=NULL;
    if(!error)error=model_element(tx.root,PRO_E_STD_CORNER_CHAMF_CORNER,0,0,&element);
    if(!error)error=ProElementReferenceSet(element,reference);
    const ProElemId size[]={PRO_E_STD_CORNER_CHAMF_EDGE1,PRO_E_STD_CORNER_CHAMF_EDGE2,PRO_E_STD_CORNER_CHAMF_EDGE3};
    for(int i=0;!error&&i<_countof(size);i++)error=model_element(tx.root,size[i],2,2.0+i,NULL);
    if(!error)error=fd_create(&tx,0);
    trace("native corner create",error);
    for(int i=0;tx.errors.error_list&&i<tx.errors.error_number;i++){
        trace("native corner element",tx.errors.error_list[i].err_item_id);
        trace("native corner element error",tx.errors.error_list[i].error);
    }
    if(!error)error=part_regenerate(model);
    if(reference)ProReferenceFree(reference);
    fd_ref_free(&corner);
    fd_cleanup(&tx,error);
    return error;
}

static ProError chamfer_trial(FILE *log,int kind)
{
    ProMdl source=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&source,name,&faces);
    const int id=error?0:faces.vertical[0];
    if(!error&&kind==2)error=chamfer_fixture_corner(source,id,L"REPLAY_CHAMFER");
    if(!error&&kind!=2){
        ProEdge edge;
        ProSurface side;
        int surface=0;
        error=ProEdgeInit((ProSolid)source,id,&edge);
        if(!error)error=ProEdgeNeighborsGet(edge,NULL,NULL,&side,NULL);
        if(!error)error=ProSurfaceIdGet(side,&surface);
        wchar_t command[512];
        swprintf_s(command,_countof(command),L"CREO_OP EDGE_CHAMFER_ADV %ls REPLAY_CHAMFER EDGE:%d SURFACE:%d %ls",
            name,id,surface,kind?L"AD 30 3":L"DD 2 4");
        if(!error)error=expansion_build(source,command,log);
    }
    fwprintf(log,L"CHAMFER SOURCE KIND=%d ERROR=%d\n",kind,error);
    ProPath output=L"";
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"CHAMFER REPLAY KIND=%d ERROR=%d\n%ls\n",kind,error,report);
    ProMdl result=NULL;
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ProFeature feature;
    if(!error)error=ux_named_feature(result,L"REPLAY_CHAMFER",&feature);
    ProFeattype type;
    if(!error)error=ProFeatureTypeGet(&feature,&type);
    if(!error&&type!=(kind==2?PRO_FEAT_CORN_CHAMF:PRO_FEAT_CHAMFER))error=PRO_TK_INCOMPLETE;
    ReplayDimensionMatch dimension={0};
    dimension.type=kind==1?PRODIMTYPE_ANGLE:PRODIMTYPE_LINEAR;
    dimension.value=kind==1?30:2;
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&dimension);
    if(!error&&dimension.count!=1)error=PRO_TK_INCOMPLETE;
    ProMassProperty before,edited,restored;
    if(!error)error=qa_geometry_mass(result,&before);
    if(!error)error=ProDimensionValueSet(&dimension.target,dimension.value+0.5);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&edited);
    if(!error&&fabs(edited.volume-before.volume)<0.01)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&dimension.target,dimension.value);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&restored);
    if(!error&&!part_close(before.volume,restored.volume))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"CHAMFER EDIT AND RESTORE KIND=%d ERROR=%d\n",kind,error);
    return error;
}

static int chamfer_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-chamfer.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError unequal=chamfer_trial(log,0);
    const ProError angle=chamfer_trial(log,1);
    const ProError corner=chamfer_trial(log,2);
    fwprintf(log,L"UNEQUAL=%d ANGLE=%d CORNER=%d PASS=%d\n",unequal,angle,corner,!unequal&&!angle&&!corner);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayChamfer",chamfer_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayChamfer\x0060;");
}
