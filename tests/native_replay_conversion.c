#define user_initialize replay_conversion_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProError conversion_command(ProMdl model,const wchar_t *protocol){
    PartCommand command={0};
    wchar_t response[PART_TEXT_MAX]=L"";
    return part_command_parse(protocol,&command)==1?
        foundation_execute(&command,model,response,_countof(response)):PRO_TK_BAD_INPUTS;
}

static int conversion_case(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"conversion-acceptance.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProMdl source=NULL;
    ProMdlName name;
    const char *stage="blank";
    ProPath output=L"";
    wchar_t text[10000]=L"",protocol[512];
    PartText out={text,_countof(text),0};
    model_background_mode=1;
    ProError error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    ImitationReplay replay={0};
    replay.target=source;
    ProFeature converted;
    if(!error)error=replay_convert_empty(&replay,L"CONVERT_EMPTY",&converted);
    fwprintf(log,L"CONVERT=%d\n",error);
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SMT_SKETCH %ls PLATE_OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name);
        error=conversion_command(source,protocol);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SMT_FIRST_WALL %ls PLATE_WALL SKETCH:PLATE_OUTLINE 2 POS",name);
        error=conversion_command(source,protocol);
    }
    fwprintf(log,L"SOURCE=%d\n",error);
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"REPLAY=%d\n%ls\n",error,text);
    fwprintf(log,L"PASS=%d\n",!error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=replay_conversion_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLConversionCase",conversion_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLConversionCase\x0060;");
}
