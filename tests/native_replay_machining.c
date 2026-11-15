#define BWTL_BATCH_REPORTING_HELPER
#include "native_imitation_batch_reporting.c"
#undef BWTL_BATCH_REPORTING_HELPER

static ProError machining_step(ProMdl model,const wchar_t *protocol,FILE *log)
{
    PartCommand cmd={0};
    wchar_t report[4096]=L"";
    const ProError error=part_command_parse(protocol,&cmd)==1?
        foundation_execute(&cmd,model,report,_countof(report)):PRO_TK_BAD_INPUTS;
    fwprintf(log,L"SOURCE %ls = %d\n%ls\n",protocol,error,report);
    fflush(log);
    return error;
}

static ProError machining_vertical(ProGeomitem *item,ProError status,ProAppData data)
{
    if(status)return status;
    ProEdge edge;
    ProVector a,b,first,second,tangent;
    if(ProGeomitemToEdge(item,&edge)||ProEdgeXyzdataEval(edge,0,a,first,second,tangent)||
       ProEdgeXyzdataEval(edge,1,b,first,second,tangent))return PRO_TK_NO_ERROR;
    if(fabs(a[0]-b[0])<1e-6&&fabs(a[1]-b[1])<1e-6&&fabs(fabs(a[2]-b[2])-20)<1e-6){
        *(int *)data=item->id;
        return PRO_TK_E_FOUND;
    }
    return PRO_TK_NO_ERROR;
}

static ProError machining_pick_edge(ProFeature *feature,ProError status,ProAppData data)
{
    if(status)return status;
    const ProError error=ProFeatureGeomitemVisit(feature,PRO_EDGE,machining_vertical,NULL,data);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}

typedef struct {ProDimension dimension;double value;int matches;} MachiningDimension;

static ProError machining_dimension(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    MachiningDimension *candidate=data;
    double value=0;
    if(!ProDimensionValueGet(dimension,&value)&&fabs(value-candidate->value)<1e-8){
        candidate->dimension=*dimension;
        candidate->matches++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError machining_accept(ProMdl source,const wchar_t *feature_name,int kind,FILE *log)
{
    ProPath delivered=L"";
    wchar_t report[18000]=L"";
    PartText out={report,_countof(report),0};
    ProError error=imitation_run_mode(source,delivered,0,1,&out);
    fwprintf(log,L"REBUILD %ls = %d\n%ls\n",feature_name,error,report);
    fflush(log);
    ProMdl result=NULL;
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ProFeature from,to;
    ProFeattype from_type,to_type;
    if(!error)error=ux_named_feature(source,feature_name,&from);
    if(!error)error=ux_named_feature(result,feature_name,&to);
    if(!error)error=ProFeatureTypeGet(&from,&from_type);
    if(!error)error=ProFeatureTypeGet(&to,&to_type);
    if(!error&&from_type!=to_type)error=PRO_TK_INCOMPLETE;
    if(!error){
        MachiningDimension edit={0};
        edit.value=kind==0?2:kind==3?8:6;
        ProMassProperty initial,modified,restored;
        error=qa_geometry_mass(result,&initial);
        if(!error)error=ProFeatureDimensionVisit(&to,machining_dimension,NULL,&edit);
        if(!error&&edit.matches!=1)error=PRO_TK_INCOMPLETE;
        if(!error)error=ProDimensionValueSet(&edit.dimension,edit.value+0.5);
        if(!error)error=part_regenerate(result);
        double value=0;
        if(!error)error=ProDimensionValueGet(&edit.dimension,&value);
        if(!error&&fabs(value-edit.value-0.5)>1e-8)error=PRO_TK_INCOMPLETE;
        if(!error)error=qa_geometry_mass(result,&modified);
        if(!error&&kind!=3&&modified.volume>=initial.volume-0.001)error=PRO_TK_INCOMPLETE;
        if(!error)error=ProDimensionValueSet(&edit.dimension,edit.value);
        if(!error)error=part_regenerate(result);
        if(!error)error=qa_geometry_mass(result,&restored);
        if(!error&&!part_close(initial.volume,restored.volume))error=PRO_TK_INCOMPLETE;
        fwprintf(log,L"DIMENSION EDIT AND RESTORE KIND=%d ERROR=%d MATCHES=%d\n",kind,error,edit.matches);
    }
    return error;
}

static ProError machining_case(FILE *log,int kind)
{
    ModelPlan plan={0,100,80,20,0,1};
    ProMdlName name;
    ProMdl source=NULL;
    const char *stage=NULL;
    ProError error=model_create(&plan,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    wchar_t protocol[512];
    ProModelitem top={0};
    top.owner=source;
    FdRef right={0},base={0};
    const wchar_t *feature=L"REPLAY_MACHINING";
    if(!error&&kind==0){
        int edge=0;
        error=ProSolidFeatVisit((ProSolid)source,machining_pick_edge,NULL,&edge);
        if(error==PRO_TK_E_FOUND)error=PRO_TK_NO_ERROR;
        if(!error&&!edge)error=PRO_TK_E_NOT_FOUND;
        swprintf_s(protocol,_countof(protocol),L"CREO_OP EDGE_CHAMFER %ls %ls 2 EDGE:%d",name,feature,edge);
    }
    if(!error&&kind){
        error=ProSolidSurfaceVisit((ProSolid)source,batch_top_face,NULL,&top);
        if(error==PRO_TK_E_FOUND)error=PRO_TK_NO_ERROR;
        else if(!error)error=PRO_TK_E_NOT_FOUND;
        if(!error)error=fd_ref_resolve(source,L"PLANE:RIGHT",PRO_SURFACE,&right);
        if(!error)error=fd_ref_resolve(source,L"PLANE:TOP",PRO_SURFACE,&base);
        const wchar_t *verb=kind==1?L"HOLE_COUNTERSINK":kind==2?L"HOLE_COUNTERBORE":L"HOLE_THREAD";
        const wchar_t *args=kind==1?L"6 12 90 THROUGH 0":kind==2?L"6 12 3 THROUGH 0":L"ISO M6x1 8 12";
        swprintf_s(protocol,_countof(protocol),L"CREO_OP %ls %ls %ls SURFACE:%d SURFACE:%d 40 SURFACE:%d 30 %ls",
            verb,name,feature,top.id,right.item.id,base.item.id,args);
    }
    if(!error)error=machining_step(source,protocol,log);
    fd_ref_free(&right);
    fd_ref_free(&base);
    if(!error)error=machining_accept(source,feature,kind,log);
    return error;
}

static int machining_replay_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-machining.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError chamfer=machining_case(log,0);
    const ProError sink=machining_case(log,1);
    const ProError bore=machining_case(log,2);
    const ProError thread=machining_case(log,3);
    fwprintf(log,L"CHAMFER=%d SINK=%d BORE=%d THREAD=%d\n",chamfer,sink,bore,thread);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=imitation_batch_reporting_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayMachining",machining_replay_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayMachining\x0060;");
}
