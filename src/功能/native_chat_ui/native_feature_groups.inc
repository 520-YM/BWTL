/* Group the existing catalog by task. The source index remains the command ID. */
enum { FEATURE_LIST_COMMAND=0x7001 };

typedef struct {
    const wchar_t *label;
    const wchar_t *groups[8];
    const wchar_t *operations[9];
} NativeFeatureTab;

typedef struct {
    const wchar_t *label;
    NativeFeatureTab tabs[7];
} NativeFeatureSection;

static const NativeFeatureSection native_feature_sections[]={
    {L"零件建模",{
        {L"新建零件",{L"新建模型",L"常用零件快速生成"}},
        {L"基准与参照",{L"读取模型与参照",L"基准",L"高级基准与参照"}},
        {L"草绘",{L"关联草绘",L"高级草绘"}},
        {L"实体与曲面",{L"实体造型与加工",L"高级拉伸、扫描与放样",L"结构、抽壳与成型",L"曲面与多实体"}},
        {L"孔槽与加工",{L"常用加工",L"高级孔、槽与边缘加工",L"安装与固定"}}
    }},
    {L"钣金与机箱",{
        {L"机箱与安装",{L"钣金与机箱",L"机柜钣金结构",L"钣金车间 · 机箱与门板",L"安装板",L"散热、防水与走线"}},
        {L"成形与折弯",{L"钣金车间 · 成形",L"钣金车间 · 折弯工艺"},{L"空钣金零件"}},
        {L"开孔与连接",{L"面板开孔",L"钣金车间 · 紧固与焊接"}},
        {L"导入与展开",{L"钣金生产 · 导入与复建",L"钣金、展开与下料",L"钣金车间 · 展开与下料",L"钣金车间 · 板材与排样"}},
        {L"制造与检验",{L"钣金车间 · 可制造性",L"钣金车间 · 检验与交付"}}
    }},
    {L"装配与定位",{
        {L"命名与建件",{L"装配命名与建件",L"组件与子装配管理"},{L"空装配",L"在装配内新建空零件"}},
        {L"放置与约束",{L"装配约束与定位",L"装配定位与检查",L"阵列与紧固件",L"机箱与设备安装"},
            {L"按实际参照做平面角度定位",L"读取顶层装配",L"读取装配中已激活的零件"}},
        {L"布局与联动",{L"装配布局与尺寸联动",L"装配流程",L"装配专项：定位、配置与工序"}},
        {L"机构与现场加工",{L"活动机构与检修",L"装配加工与现场配作",L"装配专项：紧固与工艺",
            L"装配专项：机柜与检修",L"装配专项：线束与端口",L"装配专项：机架与现场"}},
        {L"视图与配置",{L"装配视图与产品配置"}}
    }},
    {L"修改与复用",{
        {L"尺寸与规格",{L"改尺寸与改规格"},{L"修改已有驱动尺寸",L"修改已有模型参数"}},
        {L"模型与特征维护",{L"修改、文件与装配",L"读模型与整理模型",L"特征维护与零件交付"}},
        {L"特征与参数复用",{L"特征复用与参数化"}}
    }},
    {L"检查与交付",{
        {L"零件检查",{L"设计与加工检查",L"零件检测与分析"}},
        {L"装配检查",{L"装配质量检查",L"装配专项：检查与交付"},
            {L"136 组件最小间隙",L"137 实体干涉检查",L"138 装配质量属性"}},
        {L"加工与文件交付",{L"加工与交付",L"装配 BOM 与交付"},
            {L"139 导出装配 BOM",L"140 导出装配 STEP",L"128 批量导出三维文件",L"129 生成零件加工信息表",
             L"130 打包零件交付文件",L"保存零件或装配",L"导出 STEP"}}
    }},
    {L"公司标准与订单",{
        {L"标准接口",{L"公司标准接口"}},
        {L"订单改型",{L"订单改型"}}
    }},
    {L"对话与任务",{
        {L"对话与图片",{L"对话与图片"}},
        {L"聊天记录与记忆",{L"聊天记录与记忆"}},
        {L"任务控制",{L"任务控制"}}
    }}
};

static HMENU native_feature_add_submenu(HMENU parent,const wchar_t *label){
    HMENU submenu=CreatePopupMenu();
    if(submenu&&!AppendMenuW(parent,MF_POPUP,(UINT_PTR)submenu,label)){
        DestroyMenu(submenu);
        return NULL;
    }
    return submenu;
}

static int native_feature_append(HMENU menu,size_t index){
    const NativeFeature *feature=&native_features[index];
    int enabled=feature->draft[0]?native_busy==0:native_busy!=0;
    return AppendMenuW(menu,MF_STRING|(enabled?0:MF_GRAYED),(UINT_PTR)(index+1),feature->label)!=0;
}

/* Reserve explicitly moved operations before appending their former groups. */
static int native_feature_visit(int (*visit)(size_t section,size_t tab,size_t index,void *data),void *data){
    unsigned char moved[_countof(native_features)]={0};
    unsigned char inserted[_countof(native_features)]={0};
    for(size_t s=0;s<_countof(native_feature_sections);s++){
        for(size_t t=0;t<_countof(native_feature_sections[s].tabs)&&native_feature_sections[s].tabs[t].label;t++){
            const NativeFeatureTab *tab=&native_feature_sections[s].tabs[t];
            for(size_t op=0;op<_countof(tab->operations)&&tab->operations[op];op++){
                size_t i=0;
                for(;i<_countof(native_features);i++){
                    if(native_features[i].draft&&!wcscmp(native_features[i].label,tab->operations[op]))break;
                }
                if(i==_countof(native_features)||moved[i])return 0;
                moved[i]=1;
            }
        }
    }
    for(size_t s=0;s<_countof(native_feature_sections);s++){
        for(size_t t=0;t<_countof(native_feature_sections[s].tabs)&&native_feature_sections[s].tabs[t].label;t++){
            const NativeFeatureTab *tab=&native_feature_sections[s].tabs[t];
            for(size_t g=0;g<_countof(tab->groups)&&tab->groups[g];g++){
                size_t i=0;
                for(;i<_countof(native_features);i++){
                    if(!native_features[i].draft&&!wcscmp(native_features[i].label,tab->groups[g]))break;
                }
                if(i==_countof(native_features))return 0;
                for(++i;i<_countof(native_features)&&native_features[i].draft;i++){
                    if(moved[i])continue;
                    if(inserted[i]||!visit(s,t,i,data))return 0;
                    inserted[i]=1;
                }
            }
            for(size_t op=0;op<_countof(tab->operations)&&tab->operations[op];op++){
                for(size_t i=0;i<_countof(native_features);i++){
                    if(moved[i]&&!wcscmp(native_features[i].label,tab->operations[op])){
                        if(inserted[i]||!visit(s,t,i,data))return 0;
                        inserted[i]=1;
                        break;
                    }
                }
            }
        }
    }
    for(size_t i=0;i<_countof(native_features);i++){
        if(native_features[i].draft&&!inserted[i])return 0;
    }
    return 1;
}

typedef HMENU NativeFeatureTabMenus[_countof(native_feature_sections)][_countof(native_feature_sections[0].tabs)];

static int native_feature_add_operation(size_t section,size_t tab,size_t index,void *data){
    NativeFeatureTabMenus *menus=data;
    return native_feature_append((*menus)[section][tab],index);
}
static HMENU native_feature_menu_create(void){
    HMENU menu=CreatePopupMenu();
    NativeFeatureTabMenus tabs={0};
    if(!menu)return NULL;
    if(!AppendMenuW(menu,MF_STRING,FEATURE_LIST_COMMAND,L"功能列表")||
       !AppendMenuW(menu,MF_SEPARATOR,0,NULL))goto failed;
    for(size_t s=0;s<_countof(native_feature_sections);s++){
        const NativeFeatureSection *section=&native_feature_sections[s];
        HMENU parent=native_feature_add_submenu(menu,section->label);
        if(!parent)goto failed;
        for(size_t t=0;t<_countof(section->tabs)&&section->tabs[t].label;t++){
            tabs[s][t]=native_feature_add_submenu(parent,section->tabs[t].label);
            if(!tabs[s][t])goto failed;
        }
    }
    if(native_feature_visit(native_feature_add_operation,&tabs))return menu;
failed:
    DestroyMenu(menu);
    return NULL;
}
