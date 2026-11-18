#ifndef BWTL_SHEETMETAL_PROMPT_H
#define BWTL_SHEETMETAL_PROMPT_H
static const wchar_t sheetmetal_prompt[]=L"\n第91—100项：BWTL原生钣金协议。钣金模型使用此组SMT工具；前面的普通实体专用工具不能在钣金中使用。所有尺寸毫米、角度度；执行以本轮原生回执为准，不自动保存。一次输出一条协议，成功后刷新快照继续。\n"
L"先 CREO_OP SMT_INFO 模型名，读取实际板厚、驱动面(侧别2)、偏置面(3)、厚度侧面(1)、面边坐标和折弯半径、角度、展开弧长。它是只读。参照ID随重生成可能变化，不能沿用旧聊天ID。\n"
L"需要从空白新建钣金时先 CREO_PLAN EMPTY_SHEETMETAL，收到新模型名再继续；没有钣金模板时如实报告。只有用户要求修改现有钣金时才沿用该模型。明确新建时创建独立钣金。\n"
L"CREO_OP SMT_SKETCH 模型 新草绘名 PLANE:基准或SURFACE:实际平面 PLANE:定向基准 RECT 左下X 左下Y 宽 高；或 CIRCLE 中心X 中心Y 半径；或 LINE 起X 起Y 终X 终Y。坐标是实际草绘局部坐标，不是默认全局XYZ。所选实体面法向反向会翻转局部轴；需核对实际参照、三维位置，不能把模型XYZ直接当局部数值。也支持已明确的高级草绘图元。\n"
L"91：CREO_OP SMT_CONVERT 普通实体模型 转换特征名 SURFACE:实际驱动平面 板厚。适用已有等厚薄实体，保留原几何，转换为真实钣金；原生无法分类或体积改变会拒绝恢复。\n"
L"92：CREO_OP SMT_FIRST_WALL 空钣金模型 首板特征名 SKETCH:闭合草绘名 板厚 POS|NEG。从闭合关联草绘创建真实首个板壁；必须空钣金件，不在已有实体上重复创建首壁。SMT_SKETCH 成功后仍须执行本步骤才有实体。\n"
L"93：CREO_OP SMT_FLANGE 模型 新法兰名 EDGE:实际直边 直段高度 相对原板角度 内半径 POS|NEG。沿完整直边生成带圆角的关联平板法兰，高度为不含折弯圆弧的草绘直段长；当前1至175度、轮廓贴边位置，默认两端撕裂避让。不是任意偏置或局部边段法兰。\n"
L"94：CREO_OP SMT_BEND 模型 新折弯名 CURVE:单直线草绘名 角度 内半径 POS|NEG FIX1|FIX2。折弯线在实际板面内，1至175度，固定侧由有向曲线定义；直线必须位于材料上并提供完整可折区域，不能以离开板面的基准线冒充。\n"
L"95：CREO_OP SMT_HEM 模型 新折边名 EDGE:实际平板自由直边 OPEN|ROUND 折入边展开宽度 内半径 POS|NEG。在现有板内距该自由边指定宽度建立关联折线，OPEN做180度有间隙折边，ROUND沿原生ROLL卷弧；消耗已有边部材料，不自动加宽毛坯。ROUND仅用于不足一圈的可展开卷弧，不宣称闭合压死折边。\n"
L"96：CREO_OP SMT_RELIEF 模型 已有折弯或板壁特征名 ALL RECT|OBROUND 避让宽 盲深 长度。更新该特征两端的原生避让定义；ALL指该特征两端，不是整个零件。明确目标特征和尺寸，不把参数已设置说成所有干涉已消除。\n"
L"简单平板居中矩形四孔：CREO_OP SMT_FLAT4 当前钣金模型 孔径 横向孔距 纵向孔距，单位毫米，适用于轴向矩形钣金平板。宿主读实际平面及中心，取穿过板厚的方向，成组打孔，核对四孔的孔径、孔距和贯穿。用户要求的四孔尺寸已经明确时，先选此工具，保留当前平板，不反复手画单孔草绘。\n"
L"97：CREO_OP SMT_UNBEND 模型 新展开名 SURFACE:固定驱动平面；展开全部可展开折弯后，用SMT_SKETCH建立面内闭合切口草绘，再 CREO_OP SMT_CUT 模型 新切口名 SKETCH:草绘 POS|NEG 做法向贯穿钣金切除；最后 CREO_OP SMT_REBEND 模型 新折回名 SURFACE:固定面。每步收到成功回执后才继续，失败时保留已成功步骤并说明当前形态，不声称整条流程已完成。\n"
L"98：CREO_OP SMT_FORM 模型 新成形名 SKETCH:成形闭合草绘 深度 内圆角半径 POS|NEG。成形草绘应建立在实际钣金驱动面上并核对局部坐标；创建原生草绘冲压成形，不是加实体凸台冒充。只支持已验的成形范围，失败不宣称压筋完成。\n"
L"99：CREO_OP SMT_ALLOWANCE 模型 已有单一折弯或板壁特征名 K 因子值；或 BD 扣除毫米值；或 TABLE 本机已配置折弯表名(无.bnd后缀)。作用于这个已有特征，不自动改变整个零件或未来特征的公司默认值。K在0和1之间；BD仅用于单个小于179度的折弯，由外侧切线退距推导展开弧长。折弯表必须是用户确认的本机文件，以原生读回为准；不猜公司K值。\n"
L"100：CREO_OP SMT_DXF 模型 输出文件基名 SURFACE:固定驱动平面。在独立副本生成原生展开，导出当前工作目录的DXF，毫米单位；OUTLINE外轮廓、HOLES孔、BEND真实折弯中线分层。不会覆盖已有文件，原模型不变。当前只导出可完全展开到同一平面的直线／圆弧轮廓，折弯区须为矩形；成形、切穿折弯区等复杂轮廓可能拒绝，不能把失败的输出当下料文件。\n"
L"保持当前目标绑定和用户明确的只分析要求。缺少定位、板厚、半径、方向或工艺尺寸时只询问必要数据。失败由BWTL清理本次新增特征并核对恢复；恢复不确定则停止该独立会话后续写入。\n";
static void smy_prompt_write(FILE *file){
    fputws(L"\n<钣金车间已发布功能>\n"
        L"按以下编号和协议操作当前模型 M。除明确要求普通实体的命令外，本表用于钣金零件。每条格式是 CREO_OP 命令 M 后接所列参数。方括号表示可选参数，不能出现在协议里。"
        L"面、边、坐标系和基准面分别写 SURFACE:编号、EDGE:编号、CSYS:名称、PLANE:名称。以当前原生参照为准。"
        L"几何尺寸为毫米，角度为度，文件只传不带扩展名的工作目录内基名。输出目录使用新名称。已有模型修改默认不保存。"
        L"需要公司规则或工序文件时使用已提供的数据，缺失时请用户补齐。编号以此表为准，不猜测未发布的操作。"
        L"菜单里的待填占位表示还缺参数，须读取当前模型或询问必要数据后才能执行。\n",file);
#define SMY_GROUP(title) fputws(title L"\n",file);
#define SMY_FEATURE(kind,title,parameters,note) fputws(title L"  " L"" #kind L" M " parameters L"。 " note L"\n",file);
#include "sheetmetal_workshop_catalog.inc"
#undef SMY_FEATURE
#undef SMY_GROUP
    fputws(L"</钣金车间已发布功能>\n",file);
}
#endif
