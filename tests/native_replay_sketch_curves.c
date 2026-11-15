#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError sketch_trial_draw(ProSection section,const ProMatrix frame,double scale,void *context)
{
    const int kind=*(int *)context;
    int id;
    (void)frame;
    (void)scale;
    if(kind==0){
        Pro2dEllipsedef ellipse={PRO_2D_ELLIPSE,{13,21},11,5};
        return ProSectionEntityAdd(section,(Pro2dEntdef *)&ellipse,&id);
    }
    if(kind==1){
        Pro2dConicdef conic={PRO_2D_CONIC,{-10,0},{10,0},{0,5},0.5};
        ProError error=ProSectionEntityAdd(section,(Pro2dEntdef *)&conic,&id);
        if(!error)error=as_add_line(section,as_point(10,0),as_point(-10,0),&id);
        return error;
    }
    if(kind==2){
        const double w=sqrt(0.5);
        Pro2dPnt points[3]={{20,0},{20*w,8*w},{0,8}};
        const double knots[]={0,0,0,1,1,1},weights[]={1,w,1};
        return as_add_bspline(section,2,points,_countof(points),knots,weights,&id);
    }
    return PRO_TK_BAD_INPUTS;
}

static ProError sketch_trial_entity(ProSection section,Pro2dEntType kind,int *result)
{
    int *ids=NULL,count=0,found=0;
    ProError error=ProSectionEntityIdsGet(section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        ProBoolean projected=PRO_B_FALSE;
        Pro2dEntdef *entity=NULL;
        if(ProSectionEntityIsProjection(section,ids[i],&projected)||projected)continue;
        if(!ProSectionEntityGet(section,ids[i],&entity)&&entity->type==kind){
            *result=ids[i];
            found++;
        }
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(!error&&found!=1)error=PRO_TK_INCOMPLETE;
    return error;
}

static ProError sketch_trial_compare(ProMdl source,ProMdl result,const wchar_t *name,Pro2dEntType kind)
{
    FdSketch a={0},b={0};
    int from=-1,to=-1;
    ProError error=fd_sketch_open(source,name,&a,0);
    if(!error)error=fd_sketch_open(result,name,&b,0);
    if(!error)error=sketch_trial_entity(a.section,kind,&from);
    if(!error)error=sketch_trial_entity(b.section,kind,&to);
    if(!error)error=replay_section_entity_compare(a.section,from,b.section,to);
    fd_sketch_free(&a);
    fd_sketch_free(&b);
    return error;
}

static ProError sketch_trial(FILE *log)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&source,name,&geometry);
    const wchar_t *sketches[]={L"ELLIPSE_ALIGNED",L"CONIC_SHAPE",L"ELLIPSE_ARC",L"SPL_THROUGH",L"SPL_CONTROL",L"ROTATED_ELLIPSE"};
    const Pro2dEntType types[]={PRO_2D_ELLIPSE,PRO_2D_CONIC,PRO_2D_B_SPLINE,PRO_2D_SPLINE,PRO_2D_B_SPLINE,PRO_2D_B_SPLINE};
    const wchar_t *shapes[]={L"SPLINE THROUGH 0 0 8 3 16 -2 25 5",L"SPLINE CONTROL 0 0 8 3 16 -2 25 5",L"ELLIPSE 12 14 24 8 31"};
    for(int i=0;!error&&i<_countof(sketches);i++){
        if(i<3){
            ProMatrix frame;
            error=smx_custom_sketch(source,sketches[i],L"PLANE:FRONT",L"PLANE:RIGHT",sketch_trial_draw,&i,frame);
        }else{
            wchar_t protocol[512];
            swprintf_s(protocol,_countof(protocol),L"CREO_OP SKETCH_CREATE %ls %ls PLANE:FRONT PLANE:RIGHT %ls",
                name,sketches[i],shapes[i-3]);
            error=expansion_build(source,protocol,log);
        }
        fwprintf(log,L"SKETCH SOURCE %ls=%d\n",sketches[i],error);
    }
    ProPath delivered=L"";
    wchar_t report[24000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,delivered,0,1,&out);
    fwprintf(log,L"SKETCH REPLAY=%d\n%ls\n",error,report);
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    for(int i=0;!error&&i<_countof(sketches);i++){
        error=sketch_trial_compare(source,result,sketches[i],types[i]);
        fwprintf(log,L"SKETCH READBACK %ls=%d\n",sketches[i],error);
    }
    return error;
}

static ProError sketch_trial_control_difference(FILE *log)
{
    ProSection original=NULL,changed=NULL;
    Pro2dPnt points[4]={{0,0},{8,3},{16,-2},{25,5}};
    const double knots[]={0,0,0,0,1,1,1,1};
    int a=-1,b=-1;
    ProError error=ProSection2DAlloc(&original);
    if(!error)error=ProSection2DAlloc(&changed);
    if(!error)error=as_add_bspline(original,3,points,_countof(points),knots,NULL,&a);
    points[2][1]+=1.5;
    if(!error)error=as_add_bspline(changed,3,points,_countof(points),knots,NULL,&b);
    if(!error&&replay_section_entity_compare(original,a,changed,b)!=PRO_TK_INCOMPLETE)error=PRO_TK_INCOMPLETE;
    if(original)ProSectionFree(&original);
    if(changed)ProSectionFree(&changed);
    fwprintf(log,L"DIFFERENT CONTROL POINT DETECTED=%d\n",error);
    return error;
}

static int sketch_curve_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-sketch-curves.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    ProError error=sketch_trial(log);
    if(!error)error=sketch_trial_control_difference(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplaySketchCurves",sketch_curve_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplaySketchCurves\x0060;");
}
