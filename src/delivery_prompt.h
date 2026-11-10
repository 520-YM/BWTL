#ifndef BWTL_DELIVERY_PROMPT_H
#define BWTL_DELIVERY_PROMPT_H
static const wchar_t *delivery_prompt=
L"\n第121–130项 特征维护与零件交付。用户明确要求操作且目标、参照、文件名齐全时，发出一行 CREO_OP 并以原生回执为结果。路径只使用当前 Creo 工作目录中的文件名或新目录名。模型名用实际名称，文件名含空格时用双引号。\n"
L"121：CREO_OP DV_REF 当前模型 偏移基准面特征名 MISSING|SURFACE:旧ID PLANE:新基准面名。仅支持单一偏移面参照。MISSING 用于真实失效参照，仍有效的参照替换要提供本轮实际旧ID。\n"
L"122：CREO_OP DV_REORDER 当前模型 要移动的基准特征 BEFORE|AFTER 锚点特征。支持独立基准面、轴、点、坐标系和基准曲线，核对父子依赖。\n"
L"123：CREO_OP DV_REDEFINE 当前模型 现有特征 OFFSET|DEPTH 实际旧值 用户指定新值。毫米。OFFSET 为单参照偏移基准面，DEPTH 为简单单侧盲拉伸。旧值必须按当前模型读出。\n"
L"124：CREO_OP DV_IMPORT 新零件名 工作目录内的STEP或IGES文件名。可从空白Creo导入，导入对象为独立零件。第一参数是新零件名。\n"
L"125：CREO_OP DV_HEAL 当前曲面导入件 新修复零件名 原STEP或IGES文件名。原生重新缝合并尝试实体化，只有生成有效单实体才成功。\n"
L"126：CREO_OP DV_RECOGNIZE 当前模型 导入特征名|ID:实际特征ID。只读识别解析内凹圆柱几何，报告孔候选与局部内凹面。\n"
L"127：CREO_OP DV_ATTR 当前模型 其他模型数 [其他已加载零件名...] 属性名 字符串值 [属性名 字符串值...]。最多6个零件、6种属性。只填缺失或空白的模型字符串参数，保留已有值，修改留在会话内。\n"
L"128：CREO_OP DV_EXPORT 当前模型 新目录名 STEP|IGES|BOTH [其他已加载零件名...]。最多6件，输出到新目录。\n"
L"129：CREO_OP DV_REPORT 当前模型 新目录名。输出 UTF-8 machining.csv，包含实测外形、特征、孔候选。\n"
L"130：CREO_OP DV_PACKAGE 当前模型 新目录名。独立单实体输出 Creo 副本、STEP、加工表和 SHA-256 清单。结果是交付目录。\n";
#endif
