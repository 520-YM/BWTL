#ifndef BWTL_SHOP_PROMPT_H
#define BWTL_SHOP_PROMPT_H
static const wchar_t *shop_prompt=
L"\n第171–180项 常用加工。仅限当前普通单实体零件。毫米、度。F C P 代表 SURFACE:实际加工平面ID CSYS:定位坐标系 PLANE:方向面。CSYS 的原点在面上且 +Z 向外。B 代表现有内圆柱 SURFACE:实际ID。各操作以 CREO_OP 开头。只使用本轮实际参照；缺少参数或参照时先查找或澄清。\n"
L"SHOP_STEP 模型 名 F C P X Y 级数 D1 累计深1 …，3–5级，直径递减，累计深递增。\n"
L"SHOP_CROSS 模型 名 F1 C1 P1 X1 Y1 D1 深1 F2 C2 P2 X2 Y2 D2 深2，轴线垂直，在料内确实相交。\n"
L"SHOP_BACK 模型 名 F C P B 沉台直径 深度 最小剩余厚，F为选定的背面。\n"
L"SHOP_DEBURR 模型 名 C倒角尺寸 孔口数 B1 F1 …，1–6个完整圆孔口。F为SURFACE实际编号。\n"
L"SHOP_ISLAND 模型 名 F C P X Y 外框宽 外框高 深度 岛数 X1 Y1 宽1 高1 …，1–4个矩形岛。\n"
L"SHOP_LEVEL 模型 名 F C P X Y 层数 宽1 高1 累计深1 …，2–5层同心矩形。\n"
L"SHOP_DOG 模型 名 F C P X Y 现有矩形腔宽 高 实际深度 避让刀径，处理四个直角。\n"
L"SHOP_FLAT 模型 名 F C P B 缺口宽 径向深 角度，沿原圆孔的局部方向切贯穿的直壁止转缺口。\n"
L"SHOP_GROOVE 模型 名 SURFACE:外圆柱ID CSYS:轴心坐标系 AXIS:同轴基准轴 PLANE:过轴草绘面 PLANE:方向面 轴向起点 宽 径向深 槽根半径，沿坐标系Z计距。\n"
L"SHOP_TOOL 模型 名 F C P B 工具外径 单边余量 深度 最小剩余厚。让位直径等于工具外径加两倍余量。\n";
#endif
