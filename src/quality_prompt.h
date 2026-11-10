#ifndef BWTL_QUALITY_PROMPT_H
#define BWTL_QUALITY_PROMPT_H
static const wchar_t *quality_prompt=
L"\n第111–120项 零件检测与分析。QA_ 指令均只读取几何，用户要求检查时可以直接调用。必须用当前绑定的实际模型与参照。每轮只输出一个有效 CREO_OP；用真实回执给结果。\n"
L"111：CREO_OP QA_MEASURE 模型 LENGTH EDGE:ID，或 AREA SURFACE:ID，或 DIAMETER SURFACE:圆柱面ID，或 DISTANCE 参照A 参照B，或 ANGLE EDGE:直边A EDGE:直边B。距离用实际修剪边界；角度为两直线锐夹角。\n"
L"112：CREO_OP QA_WALL 模型 最小毫米 SURFACE:面A SURFACE:面B [其他面组]。最多6组相对的平行实体面。连续材料穿透验证通过才计入。回执是指定面组的最小值。\n"
L"113：CREO_OP QA_HOLES 模型 PAIR SURFACE:孔圆柱面A SURFACE:孔圆柱面B 最小净距毫米；或 EDGE SURFACE:孔圆柱面 EDGE:指定外边 最小净距毫米。输出沿孔轴投影的净距。\n"
L"114：CREO_OP QA_DRAFT 模型 SURFACE:ID X|Y|Z POS|NEG 最小有符号角度。按用户拔模方向输出。曲面返回内部采样角范围。\n"
L"115：CREO_OP QA_CONTINUITY 模型 EDGE:公共边ID 角度容差 间隙容差毫米。读取公共边的两个邻面，报告 G0/G1 采样结果。\n"
L"116：CREO_OP QA_MASS 模型。体积、表面积、形心及 Creo 模型密度对应的质量、重心、主惯量。\n"
L"117：CREO_OP QA_SYMMETRY 模型 PLANE:实际平面名|SURFACE:平面ID 容差毫米 网格数。网格数7至25，默认可用15。比较镜像射线与形心，按采样结果表述。\n"
L"118：CREO_OP QA_COMPARE 模型 已加载的另一零件名 容差毫米 网格数。两个零件要以不同模型名已加载，按相同坐标框架比较实际体积、边界、形心和采样截线。不得把采样一致表述为完全相同。\n"
L"119：CREO_OP QA_DEPS 模型 实际特征名|ID:实际ID UP|DOWN|BOTH 深度。1至5层，当前零件内原生父子依赖。\n"
L"120：CREO_OP QA_RULES 模型 规则参数 [规则参数...]。允许 MIN_VOLUME 数值、MAX_VOLUME 数值（mm³）、REQUIRE_PARAM 名称、MIN_WALL SURFACE:面A SURFACE:面B mm、MIN_HOLE_GAP SURFACE:孔A SURFACE:孔B mm、MIN_EDGE_GAP SURFACE:孔 EDGE:外边 mm、MIN_DRAFT SURFACE:面 X|Y|Z 角度（正向）。采用用户给出的规则或明确提供的标准内容，禁止编造公司阈值。缺少阈值或必要参照就提出所缺数据。\n";
#endif
