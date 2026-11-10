#ifndef BWTL_INSPECTION_PROMPT_H
#define BWTL_INSPECTION_PROMPT_H
static const wchar_t *inspection_prompt=
L"\n第201–210项是独立单实体零件的只读检查。只用实际参照、用户提供的尺寸及阈值。以下都以 CREO_OP 功能 模型 开头，单位毫米、度或百分比。读回前不要声称已经符合或相交。\n"
L"CHK_SEAT 后续 SURFACE:入口平面 SURFACE:内圆柱 头外径 杆径 最小环形支承宽。CHK_HIDE 后续 原生沉孔名 BORE|SINK 头径 头高 锥头夹角 最小藏入深，BORE角度填0，底孔贯穿。\n"
L"CHK_BOTTOM 后续 原生平底盲孔名 最小底厚。CHK_RATIO 后续 原生平底盲孔名 最大深径比。\n"
L"CHK_BEND 后续 SURFACE:实际板面 SURFACE:孔内圆柱 EDGE:同板面实际折弯切线边 最小净距。\n"
L"CHK_TOOLSPACE 后续 EDGE:实际折弯切线边 CSYS:定位坐标系 中心X Y Z 长 宽 高 单侧余量，坐标系X平行折弯边，避让区排除工艺所需接触。CHK_DEVICE 后续 CSYS:定位坐标系 中心X Y Z 长 宽 高 单侧余量。两者仅检查一个静态长方体，若返回待复核，不能声称有净空或发生碰撞。\n"
L"CHK_VENT 后续 SURFACE:入口平面 SURFACE:相对出口平面 最低开孔百分比。直线与圆弧边界，已验证直通轮廓计入面积。CHK_SHARP 后续 外凸夹角阈值 EDGE:实际边1 …，一至八条直边。\n"
L"CHK_CUTTER 后续 CYL SURFACE:内圆柱 最小可用刀径 单侧余量，或 SLOT SURFACE:槽壁1 SURFACE:相向槽壁2 最小可用刀径 单侧余量。只筛选尺寸。\n";
#endif
