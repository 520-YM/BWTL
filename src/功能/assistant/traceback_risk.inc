typedef struct {
    int number;
    const wchar_t *verb;
    const wchar_t *label;
    const wchar_t *history;
} BwtlTracebackRisk;

static const BwtlTracebackRisk bwtl_traceback_risks[]={
    {26,L"SOLID_DRAFT",L"单面定角拔模",L"添加拔模面参照或释放面集合时曾触发 Creo 回溯。"},
    {52,L"SOLID_EXTRUDE_TO",L"拉伸到下一面",L"查找下一面并释放旧版射线选取结果时曾触发 Creo 回溯。"},
    {93,L"SMT_FLANGE",L"边法兰",L"失败回滚释放草绘或处理不完整附加壁时曾触发 Creo 回溯。"},
    {155,L"CASE_GAP",L"相邻翻边留缝",L"直接重定义已有翻边时曾触发 Creo 回溯。"},
    {208,L"CHK_SHARP",L"边缘尖角检查",L"生成检查结果文本时曾触发 Creo 回溯。"},
    {254,L"ASM_REPLACE_COMPONENT",L"替换组件并保留定位",L"连续替换并重定义旧组件时曾触发 Creo 回溯。"},
    {260,L"ASM_REPAIR_REF",L"修复失效的定位参照",L"重定义已有组件约束时曾触发 Creo 回溯。"},
    {281,L"ASM_JOINT_PIN",L"建立销轴连接",L"同一装配重复创建原生销轴连接时曾触发 Creo 回溯。"},
    {282,L"ASM_JOINT_SLIDER",L"建立滑块连接",L"在同一装配已有原生销轴后创建滑块时曾触发 Creo 回溯。"},
    {283,L"ASM_JOINT_CYLINDER",L"建立圆柱连接",L"在同一装配已有原生滑块后创建圆柱连接时曾触发 Creo 回溯。"},
    {302,L"ASM_FIELD_HOLE",L"创建装配通孔",L"做多层通孔并重定义装配草绘或去料特征时曾触发 Creo 回溯。"},
    {400,L"SMY_FORM_FLAT",L"单成形特征展开与切口投影",L"旧版释放显式选取的成形参照时曾触发 Creo 回溯。"},
};

static const BwtlTracebackRisk *bwtl_traceback_risk(const PartCommand *command){
    if(command->verb!=PART_FOUNDATION)return NULL;
    for(size_t i=0;i<_countof(bwtl_traceback_risks);i++){
        const BwtlTracebackRisk *risk=&bwtl_traceback_risks[i];
        if(wcscmp(command->arg[0],risk->verb))continue;
        if(risk->number==52&&(command->count<=5||wcscmp(command->arg[5],L"NEXT")))return NULL;
        return risk;
    }
    return NULL;
}

static void bwtl_warn_traceback(const PartCommand *command){
    const BwtlTracebackRisk *risk=bwtl_traceback_risk(command);
    if(!risk)return;
    wchar_t message[512];
    swprintf_s(message,_countof(message),L"【回溯风险重点提醒】第 %d 项 %ls。%ls 当前发布版已调整并通过对应样件复测。",
        risk->number,risk->label,risk->history);
    append_card(L"提示",message);
    save_turn(L"回溯风险",message);
    trace("Traceback risk notice before execution",risk->number);
}
