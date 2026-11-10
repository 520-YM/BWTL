#ifndef BWTL_ORGANIZE_PROMPT_H
#define BWTL_ORGANIZE_PROMPT_H
static const wchar_t *organize_prompt=
L"\n第211–220项 读模型与整理模型。均以 CREO_OP 功能 模型 开头，使用本轮真实参照。除 ORG_RENAME 以外均只读。\n"
L"ORG_HOLES 后续 MODEL 或 CSYS:坐标系 显示上限1–100。ORG_SPECS 后续 直径分组公差 轴向跨度分组公差，单位毫米。\n"
L"ORG_SLOTS 后续 导入特征名或ID:编号 SURFACE:入口平面 SURFACE:相对出口平面 最小长宽比，识别矩形及圆端贯穿槽。\n"
L"ORG_PLANES 后续 ALL|+X|-X|+Y|-Y|+Z|-Z 最小面积mm² 显示上限1–24。\n"
L"ORG_RENAME 后续 数量 ID:特征编号 当前旧名 新名 …，最多8项。仅当用户明确要求更名且新旧名已确认才执行。\n"
L"ORG_UNUSED 和 ORG_STATUS 只需模型名，分别找未引用基准候选和失败/抑制状态。\n"
L"ORG_OVERLAP 后续 公差毫米 显示上限1–64，列出重复定义及平行圆柱孔段重叠候选。\n"
L"ORG_PARAMS 后续 已载入会话的另一版本零件名，比较模型级参数。ORG_IMPACT 后续 特征名或ID:编号 EDIT|SUPPRESS|DELETE 深度1–5，只报告原生依赖预检。\n";
#endif
