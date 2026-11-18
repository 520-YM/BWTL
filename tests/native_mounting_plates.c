
#define user_initialize mount_toolkit_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *test_log;
static int failures,checks;

static void checked(int id,ProError status,const wchar_t *meaning){
    failures+=status!=PRO_TK_NO_ERROR;
    checks++;
    fwprintf(test_log,L"%03d %ls %ls error=%d\n",id,status?L"FAIL":L"PASS",meaning,status);
    fflush(test_log);
}

static ProError mount_tree(ProMdl model,const wchar_t *first,const wchar_t *second){
    ProFeature a,b;
    ProFeatStatus state;
    ProError error=cm_feature_find(model,first,&a);
    if(!error)error=cm_feature_find(model,second,&b);
    if(!error)error=ProFeatureStatusGet(&a,&state);
    if(!error&&state!=PRO_FEAT_ACTIVE)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProFeatureStatusGet(&b,&state);
    if(!error&&(state!=PRO_FEAT_ACTIVE||a.id==b.id))error=PRO_TK_BAD_CONTEXT;
    return error;
}

static int mount_accept(uiCmdCmdId id,uiCmdValue *value,void *data){
    test_log=_wfopen(L"mounting-acceptance.log",L"wt, ccs=UTF-8");
    if(!test_log){ProEngineerEnd();return 0;}
    static wchar_t report[PART_TEXT_MAX];
    PartText out={report,_countof(report),0};
    ProMdl strip=NULL,adapter=NULL,missing=NULL;
    const double slides[]={200,32,2,18,6.5,70,26,8.5};
    ProError error=mount_strip(L"BW_BACK_PANEL_SLIDE",slides,&strip,&out);
    if(!error)error=mount_tree(strip,L"MOUNT_END_HOLES",L"MOUNT_SLIDING_SLOTS");
    checked(11,error,L"真实钣金板保留端孔与横向调节槽两个独立特征，并通过尺寸和实际开口读回");
    const double pair[]={180,120,3,90,50,8,140,90,10};
    error=mount_adapter(L"BW_FLOOR_ADAPTER",pair,&adapter,&out);
    if(!error)error=mount_tree(adapter,L"EQUIPMENT_HOLES",L"FLOOR_ANCHOR_HOLES");
    checked(20,error,L"原生地脚板的设备孔与地脚孔分组创建并单独核对");
    const double conflict[]={180,120,3,90,50,8,90,50,10};
    error=mount_adapter(L"BW_OVERLAP_BAD",conflict,&missing,&out);
    checked(20,error==PRO_TK_BAD_INPUTS&&!missing?0:PRO_TK_INCOMPLETE,L"孔组覆盖同一位置时在建模前拒绝");
    const double bounds[]={200,32,2,3,10,90,26,8.5};
    error=mount_strip(L"BW_THROUGH_EDGE_BAD",bounds,&missing,&out);
    checked(11,error==PRO_TK_BAD_INPUTS&&!missing?0:PRO_TK_INCOMPLETE,L"端孔穿出板边时避免写入");
    fwprintf(test_log,L"\n%ls\nMOUNT_FINAL=%d CHECKS=%d\n",report,failures,checks);
    fclose(test_log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=mount_toolkit_init(argc,argv,version,build,buffer);
    uiCmdCmdId action;
    if(!error)error=ProCmdActionAdd("BWTLMountAcceptance",mount_accept,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&action);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLMountAcceptance\x0060;");
}
