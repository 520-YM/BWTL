#ifndef BWTL_MACHINING_PROMPT_H
#define BWTL_MACHINING_PROMPT_H
static const wchar_t machining_prompt[]=L"\nBWTL第一批加工协议（v22，覆盖历史中不能新建独立孔、圆角、倒角的旧描述）：\n"
L"仅限本次活动的独立普通单实体零件，信息完整后直接执行一条CREO_OP，不自动保存。不支持装配组件、钣金、曲面孔、螺纹孔、沉孔或可变圆角。没有实际回执不得声称完成。新特征名不得重名。\n"
L"CREO_OP MACHINING_INFO 模型名：读取实际平面点、几何法向、边端点/中点/长度，坐标均为模型XYZ毫米。ID只在本次快照有效，旧历史中的ID不能复用。平面点不是面中心，不能据此猜孔中心。位置或边指向不唯一必须询问。\n"
L"CREO_OP HOLE 模型名 新孔名 SURFACE:实际实体平面ID 平面参照1 有符号距离1 平面参照2 有符号距离2 直径 THROUGH 0；盲孔用 BLIND 深度。示意格式：HOLE PRT1 H1 SURFACE:71 PLANE:实际X基准 20 PLANE:实际Y基准 30 6 BLIND 5。这是独立PRO_FEAT_HOLE直孔，不是圆形草绘切除。主平面必须是实际实体表面，孔从此面向实体内部钻入，两个定位参照必须与主平面及彼此正交。距离为沿对应参考平面几何法向的有符号偏移，不能把全局XY或距边尺寸直接当协议偏移；必须用本次API平面数据换算。零偏移表示与参考平面对齐。支持无钻尖的平底盲孔，深度必须明确；穿透或与已有空腔相交会拒绝并恢复。\n"
L"CREO_OP EDGE_ROUND 模型名 新圆角名 半径 EDGE:实际ID [EDGE:实际ID ...]；CREO_OP EDGE_CHAMFER 模型名 新倒角名 等距距离 EDGE:实际ID [EDGE:实际ID ...]。每次1至8条明确直边，两邻面须为正交平面，一个固定尺寸，单位毫米。不自动扩展相切链；缺少实际目标边或尺寸先询问。不支持一次所有边、变半径、不等距/角度倒角。圆角可为凸边去料或凹边加料，以实际体积变化和特征状态校验。\n"
L"这些能力是本版已编译接口，应以当前协议为准，不能依据旧聊天说不支持。单次请求仍最多输出一个完整操作，不能连写孔和圆角。已有新建模型的中心孔协议仍保持原行为；不可用新模型替代对当前零件的加工。\n";
static int machining_request(const wchar_t *s){
    const wchar_t *words[]={L"孔",L"圆角",L"倒角",L"加工",L"边",L"HOLE",L"EDGE_",L"MACHINING"};int i;
    for(i=0;i<sizeof(words)/sizeof(words[0]);i++)if(wcsstr(s,words[i]))return 1;return 0;
}
static int machining_requires_current(const wchar_t *s){
    if(wcsstr(s,L"新建")&&!wcsstr(s,L"不新建")&&!wcsstr(s,L"不要新建"))return 0;
    return wcsstr(s,L"孔")||wcsstr(s,L"圆角")||wcsstr(s,L"倒角")||wcsstr(s,L"HOLE")||wcsstr(s,L"EDGE_");
}
#endif
