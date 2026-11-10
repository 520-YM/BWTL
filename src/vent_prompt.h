#ifndef BWTL_VENT_PROMPT_H
#define BWTL_VENT_PROMPT_H
static const wchar_t *vent_prompt=
L"\n第181–190项 散热、防水与走线。单位毫米和度。除 VENT_ROUTE 外，协议以 CREO_OP 功能 模型 特征名 F C P X Y 实际板厚 开头。F C P 是本轮 SURFACE:实际平面ID CSYS:定位坐标系 PLANE:方向面，CSYS 的 XY 在面上且 +Z 向外。X Y 通常为中心。确认真实参照、尺寸和配件规格后使用下列参数。\n"
L"VENT_ROUND 后续 行数 列数 X孔距 Y孔距 孔径 GRID|STAGGER；VENT_SLOT 后续 行数 列数 X槽距 Y槽距 沿X圆端槽总长 槽宽；VENT_GRID 后续 行数 列数 X孔距 Y孔距 矩形孔宽 矩形孔高。行、列均1–8，孔间最小留料0.1。\n"
L"VENT_LOUVER 后续 片数 X节距 开口X宽 开口Y长 斜叶角度 叶片垂直厚度 根部连接宽。1–4片，角度10–65度，普通实体斜叶沿Y延伸并向+X抬高。\n"
L"VENT_DRAIN 后续 EDGE:实际底边ID 槽长 槽宽。把共同前缀中的 Y 改为槽距底边的正净距；真实底边与局部X平行，局部+Y朝板内。\n"
L"VENT_GLAND 后续 ROUND|D 直径 平边距圆心 允许最小厚度 允许最大厚度。ROUND 时平边距填0，D 的平边朝+X。VENT_GROMMET 后续 ROUND|RECT 宽或直径 高 圆角 允许最小厚度 允许最大厚度，圆形时高和圆角填0。\n"
L"VENT_STOP 后续 X宽 Y长 凸起高度。VENT_SEAL 后续 外框宽 外框高 槽宽 槽深 最小底部留料。两者限普通实体。\n"
L"只读 CREO_OP VENT_ROUTE 模型 CSYS:定位坐标系 圆心X 圆心Y 圆心Z 中心线半径 线束外径 单侧余量。圆弧在局部XY平面，从+X方向转到+Y。检测当前零件，按原生回执区分确认净空、中心线相交或包围盒重叠待复核。\n";
#endif
