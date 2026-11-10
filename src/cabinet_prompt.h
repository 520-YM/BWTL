#ifndef BWTL_CABINET_PROMPT_H
#define BWTL_CABINET_PROMPT_H
static const wchar_t *cabinet_prompt=
L"\n第151–160项 钣金与机箱。只对已单独打开的原生钣金零件工作。所有长度为毫米，角度为度。POS、NEG 表示明确的翻向。用当轮真实 EDGE: 或 SURFACE: 编号。板边距离从 Creo 原生起点沿边量取。参照或方向不明时先读取或询问。默认未保存。\n"
L"151 CREO_OP CASE_SEG 零件 前缀 EDGE:编号 高 角 内半径 POS|NEG 段数 起1 止1 起2 止2 ...。2至5段，间隔严格正值。\n"
L"152 CREO_OP CASE_TRAY 零件 前缀 PLANE:底面 PLANE:方向面 底长 底宽 板厚 高 内半径 端部退让 POS|NEG。活动空钣金零件中创建四边托盘，每边两端同样退让，退让至少为内半径加板厚，底板短边必须大于两端退让加内半径。自由尺寸示例：200 150 2 30 2 8 POS。\n"
L"153 CREO_OP CASE_U 零件 前缀 PLANE:底面 PLANE:方向面 纵深 内净宽 板厚 侧壁高 内半径 POS|NEG。活动空钣金零件。\n"
L"154 CREO_OP CASE_Z 零件 前缀 EDGE:编号 两驱动面高差 第二段长度 内半径 POS|NEG。两道90度。\n"
L"155 CREO_OP CASE_GAP 零件 直边翻边名 EDGE:该翻边顶端直边 SURFACE:相邻参考平面 目标缝宽 平直段留根高度。法向切口增大平直段缝宽，保留折弯根部。\n"
L"156 CREO_OP CASE_END 零件 直边翻边名 EDGE:该翻边顶端直边 START|END 缩短量 平直段留根高度。按所选顶端直边原生起端或末端做局部法向切口。\n"
L"157 CREO_OP CASE_RETURN 零件 名称 EDGE:已翻起直边 SURFACE:门板原平面 回折宽 内半径 POS|NEG。\n"
L"158 CREO_OP CASE_EAR 零件 名称 EDGE:编号 起 止 高 内半径 POS|NEG 中心孔直径。无孔填0。\n"
L"159 CREO_OP CASE_COLLAR 零件 名称 EDGE:整圆孔边 高 内半径 POS|NEG。圆孔必须先存在并贯穿。\n"
L"160 CREO_OP CASE_BENDS 零件 数量 折弯名 旧角 旧内半径 新角 新内半径 [其他同组参数]。1至4道。普通角度折弯或附着直边翻边。\n";
#endif
