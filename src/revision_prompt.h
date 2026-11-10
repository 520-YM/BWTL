#ifndef BWTL_REVISION_PROMPT_H
#define BWTL_REVISION_PROMPT_H
static const wchar_t *revision_prompt=
L"\n第191–200项 改尺寸与改规格。以下用 CREO_OP 起头，长度值毫米，角度度。仅对当前独立单实体和明确选定的特征执行。用当前模型读取的真实特征名和驱动尺寸符号；若快照尺寸用模型原生单位，先换算毫米。组操作支持1–8个特征。\n"
L"REV_SIZE 模型 旧外形X Y Z 目标外形X Y Z 驱动数 符号1 旧值 新值 …，按默认模型XYZ实际实体包围盒验收。\n"
L"REV_SHIFT 模型 第一定位方向偏移 第二定位方向偏移 孔数 孔名…。REV_PITCH 模型 第一孔距 第二孔距 四个矩形角位孔名。两者适用具有共同入口和两正交参照的原生直孔。\n"
L"REV_OPEN 模型 切除特征 草绘名 RECT 宽驱动符号 旧宽 新宽 高驱动符号 旧高 新高，或 REV_OPEN 模型 切除特征 草绘名 CIRCLE 驱动符号 旧直径 新直径。限关联单轮廓盲拉伸切除。\n"
L"REV_DIAM 模型 旧直径 新直径 数量 原生直孔名…；REV_DEPTH 模型 旧深 新深 数量 原生平底直盲孔名…。\n"
L"REV_RECESS 模型 BORE|SINK 旧外径 新外径 旧沉深或角度 新沉深或角度 数量 沉孔名…，底孔贯穿并保持原直径。REV_ROUND 模型 新半径 数量 圆角名1 旧半径1 …，限单一等半径圆角。\n"
L"REV_THICK 模型 旧板厚 新板厚，限钣金。REV_COPY 模型 新副本名；可在末尾附 REV_SIZE 的全部外形及驱动参数，仅在副本改外形。副本留在会话，原件不切换。\n";
#endif
