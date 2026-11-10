#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>

static void sx_test_bounds(ProMdl model,int axis,double low,double high){
    Pro3dPnt outline[2];
    ProError e=ProSolidOutlineGet((ProSolid)model,outline);
    if(!e&&(!part_close(outline[0][axis],low)||!part_close(outline[1][axis],high))){
        trace("actual lower bound x1000",(int)lround(outline[0][axis]*1000));
        trace("actual upper bound x1000",(int)lround(outline[1][axis]*1000));
        e=PRO_TK_GENERAL_ERROR;
    }
    trace("verify actual extrusion endpoints",e);cases++;if(e)failures++;
}

static void sx_test_volume(ProMdl model,double expected,double tolerance){
    ProMassProperty mass;
    ProError e=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);
    if(!e&&fabs(mass.volume-expected)>tolerance){trace("actual volume x1000",(int)lround(mass.volume*1000));e=PRO_TK_GENERAL_ERROR;}
    trace("verify measured curved volume",e);cases++;if(e)failures++;
}

static int sx_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL;ProMdlName name;
    wchar_t text[1600];
    const char *stage="create empty advanced solid fixture";
    Fixture block;
    ProError e;
    model_background_mode=1;
#define EMPTY() do{if((e=model_create_empty(1,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){trace(stage,e);failures++;goto done;}}while(0)
#define CASE(reject,format,...) do{swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__);run(model,text,reject);}while(0)
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls SYM_SK PLANE:FRONT PLANE:RIGHT RECT 2 3 8 10",name);
    CASE(0,L"SOLID_EXTRUDE_SYM %ls SYMMETRIC_ADD SYM_SK ADD 6",name);
    expect_volume(model,960);sx_test_bounds(model,2,-6,6);
    CASE(0,L"SKETCH_CREATE %ls SYM_CUT_SK PLANE:FRONT PLANE:RIGHT CIRCLE 6 8 1",name);
    CASE(0,L"SOLID_EXTRUDE_SYM %ls SYMMETRIC_CUT SYM_CUT_SK CUT 10",name);
    expect_volume(model,960-12*3.141592653589793);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls TWO_SK PLANE:FRONT PLANE:RIGHT RECT 2 3 8 10",name);
    CASE(0,L"SOLID_EXTRUDE_TWO %ls TWO_SIDED TWO_SK ADD 7 3 POS",name);
    expect_volume(model,800);sx_test_bounds(model,2,-7,3);
    EMPTY();
    CASE(0,L"DATUM_PLANE %ls END_POS PLANE:FRONT 15",name);
    CASE(0,L"SKETCH_CREATE %ls REF_SK PLANE:FRONT PLANE:RIGHT RECT 2 3 8 10",name);
    CASE(0,L"SOLID_EXTRUDE_TO %ls REF_END REF_SK ADD PLANE:END_POS POS",name);
    expect_volume(model,1200);sx_test_bounds(model,2,0,15);
    if((e=fixture(&model,name,&block))){trace("block for next face",e);failures++;goto done;}
    CASE(0,L"DATUM_PLANE %ls NEXT_MID PLANE:FRONT 10",name);
    CASE(0,L"SKETCH_CREATE %ls NEXT_SK PLANE:NEXT_MID PLANE:RIGHT RECT 8 8 5 4",name);
    CASE(0,L"SOLID_EXTRUDE_TO %ls NEXT_END NEXT_SK CUT NEXT NEG",name);
    expect_volume(model,95800);
    CASE(0,L"SKETCH_CREATE %ls FAR_SK PLANE:NEXT_MID PLANE:RIGHT RECT 25 8 5 4",name);
    CASE(0,L"SOLID_EXTRUDE_TO %ls FACE_END FAR_SK CUT SURFACE:%d POS",name,block.top);
    expect_volume(model,95600);
    CASE(0,L"SKETCH_CREATE %ls NO_FACE_SK PLANE:NEXT_MID PLANE:RIGHT CIRCLE 200 200 2",name);
    CASE(1,L"SOLID_EXTRUDE_TO %ls NO_FACE NO_FACE_SK CUT NEXT POS",name);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls THIN_SK PLANE:FRONT PLANE:RIGHT LINE 0 0 20 0",name);
    CASE(0,L"SOLID_EXTRUDE_THIN %ls WALL THIN_SK 8 2 SYM POS",name);
    expect_volume(model,320);sx_test_bounds(model,2,-8,0);
    CASE(1,L"SOLID_EXTRUDE_SYM %ls BAD_CLOSED THIN_SK ADD 8",name);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls MIX_PATH PLANE:FRONT PLANE:RIGHT ARC 0 0 30 0 90",name);
    CASE(0,L"SKETCH_ADD %ls MIX_PATH LINE 0 30 -30 30",name);
    CASE(0,L"SOLID_SWEEP_CHAIN %ls CHAIN_SWEEP CURVE:MIX_PATH 4",name);
    expect_volume(model,3.141592653589793*4*(30+15*3.141592653589793));
    CASE(0,L"SKETCH_CREATE %ls SPLIT_PATH PLANE:FRONT PLANE:RIGHT LINE 0 0 10 0",name);
    CASE(0,L"SKETCH_ADD %ls SPLIT_PATH LINE 20 0 30 0",name);
    CASE(1,L"SOLID_SWEEP_CHAIN %ls BAD_PATH CURVE:SPLIT_PATH 2",name);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls STRAIGHT PLANE:FRONT PLANE:RIGHT LINE 0 0 40 0",name);
    CASE(0,L"SOLID_SWEEP_PROFILE %ls RECT_SWEEP CURVE:STRAIGHT RECT 6 4",name);
    expect_volume(model,960);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls STRAIGHT PLANE:FRONT PLANE:RIGHT LINE 0 0 40 0",name);
    CASE(0,L"SOLID_SWEEP_PROFILE %ls ELLIPSE_SWEEP CURVE:STRAIGHT ELLIPSE 8 4",name);
    sx_test_volume(model,320*3.141592653589793,0.05);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls STRAIGHT PLANE:FRONT PLANE:RIGHT LINE 0 0 40 0",name);
    CASE(0,L"SKETCH_CREATE %ls PROFILE_SK PLANE:RIGHT PLANE:TOP RECT -3 -2 6 4",name);
    CASE(0,L"SOLID_SWEEP_PROFILE %ls USER_SWEEP CURVE:STRAIGHT SKETCH PROFILE_SK",name);
    expect_volume(model,960);
    if((e=fixture(&model,name,&block))){trace("block for sweep cut",e);failures++;goto done;}
    CASE(0,L"DATUM_PLANE %ls CUT_LEVEL PLANE:FRONT 10",name);
    CASE(0,L"SKETCH_CREATE %ls CUT_PATH PLANE:CUT_LEVEL PLANE:RIGHT LINE -5 25 85 25",name);
    CASE(0,L"SOLID_SWEEP_CUT %ls SWEEP_CUT CURVE:CUT_PATH CIRCLE 4",name);
    expect_volume(model,96000-320*3.141592653589793);
    CASE(0,L"SKETCH_CREATE %ls NO_CUT_PATH PLANE:CUT_LEVEL PLANE:RIGHT LINE -5 100 85 100",name);
    CASE(1,L"SOLID_SWEEP_CUT %ls NO_CUT CURVE:NO_CUT_PATH CIRCLE 4",name);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls TAPER_PATH PLANE:FRONT PLANE:RIGHT LINE 3 7 43 7",name);
    CASE(0,L"SOLID_SWEEP_VARIABLE %ls TAPER_SWEEP CURVE:TAPER_PATH 10 6",name);
    expect_volume(model,40*3.141592653589793*(25+15+9)/3);
    sx_test_bounds(model,0,3,43);
    EMPTY();
    CASE(0,L"SOLID_HELIX %ls SPRING ADD CSYS:PRT_CSYS_DEF 30 12 2 RH CIRCLE 3",name);
    sx_test_volume(model,hypot(2*3.141592653589793*30,24)*3.141592653589793*2.25,0.5);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls SHAFT_SK PLANE:FRONT PLANE:RIGHT CIRCLE 0 0 15",name);
    CASE(0,L"SOLID_EXTRUDE_SYM %ls SHAFT SHAFT_SK ADD 30",name);
    CASE(0,L"SOLID_HELIX %ls GROOVE CUT CSYS:PRT_CSYS_DEF 30 10 2.25 LH CIRCLE 3",name);
    CASE(1,L"SOLID_HELIX %ls CROSSED ADD CSYS:PRT_CSYS_DEF 30 1 2 RH CIRCLE 3",name);
    EMPTY();
    CASE(0,L"DATUM_PLANE %ls LOFT_MID PLANE:FRONT 15",name);
    CASE(0,L"DATUM_PLANE %ls LOFT_END PLANE:FRONT 40",name);
    CASE(0,L"SKETCH_CREATE %ls LOFT_A PLANE:FRONT PLANE:RIGHT CIRCLE 0 0 10",name);
    CASE(0,L"SKETCH_CREATE %ls LOFT_B PLANE:LOFT_MID PLANE:RIGHT CIRCLE 0 0 8",name);
    CASE(0,L"SKETCH_CREATE %ls LOFT_C PLANE:LOFT_END PLANE:RIGHT CIRCLE 0 0 6",name);
    CASE(0,L"SOLID_LOFT %ls THREE_SECTION LOFT_A LOFT_B LOFT_C",name);
    sx_test_volume(model,3.141592653589793*(15*(100+80+64)+25*(64+48+36))/3,0.5);
    EMPTY();
    CASE(0,L"DATUM_PLANE %ls LOFT_MID PLANE:FRONT 15",name);
    CASE(0,L"DATUM_PLANE %ls LOFT_END PLANE:FRONT 40",name);
    CASE(0,L"SKETCH_CREATE %ls LOFT_A PLANE:FRONT PLANE:RIGHT RECT -10 -8 20 16",name);
    CASE(0,L"SKETCH_CREATE %ls LOFT_B PLANE:LOFT_MID PLANE:RIGHT SLOT 0 0 18 12 0",name);
    CASE(0,L"SKETCH_CREATE %ls LOFT_C PLANE:LOFT_END PLANE:RIGHT RECT -7 -5 14 10",name);
    CASE(1,L"SOLID_LOFT %ls OUT_OF_ORDER LOFT_A LOFT_C LOFT_B",name);
    CASE(0,L"SOLID_LOFT %ls MIXED_SECTION LOFT_A LOFT_B LOFT_C",name);
done:
    trace("ADVANCED_SOLID_CASES",cases);trace("ADVANCED_SOLID_FAILURES",failures);trace("ADVANCED_SOLID_DONE",1);
    ProEngineerEnd();return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId command;ProError e;char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=ProCmdActionAdd("BWTLAdvancedSolidProbe",sx_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command)))return e;
    return ProMacroLoad(L"~ Command `BWTLAdvancedSolidProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}

