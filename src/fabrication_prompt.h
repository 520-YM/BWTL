#ifndef BWTL_FABRICATION_PROMPT_H
#define BWTL_FABRICATION_PROMPT_H
static const wchar_t *fabrication_prompt=
L"\n第221–230项 加工与交付，CREO_OP 后使用以下参数格式。M=当前零件的实际名称，D=当前 Creo 工作目录下尚不存在的英文、数字、下划线、短横线目录名。模型、几何参照和尺寸用本次原生数据，文件名和放行公差由用户确认。\n"
L"FAB_BLANK M D SURFACE:固定面：钣金下料包络与厚度CSV。FAB_BENDS M D：当前成形钣金折弯表，按模型顺序。\n"
L"FAB_HOLES M D MODEL 或 CSYS:名称：孔加工清单。FAB_PAINT M D ALL 或 FACES SURFACE:编号 [...]：全部实体边界含内壁或最多16个实体面。\n"
L"FAB_DXF M D SURFACE:固定面 [已加载钣金模型 SURFACE:固定面 ...]：一次最多6件，逐件回读后发布。FAB_DXF_CHECK M SURFACE:固定面：仅检查。固定面必须是钣金驱动侧的实际平面。\n"
L"FAB_TEMPLATE M D SURFACE:孔口面 CSYS:名称：二维模板。坐标系原点位于孔口平面，+Z朝外；输出完整圆孔口。\n"
L"FAB_INSPECT M D 项数 符号 名义值 下偏差绝对值 上偏差绝对值 [...]：最多7项，线性用mm、角度用度；X/Y/Z为默认轴向包络。超差会保留CSV。\n"
L"FAB_SORT M D [已加载零件 ...]：料号取PART_CODE（缺少时取DRAWING_NO），版本取REVISION，按组发布STEP与加工表。FAB_READBACK M STEP 文件.stp 容差 或 FAB_READBACK M DXF 文件.dxf SURFACE:固定面 容差：读取当前工作目录内的文件，容差0.0001至5 mm。STEP结果是几何核对，DXF核对受支持的图元。不要将当前模型的尺寸检验结果当成实测。\n";
#endif
