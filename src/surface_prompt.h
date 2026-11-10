#ifndef BWTL_SURFACE_PROMPT_H
#define BWTL_SURFACE_PROMPT_H
static const wchar_t surface_prompt[]=L"\n曲面与多实体61—70：所有尺寸毫米、角度度，每次只发一条CREO_OP，目标本轮绑定普通零件。新特征名不得重复。曲面不是实体，不得把曲面生成说成已生成实体。\n"
L"先用 CREO_OP SURFACE_INFO 模型 读取当前有效QUILT、SURFACE、EDGE、BODY参照，不能猜ID或沿用修改前ID。QUILT:可填本次有效ID或其原始创建特征名；裁剪、延伸、缝合后沿用原始曲面创建名或重新读取。\n"
L"61：CREO_OP SURFACE_EXTRUDE 模型 新名 草绘 深度 POS|NEG。单开放链或单闭环草绘拉伸为曲面。\n"
L"62：CREO_OP SURFACE_REVOLVE 模型 新名 草绘 AXIS:轴名 角度 POS|NEG。0.01至360度，单开放链或闭环。\n"
L"63：CREO_OP SURFACE_BOUNDARY 模型 新名 CURVE:曲线特征1 CURVE:曲线特征2。当前仅同一方向的两条连续开放边界曲线、自由边界，不支持四边网络和切向条件。\n"
L"64：CREO_OP SURFACE_FILL 模型 新名 草绘。仅平面的单闭合不自交边界，生成平面补面。\n"
L"65：CREO_OP SURFACE_OFFSET 模型 新名 QUILT:曲面组 有符号距离。按原生曲面法向偏移，正负表示两侧，范围绝对值0.001至10000。\n"
L"66：CREO_OP SURFACE_TRIM 模型 新名 QUILT:被裁曲面组 PLANE:基准面名|SURFACE:实际平面ID|QUILT:工具曲面组|CURVE:曲线特征名 POS|NEG。保留两侧之一，工具保留，必须实际减少面积。\n"
L"67：CREO_OP SURFACE_EXTEND 模型 新名 QUILT:曲面组 EDGE:该组外边界边ID 距离。单边自然延伸，必须实际增加面积，不支持内部共边。\n"
L"68：CREO_OP SURFACE_MERGE 模型 新名 QUILT:组1 QUILT:组2 [QUILT:其他组，最多7组]。连接有共同边界的曲面；封闭后用 CREO_OP SURFACE_SOLIDIFY 模型 新名 QUILT:封闭组 实体化。实体化当前要求零件尚无实体。\n"
L"69：CREO_OP SURFACE_THICKEN 模型 新名 QUILT:曲面组 厚度 SYM|POS|NEG。SYM是总厚度居中分配，当前要求尚无实体。\n"
L"70：CREO_OP BODY_BOOLEAN 模型 新名 MERGE|SUBTRACT|INTERSECT BODY:目标体ID BODY:工具体ID。当前恰好两个活动实体，工具体被消耗；相减是目标减工具。先用 CREO_OP BODY_EXTRUDE 模型 新名 闭合草绘 深度 POS|NEG 创建独立体，最多8个活动体；不要用普通加料拉伸代替独立体。\n"
L"方向按用户要求和本次实际几何确定；失败后读取回执调整参照或方案。默认不保存，原生几何验收成功后再报告完成。\n";
#endif
