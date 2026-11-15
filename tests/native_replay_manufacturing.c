#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

static ProError manufacturing_exclusion(ProMdl source,const wchar_t *name,const Fixture *faces,FILE *log)
{
    ProError error=batch_step(source,log,L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 15 SURFACE:%d 18 6 THROUGH 0",
        name,faces->top,faces->x,faces->y);
    if(!error)error=batch_step(source,log,L"CREO_OP REUSE_GRID %ls GRID SEED PLANE:RIGHT 3 16 POS PLANE:TOP 2 16 POS",name);
    if(!error)error=batch_step(source,log,L"CREO_OP REUSE_SKIP %ls GRID 2 SKIP",name);
    return error;
}

static ProError manufacturing_case(int kind,FILE *log)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&source,name,&faces);
    if(!error&&kind<2)error=batch_step(source,log,L"CREO_OP SOLID_DRAFT %ls DRAFT_A SURFACE:%d SURFACE:%d PLANE:FRONT 5 %ls",
        name,faces.xfar,faces.bottom,kind?L"NEG":L"POS");
    if(!error&&(kind==2||kind==4))error=batch_step(source,log,L"CREO_OP DATUM_PLANE %ls NEXT_MID PLANE:FRONT 10",name);
    if(!error&&(kind==2||kind==4))error=batch_step(source,log,L"CREO_OP SKETCH_CREATE %ls NEXT_SK PLANE:NEXT_MID PLANE:RIGHT RECT 8 8 5 4",name);
    if(!error&&kind==2)error=batch_step(source,log,L"CREO_OP SOLID_EXTRUDE_TO %ls NEXT_END NEXT_SK CUT NEXT NEG",name);
    if(!error&&kind==3)error=manufacturing_exclusion(source,name,&faces,log);
    if(!error&&kind==4)error=batch_step(source,log,L"CREO_OP SOLID_EXTRUDE %ls NEXT_END NEXT_SK CUT THROUGH 0 NEG",name);
    if(!error)error=batch_reopen(source,&result,log);
    if(!error&&kind<2)error=batch_edit(result,L"DRAFT_A",PRODIMTYPE_ANGLE,5,6,log);
    if(!error&&kind==2)error=batch_edit(result,L"NEXT_MID",PRODIMTYPE_LINEAR,10,11,log);
    if(!error&&kind==3){
        ProMdlName rebuilt;
        error=ProMdlMdlnameGet(result,rebuilt);
        if(!error)error=batch_step(result,log,L"CREO_OP REUSE_SKIP %ls GRID 2 RESTORE",rebuilt);
    }
    if(result)ProMdlErase(result);
    if(source)ProMdlErase(source);
    fwprintf(log,L"MANUFACTURING CASE %d ERROR=%d\n",kind,error);
    return error;
}

static int manufacturing_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-manufacturing.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    int pass=0;
    for(int i=0;i<5;i++)pass+=!manufacturing_case(i,log);
    fwprintf(log,L"MANUFACTURING CASES=%d PASS=%d\n",pass,pass==5);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayManufacturing",manufacturing_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLReplayManufacturing`;");
}
