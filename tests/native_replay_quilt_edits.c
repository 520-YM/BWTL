#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

typedef struct {ProSurface surface;int id;} ReplayQuiltEdge;

typedef struct {ProDimension item;double value;int count;} QuiltDimension;

static ProError quilt_dimension(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    QuiltDimension *found=data;
    ProDimensiontype type;
    double value;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error&&type==PRODIMTYPE_LINEAR&&fabs(value-found->value)<1e-7){
        found->item=*dimension;
        found->count++;
    }
    return error;
}

static ProError quilt_measure(ProMdl model,int variant,double *value)
{
    const wchar_t *names[]={L"QUILT:OFFSET_A",L"QUILT:PATCH",L"QUILT:PATCH",L"QUILT:SIDES"};
    FdRef ref={0};
    SfArea area;
    ProError error=sf_ref(model,names[variant],&ref);
    if(!error)error=sf_area(&ref,&area);
    if(!error&&variant==0){
        ProVector direction={0,1,0},low,high;
        error=ProSurfaceExtremesEval(area.single,direction,low,high);
        if(!error)*value=high[1];
    }else if(!error)*value=area.area;
    fd_ref_free(&ref);
    return error;
}

static ProError quilt_verify_edit(const wchar_t *output,int variant,FILE *log)
{
    const wchar_t *labels[][2]={{L"OFFSET_A",NULL},{L"EXTENDED",NULL},
        {L"CUT_PLANE",NULL},{L"SIDES_SK",L"BOTTOM_SK"}};
    const double original[]={3,5,10,20},modified[]={4,6,12,22};
    const double delta[]={1,20,20,80};
    ProMdl result=NULL;
    QuiltDimension driver[2]={0};
    double before=0,after=0,restored=0;
    const int count=variant==3?2:1;
    ProError error=ProMdlFiletypeLoad((wchar_t *)output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=quilt_measure(result,variant,&before);
    for(int i=0;!error&&i<count;i++){
        ProFeature feature;
        driver[i].value=original[variant];
        error=ux_named_feature(result,labels[variant][i],&feature);
        if(!error)error=ProFeatureDimensionVisit(&feature,quilt_dimension,NULL,&driver[i]);
        if(!error&&driver[i].count!=1)error=PRO_TK_INCOMPLETE;
    }
    for(int i=0;!error&&i<count;i++)error=ProDimensionValueSet(&driver[i].item,modified[variant]);
    if(!error)error=part_regenerate(result);
    if(!error)error=quilt_measure(result,variant,&after);
    if(!error&&fabs(fabs(after-before)-delta[variant])>0.005)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<count;i++)error=ProDimensionValueSet(&driver[i].item,original[variant]);
    if(!error)error=part_regenerate(result);
    if(!error)error=quilt_measure(result,variant,&restored);
    if(!error&&fabs(restored-before)>0.005)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"QUILT EDIT VARIANT=%d ERROR=%d GEOMETRY=%.8f/%.8f/%.8f DRIVERS=%d,%d\n",
        variant,error,before,after,restored,driver[0].count,driver[1].count);
    if(result)ProMdlErase(result);
    return error;
}

static ProError quilt_seed_edge(ProEdge edge,ProError status,ProAppData data)
{
    if(status)return status;
    ProVector a,b;
    ProError error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL);
    if(!error)error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL);
    if(!error&&fabs(a[1])<1e-6&&fabs(b[1])<1e-6)error=ProEdgeIdGet(edge,data);
    return error;
}

static ProError quilt_seed_contour(ProContour contour,ProError status,ProAppData data)
{
    if(status)return status;
    ReplayQuiltEdge *found=data;
    return ProContourEdgeVisit(found->surface,contour,quilt_seed_edge,NULL,&found->id);
}

static ProError quilt_execute_steps(ProMdl model,const wchar_t *name,const wchar_t **steps,int count,FILE *log)
{
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<count;i++){
        wchar_t protocol[512],formatted[512];
        swprintf_s(formatted,_countof(formatted),steps[i],name);
        swprintf_s(protocol,_countof(protocol),L"CREO_OP %ls",formatted);
        error=expansion_build(model,protocol,log);
    }
    return error;
}

static ProError quilt_trial(int variant,FILE *log)
{
    const wchar_t *patch[]={
        L"SKETCH_CREATE %ls FILL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",
        L"SURFACE_FILL %ls PATCH FILL_SK"
    };
    const wchar_t *offset[]={
        L"SKETCH_CREATE %ls WALL_SK PLANE:FRONT PLANE:RIGHT LINE 0 0 20 0",
        L"SURFACE_EXTRUDE %ls WALL WALL_SK 10 POS",
        L"SURFACE_OFFSET %ls OFFSET_A QUILT:WALL 3",
        L"SURFACE_OFFSET %ls OFFSET_B QUILT:WALL -3"
    };
    const wchar_t *trim[]={
        L"DATUM_PLANE %ls CUT_PLANE PLANE:RIGHT 10",
        L"SURFACE_TRIM %ls HALF_PATCH QUILT:PATCH PLANE:CUT_PLANE POS"
    };
    const wchar_t *join[]={
        L"SKETCH_CREATE %ls SIDES_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",
        L"SURFACE_EXTRUDE %ls SIDES SIDES_SK 10 POS",
        L"SURFACE_FILL %ls CAP_TOP SIDES_SK",
        L"DATUM_PLANE %ls BOTTOM_PLANE PLANE:FRONT -10",
        L"SKETCH_CREATE %ls BOTTOM_SK PLANE:BOTTOM_PLANE PLANE:RIGHT RECT 0 0 20 10",
        L"SURFACE_FILL %ls CAP_BOTTOM BOTTOM_SK",
        L"SURFACE_MERGE %ls CLOSED QUILT:SIDES QUILT:CAP_TOP QUILT:CAP_BOTTOM"
    };
    ProMdl source=NULL;
    ProMdlName name;
    const char *stage="prepare quilt editing fixture";
    ProError error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    if(!error&&variant==0)error=quilt_execute_steps(source,name,offset,_countof(offset),log);
    if(!error&&(variant==1||variant==2))error=quilt_execute_steps(source,name,patch,_countof(patch),log);
    if(!error&&variant==1){
        FdRef ref={0};
        SfArea area;
        ReplayQuiltEdge edge={0};
        error=sf_ref(source,L"QUILT:PATCH",&ref);
        if(!error)error=sf_area(&ref,&area);
        if(!error){edge.surface=area.single;error=ProSurfaceContourVisit(area.single,quilt_seed_contour,NULL,&edge);}
        if(!error&&!edge.id)error=PRO_TK_E_NOT_FOUND;
        if(!error){
            wchar_t command[512];
            swprintf_s(command,_countof(command),L"CREO_OP SURFACE_EXTEND %ls EXTENDED QUILT:PATCH EDGE:%d 5",name,edge.id);
            error=expansion_build(source,command,log);
        }
        fd_ref_free(&ref);
    }
    if(!error&&variant==2)error=quilt_execute_steps(source,name,trim,_countof(trim),log);
    if(!error&&variant==3)error=quilt_execute_steps(source,name,join,_countof(join),log);
    fwprintf(log,L"QUILT SOURCE VARIANT=%d ERROR=%d\n",variant,error);
    if(!error)error=ProMdlSave(source);

    ProPath output=L"";
    wchar_t report[18000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,0,&out);
    fwprintf(log,L"QUILT REPLAY VARIANT=%d ERROR=%d\n%ls\n",variant,error,report);

    if(!error)error=quilt_verify_edit(output,variant,log);
    return error;
}

static int quilt_edit_case(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-quilt-edits.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 1;}
    model_background_mode=1;
    ProError offset=quilt_trial(0,log);
    ProError extend=quilt_trial(1,log);
    ProError trim=quilt_trial(2,log);
    ProError merge=quilt_trial(3,log);
    fwprintf(log,L"OFFSET=%d EXTEND=%d TRIM=%d MERGE=%d PASS=%d\n",offset,extend,trim,merge,
        !offset&&!extend&&!trim&&!merge);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

#ifndef BWTL_REUSE_QUILT_EDITS
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLQuiltEdits",quilt_edit_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLQuiltEdits\x0060;");
}
#endif
