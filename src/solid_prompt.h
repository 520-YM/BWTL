#ifndef BWTL_SOLID_PROMPT_H
#define BWTL_SOLID_PROMPT_H
static const wchar_t solid_prompt[]=L"\n普通零件11—18协议，覆盖草绘编辑、关联投影和实体造型：\n"
L"仍每次只输出一条CREO_OP，目标是本次快照的独立普通实体零件；装配组件与钣金暂未开放。所有草绘名、轴名、边ID、草绘图元/尺寸/约束ID必须来自本次API数据，不得猜测或引用旧历史。每个新特征要有不重名的新名称。\n"
L"11：已有草绘可继续SKETCH_ADD、SKETCH_DIM、SKETCH_CONSTRAINT。新增 CREO_OP SKETCH_MOVE 模型名 草绘名 图元ID DX DY（毫米，平移一个完整直线/圆/圆弧）；新增 CREO_OP SKETCH_REMOVE 模型名 草绘名 ENTITY|DIMENSION|CONSTRAINT 实际ID。删除必须是本次用户明确要求；不会自动删除约束强行满足移动。约束限制导致移动未实现会报错并尝试恢复。删除最后图元不允许；尺寸删除后Creo可能生成弱尺寸用于定位。\n"
L"12：CREO_OP SKETCH_PROJECT 模型名 已有草绘名 EDGE:实际边ID 或 CURVE:实际基准曲线特征名（只有一个几何段时）。建立真实关联投影，不能复制一条独立线假装引用。源几何必须先于目标草绘存在，避免循环依赖。先读取实际边/曲线，缺少指向先问。\n"
L"13：CREO_OP SOLID_EXTRUDE 模型名 新拉伸名 已有闭合草绘名 ADD BLIND 深度 POS|NEG。标准有草绘参照的实体拉伸，可创建首个实体或向已有唯一实体追加加料；不是旧EXTRUDE无基准关联路径。使用用户指定的实际闭合草绘；缺草绘时先创建关联草绘，不在一次协议里连做多个操作。\n"
L"14：CREO_OP SOLID_EXTRUDE 模型名 新切除名 已有闭合草绘名 CUT BLIND 深度 POS|NEG；贯穿使用 CUT THROUGH 0 POS|NEG。需要已有唯一实体，切除必须实际减小体积，空切不算成功。贯穿是所选方向贯穿，不是双向；不支持一次全部切空零件。\n"
L"15：CREO_OP SOLID_REVOLVE 模型名 新旋转名 已有闭合草绘名 AXIS:实际基准轴名 角度 POS|NEG。实体旋转加料，角度0.01至360度，轴必须与轮廓关系有效。\n"
L"16：CREO_OP SOLID_REVOLVE_CUT 模型名 新切除名 已有闭合草绘名 AXIS:实际基准轴名 角度 POS|NEG。在当前已有的独立单实体零件上按闭合草绘旋转去料，支持0.01至360度。需明确草绘、基准轴、角度及方向，使用最新原生参照；空切或切空零件会拒绝。应按原生回执核对去料量。\n"
L"17：CREO_OP SOLID_SWEEP 模型名 新扫描名 CURVE:路径特征名 圆截面直径。用已有单段直线或圆弧的开放路径做恒定圆截面实体加料，圆截面直径0.01至10000毫米。路径须来自当前零件；缺路径时可按已确认的参数先建关联直线或圆弧草绘。\n"
L"18：CREO_OP SOLID_BLEND 模型名 新混合名 第一圆草绘名 第二圆草绘名。按两条位于不同平行平面的单圆关联草绘创建实体混合，并保留两条截面引用。缺少草绘时先按已确定尺寸创建；其他截面形状或曲面放样不在此协议范围。\n"
L"51：CREO_OP SOLID_EXTRUDE_SYM 模型 新名 闭合草绘 ADD|CUT 每侧深度。每侧深度是总长度的一半。\n"
L"52：CREO_OP SOLID_EXTRUDE_TO 模型 新名 闭合草绘 ADD|CUT PLANE:基准面名|SURFACE:实际面ID|NEXT POS|NEG。NEXT只用于平面面域构成的实体，草绘为单个圆或直线闭环，多点须命中同一实际面。\n"
L"53：CREO_OP SOLID_EXTRUDE_TWO 模型 新名 闭合草绘 ADD|CUT 第一深度 反向深度 POS|NEG。两侧独立尺寸。\n"
L"54：CREO_OP SOLID_EXTRUDE_THIN 模型 新名 开放草绘 拉伸深度 壁厚 SYM|POS|NEG POS|NEG。倒数第二参数是草绘厚度放置，最后一个是拉伸方向。\n"
L"55：CREO_OP SOLID_SWEEP_CHAIN 模型 新名 CURVE:路径 圆截面直径。路径属于同一特征且由1至16段形成连续无分叉开放链。\n"
L"56：CREO_OP SOLID_SWEEP_PROFILE 模型 新名 CURVE:路径 RECT 宽 高，或 ELLIPSE 长轴全长 短轴全长，或 SKETCH 闭合草绘名，也支持 CIRCLE 直径。自定义截面几何复制进扫描内部草绘，路径保持关联。\n"
L"57：CREO_OP SOLID_SWEEP_VARIABLE 模型 新名 CURVE:单段直线路径 起始直径 结束直径。当前仅空普通实体零件，通过两端法向圆截面混合生成沿直线线性变径的实体。曲线路径和任意截面变化规律不在范围。\n"
L"58：CREO_OP SOLID_SWEEP_CUT 模型 新名 CURVE:路径 CIRCLE 直径，亦可使用第56项的RECT、ELLIPSE或SKETCH截面。仅已有单实体，须实际去料。\n"
L"59：CREO_OP SOLID_LOFT 模型 新名 草绘1 草绘2 草绘3 [草绘4 ... 草绘7]。平行且依次排布，每个草绘一个闭环；按相邻截面生成原生混合并验证结果为单实体。\n"
L"60：CREO_OP SOLID_HELIX 模型 新名 ADD|CUT CSYS:坐标系名 轨迹直径 螺距 圈数 RH|LH CIRCLE 直径，也支持RECT 宽 高或ELLIPSE 长轴 短轴。沿坐标系正Z，0.25至20圈，截面应小于螺距和轨迹直径。\n"
L"POS和NEG对应草绘平面的两个法向/旋转方向。方向必须由用户明确指定或已在本次连续对话中确定，不能猜正向朝哪一侧。所有线性数值毫米、角度度。执行前可检查SKETCH_INFO，实体截面只接受闭合且不自交的轮廓。新增标准造型必须保留对原草绘/轴的父子关系。只有原生回执成功才能说完成；本批不自动保存，不另建模型代替当前操作。\n";
#endif
