#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProError tree_group_fixture(ProMdl *model,int fewer,FILE *log)
{
    ProMdlName name;
    Fixture stock;
    wchar_t command[512];
    ProError error=fixture(model,name,&stock);
    for(int i=0;!error&&i<3;i++){
        swprintf_s(command,_countof(command),L"CREO_OP HOLE %ls TREE_%lc SURFACE:%d SURFACE:%d %d SURFACE:%d 25 6 THROUGH 0",
            name,L'A'+i,stock.top,stock.x,15+18*i,stock.y);
        error=expansion_build(*model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),fewer?
            L"CREO_OP REUSE_GROUP %ls TREE_GROUP TREE_A TREE_B":
            L"CREO_OP REUSE_GROUP %ls TREE_GROUP TREE_A TREE_B TREE_C",name);
        error=expansion_build(*model,command,log);
    }
    return error;
}

static ProError tree_sketch_fixture(ProMdl *model,int alternate,FILE *log)
{
    ProMdlName name;
    const char *stage="prepare sketch ownership fixture";
    wchar_t command[512];
    ProError error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,model);
    for(int i=0;!error&&i<2;i++){
        swprintf_s(command,_countof(command),L"CREO_OP SKETCH_CREATE %ls SECTION_%lc PLANE:FRONT PLANE:RIGHT RECT 10 10 15 18",
            name,L'A'+i);
        error=expansion_build(*model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP SOLID_EXTRUDE %ls TREE_ADD SECTION_%lc ADD BLIND 12 POS",
            name,alternate?L'B':L'A');
        error=expansion_build(*model,command,log);
    }
    return error;
}

static ProError tree_verification_trial(int sketch,FILE *log)
{
    ProError (*build)(ProMdl *,int,FILE *)=sketch?tree_sketch_fixture:tree_group_fixture;
    ProMdl original=NULL,alternative=NULL;
    ProError error=build(&original,0,log);
    if(!error)error=build(&alternative,1,log);
    if(!error)error=ProMdlSave(original);
    if(!error)error=ProMdlSave(alternative);
    ProPath output=L"";
    wchar_t text[16000]=L"";
    PartText out={text,_countof(text),0};
    if(!error)error=imitation_run_mode(original,output,0,1,&out);
    fwprintf(log,L"TREE REPLAY SKETCH=%d ERROR=%d\n%ls\n",sketch,error,text);
    text[0]=0;
    out.truncated=0;
    ImitationShapeMetrics shape;
    if(!error)error=imitation_shape_read(original,&shape);
    if(!error)error=imitation_shape_verify_mode(original,alternative,&shape,0,&out);
    fwprintf(log,L"ALTERNATE GEOMETRY SKETCH=%d ERROR=%d\n",sketch,error);
    if(!error){
        error=replay_verify_order(original,alternative,&out);
        fwprintf(log,L"ALTERNATE TREE SKETCH=%d ERROR=%d\n%ls\n",sketch,error,text);
        error=error==PRO_TK_INCOMPLETE&&wcsstr(text,L"所属特征组或草绘归属不同")?
            PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
    }
    return error;
}

static int tree_verification_case(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-tree-verification.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 1;}
    model_background_mode=1;
    ProError groups=tree_verification_trial(0,log);
    ProError sketches=tree_verification_trial(1,log);
    fwprintf(log,L"GROUPS=%d SKETCHES=%d PASS=%d\n",groups,sketches,!groups&&!sketches);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLTreeVerification",tree_verification_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLTreeVerification\x0060;");
}
