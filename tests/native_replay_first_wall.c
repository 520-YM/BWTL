#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError first_wall_loops(ProMdl model,const wchar_t *name)
{
    FdSketch sketch={0};
    ProErrorlist errors={0};
    int *original=NULL,count=0;
    ProError error=fd_sketch_open(model,L"OUTLINE",&sketch,0);
    if(!error)error=ProSectionEntityIdsGet(sketch.section,&original,&count);
    static const wchar_t *const loops[]={
        L"POLYGON 6 0 0 VERTEX 50 0",L"RECT -22 -7 12 14",L"CIRCLE 13 0 4"
    };
    for(int i=0;!error&&i<_countof(loops);i++){
        wchar_t command[512];
        swprintf_s(command,_countof(command),L"CREO_OP SKETCH_ADD %ls OUTLINE %ls",name,loops[i]);
        PartCommand parsed={0};
        int ids[AS_LIMIT],added=0;
        error=part_command_parse(command,&parsed)==1?
            fd_shape_append(sketch.section,&parsed,3,1.0,ids,&added):PRO_TK_BAD_INPUTS;
    }
    for(int i=0;!error&&i<count;i++)error=ProSectionEntityDelete(sketch.section,original[i]);
    if(original)ProArrayFree((ProArray *)&original);
    if(!error)error=fd_section_solve(sketch.section,1.0);
    if(!error)error=sd_mark_section(&sketch);
    if(!error){
        sketch.options[0]=PRO_FEAT_CR_NO_OPTS;
        error=ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,sketch.options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);
    }
    if(!error)error=part_regenerate(model);
    fd_sketch_free(&sketch);
    return error;
}

static ProError first_wall_trial(FILE *log,int positive)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    const char *stage="blank sheet";
    ProError error=model_create_empty(2,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    wchar_t command[512];
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 70 70 10 10",name);
        error=expansion_build(source,command,log);
    }
    if(!error)error=first_wall_loops(source,name);
    fwprintf(log,L"FIRST WALL SKETCH=%d\n",error);
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP SMT_FIRST_WALL %ls STOCK SKETCH:OUTLINE 2.5 %ls",name,
            positive?L"POS":L"NEG");
        error=expansion_build(source,command,log);
    }
    ProPath output=L"";
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"FIRST WALL REPLAY POSITIVE=%d ERROR=%d\n%ls\n",positive,error,report);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ImitationShapeMetrics stock;
    if(!error)error=imitation_shape_read(result,&stock);
    const double expected=2.5*(1.5*sqrt(3.0)*50.0*50.0-12.0*14.0-AS_PI*4.0*4.0);
    if(!error)fwprintf(log,L"MEASURED VOLUME %.12g EXPECTED %.12g\n",stock.volume,expected);
    /* The sketch solver and Creo mass integration use native model accuracy.
       The replay itself also applies its tighter source-to-result checks. */
    if(!error&&fabs(stock.volume-expected)>expected*1e-4)error=PRO_TK_INCOMPLETE;
    double mm=0,thickness=0;
    if(!error)error=fab_thickness(result,&mm,&thickness);
    if(!error&&fabs(thickness-2.5)>1e-6)error=PRO_TK_INCOMPLETE;
    if(!error){
        Pro3dPnt box[2];
        error=ProSolidOutlineGet((ProSolid)result,box);
        const double low=positive?0:-2.5,high=positive?2.5:0;
        if(!error)fwprintf(log,L"FIRST WALL BOUNDS %.9g %.9g %.9g / %.9g %.9g %.9g\n",
            box[0][0],box[0][1],box[0][2],box[1][0],box[1][1],box[1][2]);
        if(!error&&(fabs(box[0][2]*mm-low)>0.001||fabs(box[1][2]*mm-high)>0.001))error=PRO_TK_INCOMPLETE;
        if(!error)fwprintf(log,L"FIRST WALL MULTILOOP SIDE=%d VOLUME=%.10g THICKNESS=%.10g\n",
            positive,stock.volume,thickness);
    }
    return error;
}

static int first_wall_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-first-wall.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError positive=first_wall_trial(log,1);
    const ProError negative=first_wall_trial(log,0);
    fwprintf(log,L"POSITIVE=%d NEGATIVE=%d PASS=%d\n",positive,negative,!positive&&!negative);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayFirstWall",first_wall_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayFirstWall\x0060;");
}
