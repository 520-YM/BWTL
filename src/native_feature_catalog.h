#ifndef BWTL_NATIVE_FEATURE_CATALOG_H
#define BWTL_NATIVE_FEATURE_CATALOG_H

enum { FEATURE_STOP=-2, FEATURE_KEEP_MODE=-1, FEATURE_SMART, FEATURE_ANALYZE, FEATURE_DEVELOP };
typedef struct {
    const wchar_t *label;
    const wchar_t *draft; /* A null draft starts a category. */
    int mode;
} NativeFeature;

static const NativeFeature native_features[]={
    {L"对话与图片",NULL,0},
    {L"智能协作",L"请理解以下需求：〔待填：需求〕",FEATURE_SMART},
    {L"只分析",L"只分析以下需求：〔待填：需求〕",FEATURE_ANALYZE},
    {L"分析所附图片",L"请分析所附图片。",FEATURE_ANALYZE},
    {L"添加图片",L"添加图片",FEATURE_KEEP_MODE},
    {L"清除图片",L"清除图片",FEATURE_KEEP_MODE},
    {L"聊天记录与记忆",NULL,0},
    {L"查看历史",L"查看历史",FEATURE_SMART},
    {L"查看记忆",L"查看记忆",FEATURE_SMART},
    {L"记住一条偏好",L"记住：〔待填：单行偏好〕",FEATURE_SMART},
    {L"忘记一条偏好",L"忘记：〔待填：原记忆内容〕",FEATURE_SMART},
    {L"新对话",L"新对话",FEATURE_SMART},
    {L"读取模型与参照",NULL,0},
    {L"基本信息（尺寸、材料、重量）",L"读取当前模型",FEATURE_ANALYZE},
    {L"完整读取",L"读取详细信息",FEATURE_ANALYZE},
    {L"详细尺寸",L"读取详细尺寸",FEATURE_ANALYZE},
    {L"特征",L"读取特征",FEATURE_ANALYZE},
    {L"模型参数",L"读取参数",FEATURE_ANALYZE},
    {L"边面几何",L"读取几何",FEATURE_ANALYZE},
    {L"基准与建模参照",L"只读当前零件可用的基准、平面、轴和边参照。",FEATURE_ANALYZE},
    {L"孔、圆角和倒角的定位参照",L"只读当前零件的加工参照，列出平面、法向和边的位置。",FEATURE_ANALYZE},
    {L"草绘信息",L"读取当前零件草绘〔待填：草绘名称〕的图元、尺寸和约束。",FEATURE_ANALYZE},
    {L"新建模型",NULL,0},
    {L"空实体零件",L"创建实体空零件",FEATURE_SMART},
    {L"空钣金零件",L"新建钣金空零件",FEATURE_SMART},
    {L"空装配",L"新建空装配",FEATURE_SMART},
    {L"矩形板、长方体或正方体",L"新建独立实体零件，长〔待填：长〕、宽〔待填：宽〕、高〔待填：高〕毫米。",FEATURE_SMART},
    {L"圆柱",L"新建独立实体圆柱，直径〔待填：直径〕毫米，高〔待填：高度〕毫米。",FEATURE_SMART},
    {L"原点居中圆柱",L"新建原点居中圆柱 〔待填：直径 mm〕 〔待填：高度 mm〕",FEATURE_SMART},
    {L"基础形状带中心通孔",L"新建独立实体零件，形状与尺寸为〔待填：形状及尺寸〕，中心通孔直径〔待填：孔径〕毫米。",FEATURE_SMART},
    {L"基准",NULL,0},
    {L"偏移基准面",L"在当前零件创建偏移基准面〔待填：名称〕，参照〔待填：基准面〕，有符号偏移〔待填：距离〕毫米。",FEATURE_SMART},
    {L"两平面交轴",L"在当前零件创建基准轴〔待填：名称〕，取平面〔待填：平面一〕与〔待填：平面二〕的交线。",FEATURE_SMART},
    {L"圆柱面中心轴",L"在当前零件按圆柱面〔待填：参照〕创建中心轴〔待填：名称〕。",FEATURE_SMART},
    {L"基准点",L"在当前零件创建基准点〔待填：名称〕，相对坐标系〔待填：坐标系〕的 XYZ 为〔待填：坐标〕毫米。",FEATURE_SMART},
    {L"基准坐标系",L"在当前零件创建坐标系〔待填：名称〕，参照〔待填：原坐标系〕，平移〔待填：XYZ〕毫米，旋转〔待填：三个角度〕度。",FEATURE_SMART},
    {L"直线基准曲线",L"在当前零件创建直线基准曲线〔待填：名称〕，相对坐标系〔待填：坐标系〕，端点为〔待填：两组 XYZ〕毫米。",FEATURE_SMART},
    {L"关联草绘",NULL,0},
    {L"准备或新建关联草绘",L"在当前零件创建关联草绘〔待填：名称〕，草绘面〔待填：平面〕，方向参照〔待填：参照〕，首个图元〔待填：形状与尺寸〕。",FEATURE_SMART},
    {L"添加直线、矩形、圆、圆弧或中心线",L"在当前零件草绘〔待填：名称〕中添加〔待填：图元类型、位置和尺寸〕。",FEATURE_SMART},
    {L"添加长度、角度、直径或距离尺寸",L"在当前零件草绘〔待填：名称〕添加〔待填：尺寸类型、图元和数值〕。",FEATURE_SMART},
    {L"添加几何约束",L"在当前零件草绘〔待填：名称〕添加〔待填：约束类型和参照图元〕。",FEATURE_SMART},
    {L"修改草绘尺寸",L"修改当前零件草绘〔待填：名称〕的尺寸〔待填：编号和原值〕为〔待填：新值〕。",FEATURE_SMART},
    {L"平移直线、圆或圆弧",L"将当前零件草绘〔待填：名称〕的图元〔待填：编号〕平移 X〔待填：位移〕、Y〔待填：位移〕毫米。",FEATURE_SMART},
    {L"删除指定图元、尺寸或约束",L"删除当前零件草绘〔待填：名称〕中的〔待填：对象类型和编号〕。",FEATURE_SMART},
    {L"关联投影边或单段基准曲线",L"将当前零件的〔待填：边或单段基准曲线〕关联投影到已有草绘〔待填：名称〕。",FEATURE_SMART},
    {L"实体造型与加工",NULL,0},
    {L"空零件首个基础拉伸",L"在当前空实体零件创建基础拉伸，形状及尺寸为〔待填：矩形或圆形及尺寸〕，高度〔待填：高度〕毫米。",FEATURE_SMART},
    {L"按闭合草绘拉伸加料",L"在当前零件使用闭合草绘〔待填：名称〕，沿〔待填：方向〕拉伸加料〔待填：深度〕毫米。",FEATURE_SMART},
    {L"按闭合草绘定深切除",L"在当前零件使用闭合草绘〔待填：名称〕，沿〔待填：方向〕切除〔待填：深度〕毫米。",FEATURE_SMART},
    {L"按闭合草绘贯穿切除",L"在当前零件使用闭合草绘〔待填：名称〕，沿〔待填：方向〕单向贯穿切除。",FEATURE_SMART},
    {L"按闭合草绘旋转加料",L"在当前零件使用闭合草绘〔待填：名称〕，绕基准轴〔待填：轴〕按〔待填：方向〕旋转加料〔待填：角度〕度。",FEATURE_SMART},
    {L"按闭合草绘旋转切除",L"在当前零件使用闭合草绘〔待填：名称〕，绕基准轴〔待填：轴〕按〔待填：方向〕旋转切除〔待填：角度〕度。",FEATURE_SMART},
    {L"独立直通孔",L"在当前零件的平面〔待填：入口面〕创建直径〔待填：直径〕毫米的贯穿直孔，两个正交定位参照及有符号距离为〔待填：参照与距离〕。",FEATURE_SMART},
    {L"独立平底盲孔",L"在当前零件的平面〔待填：入口面〕创建直径〔待填：直径〕、深〔待填：深度〕毫米的平底盲孔，两个正交定位参照及有符号距离为〔待填：参照与距离〕。",FEATURE_SMART},
    {L"固定半径直边圆角",L"在当前零件对〔待填：1～8条直边〕创建半径〔待填：半径〕毫米的圆角。",FEATURE_SMART},
    {L"等距离直边倒角",L"在当前零件对〔待填：1～8条直边〕创建等距离〔待填：距离〕毫米的倒角。",FEATURE_SMART},
    {L"修改、文件与装配",NULL,0},
    {L"修改已有驱动尺寸",L"修改当前〔待填：零件或装配〕的驱动尺寸〔待填：符号及原值〕为〔待填：新值〕。",FEATURE_SMART},
    {L"修改已有模型参数",L"将当前〔待填：零件或装配〕参数〔待填：名称及原值〕改为〔待填：新值〕。",FEATURE_SMART},
    {L"抑制指定已有特征",L"执行抑制：当前零件的〔待填：孔、圆角、倒角或切除特征名称〕。",FEATURE_SMART},
    {L"恢复指定抑制特征",L"执行恢复：当前零件的〔待填：被抑制特征名称〕。",FEATURE_SMART},
    {L"重生成模型",L"执行重生成，检查当前〔待填：零件或装配〕。",FEATURE_SMART},
    {L"复制零件",L"复制当前零件，副本名〔待填：名称〕。",FEATURE_SMART},
    {L"重命名零件",L"将当前零件重命名为〔待填：名称〕。",FEATURE_SMART},
    {L"打开或显示工作目录中的零件",L"执行打开或显示当前工作目录中的零件〔待填：名称〕。",FEATURE_SMART},
    {L"保存零件或装配",L"保存当前〔待填：零件或装配〕。",FEATURE_SMART},
    {L"导出 STEP",L"导出当前零件为 STEP，文件名〔待填：名称〕。",FEATURE_SMART},
    {L"读取顶层装配",L"只读当前顶层装配的基本信息。",FEATURE_ANALYZE},
    {L"读取装配中已激活的零件",L"读取当前已激活组件零件的基本信息。",FEATURE_ANALYZE},
    {L"任务控制与开发",NULL,0},
    {L"查看执行明细",L"执行明细",FEATURE_KEEP_MODE},
    {L"查看待执行方案",L"查看方案",FEATURE_SMART},
    {L"执行待定建模方案",L"执行建模",FEATURE_SMART},
    {L"取消待定操作",L"取消操作",FEATURE_SMART},
    {L"停止当前任务",L"",FEATURE_STOP},
    {L"自检零件工具",L"自检零件工具",FEATURE_SMART},
    {L"开发或调整插件",L"〔待填：要新增或调整的功能及验收要求〕",FEATURE_DEVELOP},
    {L"应用已验升级",L"应用已验升级",FEATURE_KEEP_MODE}
};

static void native_feature_menu(HWND button){
    enum { FEATURE_LIST_COMMAND=0x7001 };
    HMENU menu=CreatePopupMenu(),group=NULL;
    RECT anchor;
    UINT selection;
    size_t i;
    if(!menu)return;
    if(!AppendMenuW(menu,MF_STRING,FEATURE_LIST_COMMAND,L"功能列表")||
        !AppendMenuW(menu,MF_SEPARATOR,0,NULL)){
        DestroyMenu(menu);
        return;
    }
    for(i=0;i<sizeof(native_features)/sizeof(native_features[0]);i++){
        const NativeFeature *feature=&native_features[i];
        if(!feature->draft){
            group=CreatePopupMenu();
            if(!group||!AppendMenuW(menu,MF_POPUP,(UINT_PTR)group,feature->label)){
                if(group)DestroyMenu(group);
                DestroyMenu(menu);
                return;
            }
        }else{
            int enabled=feature->mode==FEATURE_STOP?native_busy==1:!native_busy;
            AppendMenuW(group,MF_STRING|(enabled?0:MF_GRAYED),(UINT_PTR)(i+1),feature->label);
        }
    }
    GetWindowRect(button,&anchor);
    selection=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY,anchor.left,anchor.bottom,native_window,NULL);
    DestroyMenu(menu);
    if(selection==FEATURE_LIST_COMMAND){
        native_feature_list_action();
        return;
    }
    if(!selection||selection>sizeof(native_features)/sizeof(native_features[0]))return;
    {
        const NativeFeature *feature=&native_features[selection-1];
        if(!feature->draft)return;
        if(feature->mode==FEATURE_STOP){
            if(native_busy==1)native_cancel_action();
            return;
        }
        if(native_busy)return;
        if(feature->mode>=0){
            SendMessageW(native_mode_control,CB_SETCURSEL,feature->mode,0);
            InterlockedExchange(&native_mode,feature->mode);
        }
        SendMessageW(native_prompt,EM_SETSEL,0,-1);
        SendMessageW(native_prompt,EM_REPLACESEL,TRUE,(LPARAM)feature->draft);
        SetFocus(native_prompt);
    }
}

#endif
