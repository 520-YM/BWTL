
#define user_initialize bwtl_order_fixture_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *order_log;
static int failed,checks;

static void receipt(int number,const wchar_t *detail,int condition){
    fwprintf(order_log,L"%03d %ls %ls\n",number,condition?L"PASS":L"FAIL",detail);
    fflush(order_log);
    failed+=!condition;
    checks++;
}

static int fixture(const wchar_t *name,const wchar_t *body,wchar_t path[MAX_PATH]){
    ProPath where;
    if(ProDirectoryCurrentGet(where)||swprintf_s(path,MAX_PATH,L"%ls\\%ls",where,name)<0)return 0;
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file)return 0;
    int good=fputws(body,file)>=0;
    return fclose(file)==0&&good;
}

typedef struct {double wanted;ProName name;} DimensionPick;
static ProError order_dim(ProDimension *dim,ProError status,ProAppData context){
    DimensionPick *pick=context;
    ProDimensiontype type;
    double value=0;
    if(!status&&!pick->name[0]&&!ProDimensionTypeGet(dim,&type)&&type==PRODIMTYPE_LINEAR&&
        !ProDimensionValueGet(dim,&value)&&fabs(value-pick->wanted)<0.001)
        ProDimensionSymbolGet(dim,pick->name);
    return PRO_TK_NO_ERROR;
}

static int order_cases(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    order_log=_wfopen(L"order-acceptance.log",L"wt, ccs=UTF-8");
    if(!order_log){ProEngineerEnd();return 0;}
    static wchar_t log[PART_TEXT_MAX];
    PartText out={log,_countof(log),0};
    const wchar_t *key=L"TEST_26001";
    ProMdl model=NULL;
    ProMdlName name=L"BWTL_ORDER_TEST";
    const char *stage="";
    ProError error=quick_seed(name,&model);
    const ModelPlan plan={.a=35,.b=25,.height=4,.ready=1};
    if(!error)error=model_build_internal(&plan,name,&stage,model,NULL,NULL);
    if(!error)error=ProMdlSave(model);
    if(error){
        fwprintf(order_log,L"FIXTURE_ERROR=%d\n",error);
        failed++;
        goto done;
    }
    double extent[3];
    int width=-1;
    for(int i=0;i<3;i++){
        wchar_t metric[]={L'B',L'O',L'X',L':',(wchar_t)(L'X'+i),0};
        if(!or_read_number(model,metric,&extent[i])&&fabs(extent[i]-35)<0.001)width=i;
    }
    if(width<0){failed++;goto done;}
    DimensionPick pick={.wanted=35};
    ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,order_dim,NULL,&pick);
    wchar_t input[MAX_PATH],map[MAX_PATH],body[560],tasks[MAX_PATH],guard[MAX_PATH],doc[MAX_PATH];
    int created=fixture(L"订单.txt",L"整机宽度＝忽略文字\n整机宽度：35 毫米\n屏幕尺寸=55\n安装型式=立式\n散热方式=风冷\n检修方向=右开\n材料厚度=4 mm\n",input);
    receipt(2,L"订单文字别名、毫米和来源行",created&&!or_import(key,input,&out)&&wcsstr(log,L"6 个"));
    created=fixture(L"机型.tsv",L"DEMO55\t55\t立式\t风冷\t右开\t屏框和风仓\nDEMO43\t43\t壁挂\t风冷\t左开\t壁挂支架\n",input);
    receipt(1,L"母型排名",created&&!or_templates(key,input,&out)&&wcsstr(log,L"10 分")&&wcsstr(log,L"DEMO55"));
    swprintf_s(body,_countof(body),L"外宽\t整机布局外宽\tBOX:%lc\t0.001\n",L'X'+width);
    created=fixture(L"映射.tsv",body,map);
    receipt(3,L"匹配别名、Creo 实测尺寸和公差",created&&!or_mapping(model,key,map,&out));
    swprintf_s(body,_countof(body),L"保留边\tBOX:%lc\t0.01\n",L'X'+width);
    created=fixture(L"保留项.tsv",body,guard);
    receipt(4,L"保存保护基线并重新测量",created&&!or_capture(model,key,guard,1,&out)&&!or_compare(model,key,1,&out));
    created=fixture(L"证据.tsv",L"外宽\t35 mm\t订单第2行\n外宽\t36 mm\tPDF第1页\n",input);
    receipt(5,L"尺寸冲突定位到两个出处",created&&or_conflicts(input,&out)==PRO_TK_INCOMPLETE&&wcsstr(log,L"PDF第1页"));
    receipt(6,L"独立副本两端尺寸实算",pick.name[0]&&!or_trial(model,pick.name,30,45,&out)&&wcsstr(log,L"45 mm 试算通过"));
    created=fixture(L"任务.tsv",L"OUT\t风仓\t交付\t出图\tASM\t1\nASM\t风仓\t装配\t安装\tPART\t0\nPART\t钣金\t改型\t改孔\t-\t0\n",tasks);
    receipt(7,L"按依赖自动将改型排在装配、交付之前",created&&!or_tasks(key,tasks,&out)&&wcsstr(log,L"1. 钣金"));
    created=fixture(L"循环.tsv",L"FIRST\t试件\t改型\t做\tSECOND\t1\nSECOND\t试件\t改型\t做\tFIRST\t0\n",tasks);
    receipt(7,L"拒绝循环依赖",created&&or_tasks(key,tasks,&out)==PRO_TK_BAD_INPUTS&&wcsstr(log,L"形成循环"));
    receipt(8,L"保存实测尺寸基线及版本",!or_capture(model,key,guard,0,&out));
    receipt(10,L"绑定正确目录，拒绝在同目录登记另一订单",!or_workspace(model,key,1,&out)&&
        !or_workspace(model,key,0,&out)&&or_workspace(model,L"TEST_26002",1,&out)==PRO_TK_E_IN_USE);
    created=fixture(L"交付证明.txt",L"独立验收夹具\n",doc);
    swprintf_s(body,_countof(body),L"001\t外宽\tBOX:%lc\t35\t0.001\t交付证明.txt\n",L'X'+width);
    created=created&&fixture(L"闭环.tsv",body,input);
    receipt(9,L"闭环单核对实测、已保存模型及订单目录中的交付物",created&&!or_closeout(model,key,input,&out)&&wcsstr(log,L"闭环 1/1"));
    if(pick.name[0]){
        UxDimension d;
        error=ux_dimension(model,pick.name,&d);
        if(!error)error=ProDimensionValueSet(&d.dim,38);
        if(!error)error=part_regenerate(model);
        receipt(4,L"实测改动导致保护项被阻止",!error&&or_compare(model,key,1,&out)==PRO_TK_INCOMPLETE&&
            or_closeout(model,key,input,&out)==PRO_TK_INCOMPLETE);
        receipt(8,L"记录 35 → 38 mm 的关键尺寸变化",!error&&!or_compare(model,key,0,&out)&&wcsstr(log,L"35 → 38"));
    }else receipt(8,L"实测改动",0);
    fwprintf(order_log,L"\n%ls\n",log);
done:
    fwprintf(order_log,L"ORDER_FINAL=%d CHECKS=%d\n",failed,checks);
    fclose(order_log);
    ProEngineerEnd();
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_order_fixture_init(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLOrderAcceptance",order_cases,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLOrderAcceptance\x0060;");
}
