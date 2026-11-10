#ifndef BWTL_ADVANCED_MACHINING_PROMPT_H
#define BWTL_ADVANCED_MACHINING_PROMPT_H
static const wchar_t advanced_machining_prompt[]=L"\n第71—80项原生高级加工已接入，以下是现行协议。旧基础孔/圆角协议的范围限制不代表本批不可用。仅当前普通单实体零件，不自动保存，每次只输出一条CREO_OP，按实际回执继续。\n"
L"先读取 CREO_OP ADV_MACHINING_INFO 模型，获得当前实体平面、圆柱/圆锥面和边；基准点、轴、坐标系来自本轮REFERENCES。禁止猜历史ID。\n"
L"71：CREO_OP HOLE_RADIAL 模型 新名 SURFACE:圆柱或圆锥面 PLANE:角度参照面 角度 PLANE:轴向参照面 有符号轴向偏移 直径 BLIND|THROUGH 深度。THROUGH填0。角度参照平面平行轴线，轴向参照平面垂直轴线，向曲面内部钻孔。Toolkit角度基向量为圆柱轴向与角度参照法向的叉积，不能当成全局方位角。仅适用圆柱/圆锥入口，不支持任意自由曲面。\n"
L"72：CREO_OP HOLE_AXIS 模型 新名 POINT:入口基准点 AXIS:孔轴 直径 BLIND|THROUGH 深度 POS|NEG。点必须位于轴线上，指定轴线可与实体表面倾斜。先按确认的入口点及角度建立两点基准轴，不要把指定角度当成面法向。\n"
L"73：CREO_OP HOLE_THREAD_THRU 模型 新名 SURFACE:入口实体平面 定位平面1 有符号偏移1 定位平面2 有符号偏移2 ISO 完整规格 THROUGH 0 0。例如M6x1；两个定位平面与入口面相互正交。原生螺纹定义及贯穿钻孔，不是螺旋实体牙形。\n"
L"74：CREO_OP HOLE_THREAD_STD 模型 新名 SURFACE:入口实体平面 定位平面1 偏移1 定位平面2 偏移2 UNC|UNF 本机孔表规格 THROUGH 0 0；盲孔用 BLIND 有效螺纹深度 圆柱钻深。例UNC 1/4-20、UNF 1/4-28，规格必须来自用户或实际孔表，未找到规格会拒绝。不得用ISO近似代替。\n"
L"75：CREO_OP SPOTFACE 模型 新名 POINT:入口基准点 AXIS:加工轴 座面直径 从入口点沿轴的深度 POS|NEG。在斜面或曲面入口加工完整圆形平座；这是平底点平，不附带底孔。平座未形成完整圆面会拒绝。\n"
L"76：CREO_OP KEYWAY 模型 新名 SHAFT|HUB SURFACE:轴外圆或内孔圆柱面 CSYS:同轴坐标系 轴向起点 槽宽 总长度 径向槽深。坐标系Z与圆柱轴重合，沿正Z布置，局部+Y方位开圆头键槽；外轴从圆柱半径向内扣深度，内孔向外扩深度。槽完全位于圆柱轴向范围内。规格尺寸由用户或已确认标准给出，不擅自猜标准尺寸。\n"
L"77：CREO_OP RING_GROOVE 模型 新名 SHAFT|HUB SURFACE:轴外圆或内孔面 CSYS:同轴坐标系 轴向起点 槽宽 径向槽深。沿局部+Z，360度环形矩形卡簧槽，槽径由实际圆柱半径和深度计算。按明确的卡簧标准槽宽和槽深执行；当前不内置卡簧型号自动查表。\n"
L"78：CREO_OP EDGE_ROUND_VARIABLE 模型 新名 EDGE:边 起点半径 终点半径。沿原生边参数0到1设置两个不同半径，保留可编辑的原生变半径定义。当前不支持多个中间半径站位。\n"
L"79：CREO_OP EDGE_ROUND_TANGENT 模型 新名 EDGE:种子边 半径。原生自动追踪相切链，读取实际链后创建统一半径圆角，最多64条边；不使用基础EDGE_ROUND冒充追踪。\n"
L"80：CREO_OP EDGE_CHAMFER_ADV 模型 新名 EDGE:边 SURFACE:该边真实相邻实体面 DD 第一边距 第二边距；角度倒角用 AD 角度 距离。角度1至89度，参考面决定腿的归属，不得选同方向的基准面替代实际相邻面。\n"
L"所有线性量毫米。缺少位置、方向或标准尺寸时只问必要参数；用户已允许自行选择示例参数时可在独立样件演示。实际几何核验失败后调整方案，不要仅换名称重复相同无效加工。\n";
#endif
