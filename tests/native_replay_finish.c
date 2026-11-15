#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

static ProError finish_blend_area(ProMdl model,double *area)
{
    ImitationShapeMetrics shape;
    ProError error=imitation_shape_read(model,&shape);
    if(!error&&shape.quilts!=1)error=PRO_TK_INCOMPLETE;
    if(!error)*area=shape.quilt_area;
    return error;
}

static ProError finish_edit_boundary(ProMdl model,FILE *log)
{
    ProFeature plane;
    ReplayDimensionMatch edit={.type=PRODIMTYPE_LINEAR,.value=25};
    double before=0,changed=0,restored=0;
    ProError error=ux_named_feature(model,L"END_PLANE",&plane);
    if(!error)error=ProFeatureDimensionVisit(&plane,replay_match_dimension,NULL,&edit);
    if(!error&&edit.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=finish_blend_area(model,&before);
    if(!error)error=ProDimensionValueSet(&edit.target,27);
    if(!error)error=part_regenerate(model);
    if(!error)error=finish_blend_area(model,&changed);
    if(!error&&(!part_close(before,700)||!part_close(changed,756)))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&edit.target,25);
    if(!error)error=part_regenerate(model);
    if(!error)error=finish_blend_area(model,&restored);
    if(!error&&!part_close(before,restored))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"BOUNDARY AREA %.7g -> %.7g -> %.7g ERROR=%d\n",before,changed,restored,error);
    return error;
}

static ProError finish_curve_start(ProMdl source,const wchar_t *name,int kind,FILE *log){
    ProError error=batch_step(source,log,L"CREO_OP DATUM_PLANE %ls END_PLANE PLANE:FRONT 25",name);
    const wchar_t *first=kind==2?L"CIRCLE 0 0 13":L"LINE 4 3 32 3";
    const wchar_t *last=kind==2?L"CIRCLE 0 0 7":L"LINE 4 3 32 3";
    if(!error)error=batch_step(source,log,L"CREO_OP SKETCH_CREATE %ls A PLANE:FRONT PLANE:RIGHT %ls",name,first);
    if(!error)error=batch_step(source,log,L"CREO_OP SKETCH_CREATE %ls B PLANE:END_PLANE PLANE:RIGHT %ls",name,last);
    if(!error&&kind==1)error=batch_step(source,log,L"CREO_OP SURFACE_BOUNDARY %ls PATCH CURVE:A CURVE:B",name);
    if(!error&&kind==2)error=batch_step(source,log,L"CREO_OP SOLID_BLEND %ls PATCH A B",name);
    return error;
}

static int finish_probe(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-finish.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    int pass=0;
    for(int i=1;i<=2;i++){
        ProMdl source=NULL,result=NULL;
        ProMdlName name=L"";
        ProError error=batch_empty(1,&source,name);
        if(!error)error=finish_curve_start(source,name,i,log);

        if(!error)error=batch_reopen(source,&result,log);
        if(!error)error=i==1?finish_edit_boundary(result,log):batch_edit(result,L"B",PRODIMTYPE_DIAMETER,14,16,log);
        if(result)ProMdlErase(result);
        if(source)ProMdlErase(source);
        fwprintf(log,L"CASE %d ERROR=%d\n",i,error);
        pass+=!error;
    }
    fwprintf(log,L"FINISH CASES=%d PASS=%d\n",pass,pass==2);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayFinish",finish_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayFinish\x0060;");
}

