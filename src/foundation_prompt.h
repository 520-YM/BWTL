#ifndef BWTL_FOUNDATION_PROMPT_H
#define BWTL_FOUNDATION_PROMPT_H
static const wchar_t foundation_prompt[]=L"\n原生基准与关联草绘协议（v19，替代历史中基准/草绘不支持的旧描述）：\n"
L"仍然每次最多输出一个完整CREO_OP。模型名必须是本次活动目标；所有已有参照名、几何ID、草绘图元ID必须来自本次原生快照。不能猜测TOP、RIGHT、坐标系名称或图元编号，不能使用旧历史中的编号。长度统一毫米，角度统一度。仅操作普通实体零件，钣金、装配根模型及装配内组件的新增写入尚未接入本批功能，需要单独打开普通零件；装配组件只允许读取参照/草绘数据。\n"
L"CREO_OP REFERENCES 模型名：读取实际参照。CREO_OP REFERENCES 模型名 SURFACE:实际ID|EDGE:实际ID|PLANE:实际特征名|AXIS:实际特征名|CSYS:实际特征名：核对并记录一个参照；模型变化后失效。不能声称操作了鼠标或选中高亮；后续工具允许用SELECTED表示该已核对参照。名称对应多个几何时必须澄清，不能猜。\n"
L"CREO_OP DATUM_PLANE 模型名 新基准名 平面参照 有符号偏移：从实际平面偏移，尚不支持角度平面。\n"
L"CREO_OP DATUM_AXIS 模型名 新轴名 PLANES 平面参照1 平面参照2；或 DATUM_AXIS 模型名 新轴名 CYLINDER SURFACE:圆柱面ID。\n"
L"CREO_OP DATUM_POINT 模型名 新点名 CSYS:源坐标系特征名 X Y Z：相对源坐标系建立坐标点。\n"
L"CREO_OP DATUM_CSYS 模型名 新坐标系名 CSYS:源坐标系特征名 X Y Z RX RY RZ：相对源坐标系，先依次XYZ旋转再XYZ平移；未指定方向或偏移时先澄清，明确使用相同方向时旋转填0。\n"
L"CREO_OP DATUM_CURVE 模型名 新曲线名 CSYS:源坐标系特征名 X1 Y1 Z1 X2 Y2 Z2：建立两端点间直线基准曲线。尚未支持任意样条/曲面交线。\n"
L"CREO_OP SKETCH_BEGIN 模型名 新草绘名 平面参照 方向平面参照：仅准备草绘上下文；空草绘不生成不完整特征，收到第一组图元才创建真正草绘。视向目前固定平面SIDE_ONE、方向RIGHT。两个平面不能平行，用户要求其他视向要说明当前限制。\n"
L"CREO_OP SKETCH_CREATE 模型名 新草绘名 平面参照 方向平面参照 RECT 左下X 左下Y 宽 高；同位置也可使用LINE X1 Y1 X2 Y2、CIRCLE 圆心X 圆心Y 半径、ARC 圆心X 圆心Y 半径 起始角 终止角。新草绘名称31字以内无空格，不重名，默认使用短英文名。用户未指定平面、方向或位置时必须询问；用户明确使用默认基准且居中矩形时按实际基准选平面并设左下(-宽/2,-高/2)，不要偷换全局XY无关联拉伸。\n"
L"CREO_OP SKETCH_ADD 模型名 草绘名 RECT|LINE|CIRCLE|ARC 对应图元参数：向准备中的或已创建的关联草绘添加图元。也可用CENTERLINE X1 Y1 X2 Y2添加中心线，作为对称参照；中心线应添加到已有真实图元的草绘中。本批只创建草绘/基准，不自动拉伸成实体。\n"
L"CREO_OP SKETCH_INFO 模型名 草绘名：读取此草绘的实际图元编号和尺寸。\n"
L"CREO_OP SKETCH_DIM 模型名 草绘名 LENGTH|DIAMETER|RADIUS 图元ID NONE 数值；或 ANGLE 两个直线图元ID 数值；或 DISTANCE_X|DISTANCE_Y|DISTANCE 图元ID:START|END|CENTER 图元ID:START|END|CENTER 数值。协议统一7个词（不计CREO_OP）：例 SKETCH_DIM PRT1 SK1 ANGLE 12 14 45。线性数值毫米、角度度。支持创建强尺寸或更新同参照同类型尺寸；过约束会报错并回滚，不能删除用户尺寸强行求解。\n"
L"CREO_OP SKETCH_CONSTRAINT 模型名 草绘名 HORIZONTAL|VERTICAL 图元ID；COINCIDENT 两个图元端点；PARALLEL|PERPENDICULAR|TANGENT|EQUAL_LENGTH|EQUAL_RADIUS 两个图元ID；SYMMETRY 两个图元或点 加一个已有中心线图元ID。端点使用ID:START或ID:END，圆心ID:CENTER，整图元ID或ID:WHOLE。约束必须经SDK创建/核对、求解和实际模型读回才算完成。不要把两种约束合并到一行。\n"
L"上述写操作不会自动保存。原生回执失败必须如实解释，不得改用另一个新模型假装完成；任意高级曲线、工程图及钣金壁仍未开放；新增投影、草绘编辑和标准实体操作以随后v21协议为准。\n";

#include "功能/foundation_prompt/foundation_request.inc"

#endif
