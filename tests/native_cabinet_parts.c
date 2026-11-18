
#define user_initialize cbp_toolkit_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *receipt;
static int failed, checked;

static void cbp_test(int feature,ProError error,const wchar_t *label){
    fwprintf(receipt,L"%03d %ls %ls error=%d\n",feature,error?L"FAIL":L"PASS",label,error);
    fflush(receipt);
    failed+=error!=0;
    checked++;
}

static ProError cbp_run_request(const wchar_t *text,ProMdl *model,PartText *out){
    CabinetPartRequest request={0};
    if(cbp_parse(text,&request)!=1)return PRO_TK_BAD_INPUTS;
    return cbp_execute(&request,model,out);
}

static ProError cbp_require_feature(ProMdl model,const wchar_t *name){
    ProFeature native;
    return cm_feature_find(model,name,&native);
}

static ProError cbp_saved_reopen(ProMdl *model,const wchar_t *expected){
    ProPath path;
    ProError status=ProMdlSave(*model);
    if(!status)status=ProMdlOriginGet(*model,path);
    if(!status)status=ProMdlErase(*model);
    if(status)return status;
    *model=NULL;
    status=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,model);
    if(!status)status=part_health(*model);
    if(!status)status=cbp_require_feature(*model,expected);
    return status;
}

static int cbp_accept(uiCmdCmdId id,uiCmdValue *value,void *data){
    receipt=_wfopen(L"cabinet-parts-acceptance.log",L"wt, ccs=UTF-8");
    if(!receipt){ProEngineerEnd();return 0;}
    static wchar_t details[PART_TEXT_MAX];
    PartText out={details,_countof(details),0};
    ProMdl model=NULL;
    ProError status=cbp_run_request(L"新建防雨折边压条 BW_RAIN_TEST 280 22 8 1.5 1.5 18 2 8 11",&model,&out);
    if(!status)status=cbp_saved_reopen(&model,L"RAIN_RETURN_LIP");
    cbp_test(12,status,L"原生单折弯和 8/11 mm 两个排水端开口，保存后重开");
    model=NULL;
    status=cbp_run_request(L"新建导向轮可调底板 BW_ROLLER_X X+ 114 55 2 32 34 8 5 17 2",&model,&out);
    if(!status)status=cbp_saved_reopen(&model,L"ROLLER_TRAVEL_STOP");
    if(!status)status=cbp_require_feature(model,L"TRAVEL_MARK_06");
    cbp_test(18,status,L"X 正向双长槽、止挡和六个可测刻度基准点，保存后重开");
    model=NULL;
    status=cbp_run_request(L"新建导向轮可调底板 BW_ROLLER_Y Y- 120 58 2 32 32 8 6 16 2",&model,&out);
    if(!status)status=cbp_require_feature(model,L"TRAVEL_MARK_05");
    cbp_test(18,status,L"旋转至 Y 负向，槽方向和实测刻度随动");
    model=NULL;
    status=cbp_run_request(L"新建多段过渡连接板 BW_STEP_TWO 64 28 0 30 1.5 1.5 0 18 34 32 6",&model,&out);
    if(!status)status=cbp_saved_reopen(&model,L"HIGH_INTERFACE_HOLES");
    if(!status)status=cbp_require_feature(model,L"LOW_INTERFACE_HOLES");
    cbp_test(14,status,L"两平台实测 18 mm 高差，独立低侧、高侧孔组及重开");
    model=NULL;
    status=cbp_run_request(L"新建多段过渡连接板 BW_STEP_THREE 76 38 28 32 2 2 14 31 40 34 6",&model,&out);
    if(!status)status=cbp_require_feature(model,L"LEVEL_MID");
    if(!status)status=cbp_require_feature(model,L"LEVEL_HIGH");
    cbp_test(14,status,L"三平台完成四道折弯，实际逐级 14/31 mm");
    CabinetPart source={0};
    status=cbp_seed(&source,L"BW_HINGE_SOURCE",60,18,12,0);
    if(!status)status=cbp_run_request(L"新建快拆铰链护架 BW_HINGE_GUARD BW_HINGE_SOURCE X 22 20 5 50 16 12 5.5 2 2",&model,&out);
    if(!status)status=cbp_saved_reopen(&model,L"SCREW_SERVICE_WINDOW");
    if(!status)status=cbp_require_feature(model,L"GUARD_MOUNTING_HOLES");
    cbp_test(19,status,L"读取原生铰链模型包络，核对拔销通道、螺钉窗和四个安装孔并重开");
    model=NULL;
    status=cbp_run_request(L"新建防雨折边压条 BW_BAD_DRAIN 20 22 8 1 2 12 2 11 11",&model,&out);
    cbp_test(12,status==PRO_TK_BAD_INPUTS&&!model?0:PRO_TK_INCOMPLETE,L"排水开口相撞时拒绝创建");
    model=NULL;
    status=cbp_run_request(L"新建导向轮可调底板 BW_BAD_SCALE X+ 90 40 2 20 34 8 30 14 2",&model,&out);
    cbp_test(18,status==PRO_TK_BAD_INPUTS&&!model?0:PRO_TK_INCOMPLETE,L"超出实测滑程的刻度距被拒绝");
    status=cbp_run_request(L"新建风仓可拆内隔板 BW_AIR_SLOT X+ SLOTS 180 90 2 20 16 24 14 6 18 94 5",&model,&out);
    if(!status)status=cbp_saved_reopen(&model,L"BAFFLE_FASTENERS");
    if(!status)status=cbp_require_feature(model,L"BAFFLE_CORNER_CLEARANCE");
    CabinetPart saved_baffle={.txn={.model=model,.scale=1},.width=180,.depth=90,.thickness=2};
    if(!status)status=cbp_baffle_withdraw_points(&saved_baffle,1,0);
    cbp_test(15,status,L"转角、长槽和抽出基准保存重开");
    model=NULL;
    status=cbp_run_request(L"新建风仓可拆内隔板 BW_AIR_HOLES X- HOLES 140 80 1.5 18 14 20 13 5 0 86 4",&model,&out);
    cbp_test(15,status,L"反向抽出，分别读回圆孔与转角");
    model=NULL;
    status=cbp_run_request(L"新建风仓可拆内隔板 BW_AIR_NARROW X+ SLOTS 180 90 2 20 16 24 14 6 18 85 5",&model,&out);
    cbp_test(15,status==PRO_TK_BAD_INPUTS&&!model?0:PRO_TK_INCOMPLETE,L"检修口比隔板窄，创建前拒绝");
    fwprintf(receipt,L"\n%ls\nCABINET_PARTS_FINAL=%d CHECKS=%d\n",details,failed,checked);
    fclose(receipt);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError status=cbp_toolkit_init(argc,argv,version,build,buffer);
    uiCmdCmdId command;
    if(!status)status=ProCmdActionAdd("BWTLCabinetPartsAcceptance",cbp_accept,uiProe2ndImmediate,NULL,
        PRO_B_TRUE,PRO_B_TRUE,&command);
    return status?status:ProMacroLoad(L"~ Command \x0060" L"BWTLCabinetPartsAcceptance\x0060;");
}
