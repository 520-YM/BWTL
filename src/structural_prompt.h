#ifndef BWTL_STRUCTURAL_PROMPT_H
#define BWTL_STRUCTURAL_PROMPT_H
static const wchar_t structural_prompt[]=L"\n第81—90项结构与成型协议，当前普通单实体零件；所有尺寸毫米，角度度。先用本轮ADV_MACHINING_INFO和REFERENCES识别实际面、点、轴和坐标系。每次一条协议，参数完整后直接执行，以原生回执为准，不自动保存。\n"
L"81：CREO_OP SHELL_MULTI 模型 新名 全局壁厚 SURFACE:开口面1 SURFACE:开口面2 [其他开口面，最多8个]。一次多开口、统一向内抽壳。\n"
L"82：CREO_OP SHELL_LOCAL 模型 新名 全局壁厚 SURFACE:开口面 SURFACE:特殊壁面 局部厚度 [SURFACE:其他壁面 厚度，最多5组]。当前一个开口面，局部厚度面不得是开口面或重复面。\n"
L"83：CREO_OP DRAFT_MULTI 模型 新名 PLANE:中性面 PLANE:拔模方向面 有符号角度 POS|NEG SURFACE:待拔面1 [其他面，最多8个]。实际待拔面为平面，方向面与中性面平行，待拔面与该方向正交；不自动扩展相切面。\n"
L"84：CREO_OP DRAFT_SPLIT 模型 新名 PLANE:中间中性面 PLANE:方向面 第一侧有符号角度 第二侧有符号角度 POS|NEG SURFACE:待拔面 [其他面，最多8个]。按中性面分割两侧，不是分模曲线分割；第一、第二侧按原生拔模方向定义。\n"
L"85：CREO_OP DRAFT_VARIABLE 模型 新名 SURFACE:平面 PLANE:中性面 PLANE:方向面 POINT:站位1 有符号角度1 POINT:站位2 有符号角度2 POS|NEG。两站位在待拔面与中性面交线上且不重合，生成原生可变角拔模，不用两个定角特征冒充。当前两站位，每个角绝对值0.1至30度。\n"
L"86：CREO_OP DRAFT_PATH_RIB 模型 新名 CURVE:直线路径 PLANE:基面或SURFACE:实际基面 筋底宽 高度 拔模角。当前路径只支持一条位于基面上的直线；圆弧和多段路径尚未支持。沿所选基面法向长高，顶部宽=底宽-2*高度*tan角，必须为正；用关联路径起点、路径法向平面和梯形截面拉伸建立加强筋。\n"
L"87：CREO_OP SCREW_BOSS 模型 新名 SURFACE:实际支承平面 CSYS:定位坐标系 外径 高度 底孔直径 孔深。坐标系原点位于支承面上、+Z沿外法向；底孔从柱顶沿-Z钻，孔深不得超过柱高，无孔两项填0，贯穿柱填孔深=柱高。可继续用现有螺纹协议加工，但不能将尺寸底孔说成已生成螺纹。\n"
L"88：CREO_OP SNAP_HOOK 模型 新名 SURFACE:支承平面 CSYS:定位坐标系 悬臂长度 宽度 板厚 钩高 斜坡长度。原点在支承面，+X为面外悬臂方向，+Y为宽度，+Z为厚度和钩位。匹配窗口另用 CREO_OP SNAP_WINDOW 模型 新名 SURFACE:配合板面 CSYS:窗口坐标系 卡扣宽 板厚 钩高 单侧间隙 切穿板厚；坐标系+X为配合板面外法向，窗口沿-X切，Y两侧分别加间隙，Z从-间隙到板厚+钩高+间隙。这是普通零件内几何，不自动建立装配配合。\n"
L"89：CREO_OP SEAL_GROOVE_FACE 模型 新名 SURFACE:端面 CSYS:同轴定位坐标系 槽中心线直径 密封圈截面直径 压缩百分数 槽宽。坐标系原点在端面，+Z向外；矩形环槽深=截面直径*(1-压缩率/100)。径向用 CREO_OP SEAL_GROOVE_RADIAL 模型 新名 SHAFT|HUB SURFACE:圆柱面 CSYS:同轴坐标系 轴向起点 截面直径 压缩百分数 轴向槽宽。压缩0至40，槽宽至少等于截面直径。标准尺寸和压缩率由用户或确认的技术标准给出，不擅自推荐密封方案。\n"
L"90：CREO_OP STOCK_ALLOWANCE 原模型 新副本模型名 正余量 SURFACE:加工面 [其他面，最多8个]。复制出独立毛坯，向指定面增加余量，核对原模型未变化；副本不自动保存、不切换窗口，也不自动跟随原件后续变更。副本留在Creo会话中供用户选择；不能把源模型当毛坯继续改。\n"
L"支承坐标系需从实际模型或用户指定定位建立，缺少位置或方向时只问必要参数。执行失败会移除本次新增结构及其辅助参照并核对恢复；恢复未能核实则停止该会话后续结构写入。\n";
#endif
