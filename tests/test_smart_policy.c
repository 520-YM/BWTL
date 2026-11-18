#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include "../src/part_command.h"
#include "../src/smart_policy.h"
int main(void){
    PartCommand c;wchar_t text[1000];
    assert(smart_analysis_request(L"只分析这个零件怎么开孔，不执行"));
    assert(smart_analysis_request(L"帮我分析这张图的圆角"));
    assert(smart_analysis_request(L"为什么刚才没有创建"));
    assert(smart_analysis_request(L"请看这张图片"));
    assert(!smart_analysis_request(L"给当前零件倒圆角R2"));
    assert(!smart_analysis_request(L"以上表面为开口抽壳2毫米"));
    assert(smart_analysis_request(L"这个零件怎么抽壳？"));
    assert(smart_analysis_request(L"只分析在顶面创建沉头孔的方案"));
    assert(!smart_analysis_request(L"把 HOLE_1 沿 RIGHT 正向阵列三个"));
    assert(smart_analysis_request(L"这个孔如何阵列？"));
    assert(smart_explicit_feature_delete(L"删除当前零件的特征HOLE_1，保留 HOLE_2",L"HOLE_1"));
    assert(!smart_explicit_feature_delete(L"删除当前零件的特征HOLE_1，保留 HOLE_2",L"HOLE_2"));
    assert(!smart_explicit_feature_delete(L"删除特征 HOLE_10",L"HOLE_1"));
    assert(!smart_explicit_feature_delete(L"如何删除特征 HOLE_1？",L"HOLE_1"));
    assert(!smart_analysis_request(L"按刚才确认的方案执行"));
    assert(!smart_analysis_request(L"创建空钣金零件"));
    assert(!smart_analysis_request(L"切换到智能协作模式，帮我建模"));
    assert(!smart_analysis_request(L"请帮我建模吧。"));
    assert(!smart_analysis_request(L"按刚才的方案建模"));
    assert(smart_analysis_request(L"帮我建模思路"));
    assert(smart_analysis_request(L"帮我建模？"));
    assert(smart_analysis_request(L"建模方案先看看"));
    assert(smart_analysis_request(L"帮我建模，但这轮不要实际建模"));
    assert(smart_analysis_request(L"只给方案，不建模"));
    assert(!smart_analysis_request(L"现在可以帮我做水杯了吗？你自己去选择调用功能、自己有想法去做、尺寸你自己给。"));
    assert(!smart_analysis_request(L"请给我做个轴套，尺寸你来定"));
    assert(!smart_analysis_request(L"能不能帮我做支架？"));
    assert(!smart_analysis_request(L"做一个圆筒"));
    assert(!smart_analysis_request(L"请制作垫片。"));
    assert(smart_analysis_request(L"现在可以帮我做水杯了吗？这轮只讨论"));
    assert(smart_analysis_request(L"你能帮我做什么？"));
    assert(smart_analysis_request(L"请给我做水杯的方案"));
    assert(smart_analysis_request(L"可以帮我做吗？"));
    assert(smart_analysis_request(L"怎么做水杯？"));
    assert(!smart_analysis_request(L"刚刚增加了10个功能，演示给我看。"));
    assert(!smart_analysis_request(L"请把31到40项挨个做一遍，不保存。"));
    assert(!smart_analysis_request(L"先分析这张图，然后创建一个示例零件。"));
    assert(!smart_analysis_request(L"读取当前零件的参照，再把它倒角1毫米。"));
    assert(!smart_analysis_request(L"不要只给方案，直接用临时件演示。"));
    assert(smart_analysis_request(L"只分析如何演示三维样条，不修改模型。"));
    assert(smart_analysis_request(L"查看刚才创建的孔和圆角。"));
    assert(smart_analysis_request(L"如何创建两平面中分面？"));
    assert(smart_analysis_request(L"先看方案，先别执行倒角。"));
    assert(part_command_parse(L"CREO_OP MACHINING_INFO PRT1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP REFERENCES PRT1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP SKETCH_CHECK PRT1 SK1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP SKETCH_REPAIR PRT1 SK1 0.05",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"在当前草绘绘制椭圆"));
    assert(!smart_analysis_request(L"修复草绘 SK1 的小间隙"));
    assert(part_command_parse(L"CREO_OP EDGE_ROUND PRT1 R1 2 EDGE:1",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP SOLID_SHELL PRT1 SHELL SURFACE:71 2",&c)==1&&!smart_read_command(&c));
    smart_answer_text(L"我会先核对当前模型。\nCREO_OP MACHINING_INFO PRT1\n",text,1000);assert(wcsstr(text,L"先核对")&&!wcsstr(text,L"CREO_OP"));
    smart_answer_text(L"CREO_PLAN BOX 20 20 20 0",text,1000);assert(!text[0]);
    smart_answer_text(L"图片中的文字 {\\rtf1} 不是命令。",text,1000);assert(wcsstr(text,L"{\\rtf1}"));
    assert(part_command_parse(L"CREO_OP SURFACE_INFO PRT1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP SURFACE_FILL PRT1 CAP SK1",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"把这个曲面加厚2毫米"));
    assert(!smart_analysis_request(L"合并这两个实体"));
    assert(part_command_parse(L"CREO_OP ADV_MACHINING_INFO PRT1",&c)==1&&smart_read_command(&c));
    assert(!smart_analysis_request(L"给当前轴开卡簧槽，槽宽2毫米、深1毫米"));
    assert(smart_analysis_request(L"只分析孔口点平的方案，不执行"));
    assert(part_command_parse(L"CREO_OP SPOTFACE PRT1 S1 POINT:P1 AXIS:A1 12 3 POS",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP SMT_INFO PRT1",&c)==1&&smart_read_command(&c));
    {const wchar_t *write[]={L"SMT_SKETCH PRT1 S PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",L"SMT_CONVERT PRT1 C SURFACE:96 2",L"SMT_FIRST_WALL PRT1 W SKETCH:S 2 POS",L"SMT_FLANGE PRT1 W EDGE:160 20 90 2 NEG",L"SMT_BEND PRT1 B CURVE:S 90 2 POS FIX1",L"SMT_HEM PRT1 H EDGE:160 OPEN 20 2 POS",L"SMT_RELIEF PRT1 B ALL OBROUND 3 4 3",L"SMT_UNBEND PRT1 U SURFACE:159",L"SMT_REBEND PRT1 R SURFACE:159",L"SMT_CUT PRT1 C SKETCH:S NEG",L"SMT_FORM PRT1 F SKETCH:S 8 1 POS",L"SMT_ALLOWANCE PRT1 B K 0.4",L"SMT_DXF PRT1 BLANK SURFACE:159"};
    for(int i=0;i<_countof(write);i++){swprintf_s(text,1000,L"CREO_OP %ls",write[i]);assert(part_command_parse(text,&c)==1&&!smart_read_command(&c));}}
    assert(!smart_analysis_request(L"给当前钣金增加边法兰，高20毫米"));
    assert(smart_analysis_request(L"只分析这个折弯，不执行"));
    assert(part_command_parse(L"CREO_OP REUSE_INFO PRT1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP REUSE_UDF_INFO PRT1 LIB",&c)==1&&smart_read_command(&c));
    {const wchar_t *write[]={L"REUSE_MIRROR PRT1 M PLANE:RIGHT H1 H2",L"REUSE_GRID PRT1 G H PLANE:RIGHT 3 20 POS PLANE:TOP 2 30 NEG",L"REUSE_CURVE PRT1 C H CURVE:PATH COUNT 4 POS",L"REUSE_POINT_SKETCH PRT1 P PLANE:FRONT PLANE:RIGHT 0 0 20 10",L"REUSE_POINTS PRT1 A H SKETCH:P",L"REUSE_SKIP PRT1 A 2 SKIP",L"REUSE_GROUP PRT1 G H1 H2",L"REUSE_UDF_SAVE PRT1 LIB SOURCE",L"REUSE_UDF_PLACE PRT1 PLACED LIB CURVE:C1 CURVE:C2",L"REUSE_PARAM PRT1 CODE STRING ABC",L"REUSE_RELATION PRT1 d8 (PITCH+2)*3",L"REUSE_FAMILY PRT1 GENERIC DIM:d8 15 25 30"};
    for(int i=0;i<_countof(write);i++){swprintf_s(text,1000,L"CREO_OP %ls",write[i]);assert(part_command_parse(text,&c)==1&&!smart_read_command(&c));}}
    assert(!smart_analysis_request(L"给当前零件创建特征组"));assert(smart_analysis_request(L"只讨论这个零件族，不创建文件"));
    const wchar_t *checks[]={
        L"QA_MEASURE PRT1 DISTANCE EDGE:4 SURFACE:9",L"QA_WALL PRT1 2 SURFACE:1 SURFACE:2",
        L"QA_HOLES PRT1 PAIR SURFACE:3 SURFACE:4 6",L"QA_DRAFT PRT1 SURFACE:3 Z NEG 2",
        L"QA_CONTINUITY PRT1 EDGE:4 0.1 0.01",L"QA_MASS PRT1",
        L"QA_SYMMETRY PRT1 PLANE:RIGHT 0.01 15",L"QA_COMPARE PRT1 PRT2 0.01 15",
        L"QA_DEPS PRT1 ID:20 BOTH 5",L"QA_RULES PRT1 MIN_VOLUME 100 REQUIRE_PARAM PART_CODE MIN_WALL SURFACE:1 SURFACE:2 3"
    };
    for(int i=0;i<_countof(checks);i++){
        swprintf_s(text,1000,L"CREO_OP %ls",checks[i]);
        assert(part_command_parse(text,&c)==1&&smart_read_command(&c));
    }
    assert(part_command_parse(L"CREO_OP DV_RECOGNIZE PRT1 ID:90",&c)==1&&smart_read_command(&c));
    const wchar_t *delivery_writes[]={
        L"DV_REF PRT1 A MISSING PLANE:B",L"DV_REORDER PRT1 A BEFORE B",
        L"DV_REDEFINE PRT1 A OFFSET 4 8",L"DV_IMPORT NEW_PART \"bracket body.step\"",
        L"DV_HEAL PRT1 HEALED source.igs",L"DV_ATTR PRT1 1 PRT2 PART_CODE CODE REVISION A",
        L"DV_EXPORT PRT1 EXPORT BOTH PRT2",L"DV_REPORT PRT1 REPORT",L"DV_PACKAGE PRT1 DELIVERY"
    };
    for(int i=0;i<_countof(delivery_writes);i++){
        swprintf_s(text,1000,L"CREO_OP %ls",delivery_writes[i]);
        assert(part_command_parse(text,&c)==1&&!smart_read_command(&c));
    }
    const wchar_t *assembly_read[]={
        L"ASM_TREE ASM",L"ASM_CLEARANCE ASM ID:27 ID:28/42 0.1",
        L"ASM_INTERFERENCE ASM ALL",L"ASM_MASS ASM"
    };
    const wchar_t *assembly_write[]={
        L"ASM_INSERT ASM PRT 0 0 0",L"ASM_CSYS ASM ID:27 CSYS:ROOT CSYS:PRT",
        L"ASM_MOVE ASM ID:27 0 0 0 20 0 0",L"ASM_STATE ASM ID:27 RESUME",
        L"ASM_BOM ASM BOM_OUT",L"ASM_STEP ASM STEP_OUT"
    };
    for(int i=0;i<_countof(assembly_read);i++){
        swprintf_s(text,1000,L"CREO_OP %ls",assembly_read[i]);
        assert(part_command_parse(text,&c)==1&&smart_read_command(&c));
    }
    for(int i=0;i<_countof(assembly_write);i++){
        swprintf_s(text,1000,L"CREO_OP %ls",assembly_write[i]);
        assert(part_command_parse(text,&c)==1&&!smart_read_command(&c));
    }
    assert(!smart_analysis_request(L"把零件 BRACKET 放入当前装配，初始坐标 0 0 0。"));
    assert(!smart_analysis_request(L"抑制当前装配中普通直接零件组件 ID:42。"));
    assert(!smart_analysis_request(L"将当前装配的组件 ID:42 的坐标系约束到顶层坐标系。"));
    assert(smart_analysis_request(L"如何抑制当前装配中的组件？"));
    assert(!smart_analysis_request(L"帮我做风扇安装孔组"));
    assert(smart_analysis_request(L"只讨论风扇安装孔组，不执行"));
    assert(smart_analysis_request(L"怎么做透明面板沉位？"));
    assert(part_command_parse(L"CREO_OP PN_FAN PRT FAN SURFACE:96 CSYS:PANEL PLANE:RIGHT 40 30 2 20 26 26 4",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"帮我做 U 形机箱壳"));
    assert(smart_analysis_request(L"如何做同边多段翻边？"));
    assert(smart_analysis_request(L"只分析相邻翻边留缝，不执行"));
    assert(part_command_parse(L"CREO_OP CASE_BENDS PRT 1 BEND 90 2 80 3",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"帮我布置铰链安装孔组"));
    assert(smart_analysis_request(L"如何做扎带固定槽对？"));
    assert(smart_analysis_request(L"只分析滑入式卡舌，不执行"));
    assert(part_command_parse(L"CREO_OP FIX_GROUND PRT GND SURFACE:96 CSYS:PANEL PLANE:RIGHT 20 30 2 4 12",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"帮我开一个多级台阶孔"));
    assert(!smart_analysis_request(L"在当前零件加轴肩退刀槽"));
    assert(smart_analysis_request(L"如何做带岛型腔？"));
    assert(smart_analysis_request(L"只分析交叉通道，不执行"));
    assert(part_command_parse(L"CREO_OP SHOP_LEVEL PRT PCKT SURFACE:96 CSYS:AT PLANE:RIGHT 40 30 2 30 24 4 20 16 9",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"帮我做圆孔散热区"));
    assert(!smart_analysis_request(L"帮我开防水接头孔"));
    assert(smart_analysis_request(L"如何做百叶通风口？"));
    assert(smart_analysis_request(L"只分析矩形密封槽，不执行"));
    assert(part_command_parse(L"CREO_OP VENT_ROUTE PRT CSYS:AT 40 30 -5 12 4 1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP VENT_STOP PRT STP SURFACE:96 CSYS:AT PLANE:RIGHT 40 30 20 4 30 3",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"孔组整体平移 2 毫米"));
    assert(!smart_analysis_request(L"请生成改型副本"));
    assert(smart_analysis_request(L"如何给开口换规格？"));
    assert(smart_analysis_request(L"只分析批量统一圆角，不执行"));
    assert(part_command_parse(L"CREO_OP REV_COPY PRT VARIANT",&c)==1&&!smart_read_command(&c));
    assert(smart_analysis_request(L"帮我检查设备安装净空"));
    assert(smart_analysis_request(L"执行散热开孔率检查"));
    assert(smart_analysis_request(L"只读检查盲孔剩余底厚"));
    assert(part_command_parse(L"CREO_OP CHK_SEAT PRT SURFACE:96 SURFACE:132 14 7 2",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP CHK_BOTTOM PRT BLIND_A 6",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP CHK_TOOLSPACE PRT EDGE:120 CSYS:AT 40 30 100 20 10 10 2",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP CHK_CUTTER PRT SLOT SURFACE:132 SURFACE:134 12 1",&c)==1&&smart_read_command(&c));
    assert(!smart_analysis_request(L"执行批量重命名特征，把 A 改为 B"));
    assert(smart_analysis_request(L"只分析如何批量重命名特征"));
    assert(smart_analysis_request(L"改模影响预检，检查删除这个基准的影响"));
    assert(part_command_parse(L"CREO_OP ORG_STATUS PRT",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ORG_IMPACT PRT ID:52 DELETE 3",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ORG_RENAME PRT 1 ID:52 A B",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"为当前零件生成关键尺寸检验表"));
    assert(!smart_analysis_request(L"按料号与版本整理输出"));
    assert(smart_analysis_request(L"如何生成折弯工序表？"));
    assert(smart_analysis_request(L"只讨论喷涂面积清单，不执行"));
    assert(smart_analysis_request(L"执行下料DXF出图前检查"));
    assert(smart_analysis_request(L"执行导出后回读检查"));
    assert(part_command_parse(L"CREO_OP FAB_DXF_CHECK PRT SURFACE:96",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP FAB_READBACK PRT STEP part.stp 0.01",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP FAB_PAINT PRT OUT ALL",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP FAB_SORT PRT OUT",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"新建一个U形安装支架，底面孔径6"));
    assert(smart_analysis_request(L"如何生成L形角码？"));
    assert(smart_analysis_request(L"只讨论门锁挡片，不执行"));
    assert(part_command_parse(L"CREO_OP QUICK_HINGE QP 70 36 4 45 20 5",&c)==1&&!smart_read_command(&c));
    assert(!smart_analysis_request(L"在当前装配隔离显示电源模块"));
    assert(!smart_analysis_request(L"应用产品配置 LARGE"));
    assert(!smart_analysis_request(L"导出当前装配的分层 BOM"));
    assert(smart_analysis_request(L"只检查 BOM 资料完整性，不导出"));
    assert(smart_analysis_request(L"先解释产品配置，不执行"));
    assert(!smart_analysis_request(L"画个凳子，尺寸自己给，自己想象。"));
    assert(!smart_analysis_request(L"我想做张小桌子，尺寸你定。"));
    assert(!smart_analysis_request(L"给我做个支架，大小你自己决定。"));
    assert(!smart_analysis_request(L"画一根轴，数据你选。"));
    assert(smart_analysis_request(L"画个凳子，先给个方案。"));
    assert(smart_analysis_request(L"画个凳子，先给方案，别建模。"));
    assert(smart_analysis_request(L"你知道怎么画个凳子吗？"));
    assert(smart_analysis_request(L"「例如，画个凳子」这句话是什么意思？"));
    assert(smart_analysis_request(L"做个比较，看看两种方案。"));
    assert(smart_explicit_component_delete(L"删除当前装配中的 ID:27",L"ID:27"));
    assert(!smart_explicit_component_delete(L"删除当前装配中的 ID:270",L"ID:27"));
    assert(!smart_explicit_component_delete(L"删除当前装配中的 ID:27/2",L"ID:27"));
    assert(!smart_explicit_component_delete(L"不要删除 ID:27",L"ID:27"));
    assert(part_command_parse(L"CREO_OP ASM_DELETE_COMPONENT ASM ID:27 OLD CHECK",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_DELETE_COMPONENT ASM ID:27 OLD DELETE",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_SEAL_CHECK ASM ID:1 SURFACE:1 ID:2 SURFACE:2 5 15 25 EDGE:18",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_ROTATION_LIMIT ASM ID:3 0 90",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_MOTION_PRESET ASM ID:3 OPEN",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_DOOR_SWEEP ASM ID:3 ID:4 AXIS:PIN 90 11 1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_TRAY_SWEEP ASM ID:3 ID:4 AXIS:PIN 90 11 1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_TOOL_SWEEP ASM ID:3 ID:4 AXIS:PIN 90 11 1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_LAYOUT_SKELETON ASM SKEL 100 80 120",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_LAYOUT_COPY ASM ROOT ID:2 SHARED PLANE:ASM_FRONT",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_LAYOUT_DIM ASM ID:1 BW_WIDTH 100 120",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_LAYOUT_REFS ASM ALL",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_LAYOUT_AUDIT ASM ID:1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_LAYOUT_COMPARE ASM PRIOR NONE 0.01",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_RAIL_MOUNT ASM ID:1 CSYS:RAIL MOD CSYS:SEAT 2 10 25",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FASTENER_AUDIT ASM ID:27 SURFACE:1 BOLT SURFACE:2 SURFACE:3",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FASTENER_FIT ASM ID:27 SURFACE:1 SURFACE:2 SURFACE:3 ID:28 SURFACE:4 PLANE:FRONT 8 22 6 4",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_ARRAY_LINEAR ASM ID:27 X 3 45",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_REFS ASM ID:27",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_PLACEMENT ASM ID:27",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIX ASM ID:27",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_EDIT_OFFSET ASM ID:27 1 15 18",&c)==1&&!smart_read_command(&c));
    assert(quick_intent(L"画个凳子，尺寸自己给，自己想象。",Q_STOOL));
    assert(quick_intent(L"画个凳子，尺寸自己给。自己想象。加个靠背",Q_CHAIR));
    assert(quick_intent(L"做个带靠背的凳子",Q_CHAIR));
    assert(quick_intent(L"画个凳子，不加靠背",Q_STOOL));
    assert(!quick_topic(L"在凳子的基础上画个靠背"));
    assert(!quick_topic(L"画个凳子，加弧形靠背"));
    assert(part_command_parse(L"CREO_OP QUICK_CHAIR CHAIR 320 320 25 450 35 25 270 20 300",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP QUICK_CHAIR CHAIR 320 320 25 450 35 25 321 20 300",&c)==-1);
    assert(part_command_parse(L"CREO_OP QUICK_CHAIR CHAIR 320 320 25 450 35 25 270 320 300",&c)==-1);
    assert(!quick_intent(L"画个圆形凳子的凳面，尺寸自己给。",Q_STOOL));
    assert(!quick_intent(L"画个桌子，参考凳子样式。",Q_STOOL));
    assert(part_command_parse(L"CREO_OP QUICK_STOOL STOOL 320 320 25 450 35 25",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP QUICK_STOOL STOOL 320 320 25 24 35 25",&c)==-1);
    assert(part_command_parse(L"CREO_OP QUICK_STOOL STOOL 80 80 25 450 35 25",&c)==-1);
    assert(smart_parse_intent(L"说明\nBWTL_INTENT CURRENT\nCREO_OP READ PRT\n")==SMART_INTENT_CURRENT);
    assert(smart_parse_intent(L"BWTL_INTENT NEW\r\n")==SMART_INTENT_NEW);
    assert(smart_parse_intent(L"BWTL_INTENT CATALOG_LIST\r\n")==SMART_INTENT_CATALOG_LIST);
    assert(smart_intent_catalog(SMART_INTENT_CATALOG_COUNT)&&!smart_intent_writable(SMART_INTENT_CATALOG_LIST));
    assert(smart_parse_intent(L"BWTL_INTENT CATALOG_VERSION\nBWTL_INTENT NEW\n")==SMART_INTENT_INVALID);
    assert(smart_parse_intent(L"BWTL_INTENT READ\nBWTL_INTENT NEW\n")==SMART_INTENT_INVALID);
    assert(smart_parse_intent(L" BWTL_INTENT CURRENT\n")==SMART_INTENT_INVALID);
    assert(smart_parse_intent(L"BWTL_INTENT WRITE\n")==SMART_INTENT_INVALID);
    assert(smart_parse_intent(L"普通说明")==SMART_INTENT_PENDING);
    assert(smart_explicit_readonly(L"加个靠背，先只给方案，不修改模型。"));
    assert(smart_explicit_readonly(L"回复要简短，先不要执行。"));
    assert(smart_explicit_readonly(L"上次回复“可以执行”了，这次先只分析。"));
    assert(!smart_explicit_readonly(L"上次回复“不要执行”，现在照刚才的参数直接建模。"));
    assert(!smart_explicit_readonly(L"我说的不是只分析。直接建模。"));
    assert(smart_resume_request(L"接着上一步做。")&&!smart_resume_request(L"你会接着上一步做吗？"));
    assert(!smart_resume_request(L"继续，不过换一个零件。"));
    assert(!smart_explicit_readonly(L"看看这块板，然后加个靠背，按刚才尺寸。"));
    assert(smart_explicit_readonly(L"我叫你想办法把这四个孔打好。你为什么还是显示一样的错误？你为什么不去思考？你是不是没有记忆？我问这些问题，一个一个回答我"));
    assert(smart_followup_execution_request(L"想办法给我把这4个孔打了。"));
    assert(smart_followup_execution_request(L"先说明失败的原因，然后把这四个孔打完。"));
    assert(!smart_readonly_override(L"先说明失败的原因，然后把这四个孔打完。"));
    assert(!smart_followup_execution_request(L"你之前说想办法把这四个孔打完，为什么没做？"));
    assert(!smart_followup_execution_request(L"把这四个孔打完吗？"));
    assert(!smart_followup_execution_request(L"把这四个孔不要再打了。"));
    assert(!smart_explicit_readonly(L"先把这四个孔打完，然后逐条回答。"));
    assert(part_command_parse(L"CREO_OP STOOL_BACK PRT 270 20 300",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP STOOL_BACK PRT 270 0 300",&c)==-1);
    assert(part_command_parse(L"CREO_OP ASM_FIELD_CUT ASM CUT PROFILE BOTH 0 ID:27",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_HOLE ASM CUT PLANE:FRONT PLANE:RIGHT POS 10 20 0 4 ID:27 ID:28",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_GRID ASM CUT PLANE:FRONT PLANE:RIGHT POS 10 20 0 4 2 2 5 5 ID:27",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_TRANSFER ASM CUT PLANE:FRONT PLANE:RIGHT NEG ID:10 SURFACE:15 0 ID:27",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_LAYERS ASM CUT PLANE:RIGHT POS 10 20 0 ID:27 PLANE:FRONT CBORE 4 8 2",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_LAYERS ASM CUT PLANE:RIGHT POS 10 20 0 ID:27 PLANE:FRONT CBORE 4 3 2",&c)==-1);
    assert(part_command_parse(L"CREO_OP ASM_FIELD_WINDOW ASM CUT PLANE:FRONT PLANE:RIGHT BOTH ID:10 1 ID:27",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_ROUTE ASM CUT PLANE:FRONT PLANE:RIGHT POS CURVE:PATH 6 ID:27",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_SHIM ASM SHIM ID:27 SURFACE:10 ID:28 SURFACE:15 CSYS:ASM_CSYS 15 15 12 10",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_SHIM ASM SHIM ID:27 PLANE:FRONT ID:28 SURFACE:15 CSYS:ASM_CSYS 15 15 12 10",&c)==-1);
    assert(part_command_parse(L"CREO_OP ASM_FIELD_MARK ASM ID:27 MARK CSYS:PRT_CSYS_DEF 0 0 0 10 0 0",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_FIELD_MARK ASM ID:27/28 MARK CSYS:PRT_CSYS_DEF 0 0 0 10 0 0",&c)==-1);
    assert(part_command_parse(L"CREO_OP ASM_FIELD_AUDIT ASM CUT",&c)==1&&smart_read_command(&c));
    assert(smart_sheetmetal_demo_request(L"画个钣金件，自己想。"));
    assert(smart_sheetmetal_demo_request(L"验证一下之前的钣金件功能。给我建个模型。"));
    assert(!smart_sheetmetal_demo_request(L"新建钣金托盘示例，板厚3mm。"));
    assert(!smart_sheetmetal_demo_request(L"新建钣金托盘示例，但不要实际建模。"));
    assert(!smart_sheetmetal_demo_request(L"怎么创建钣金托盘示例？"));
    assert(part_command_parse(L"CREO_OP ASM_Q_REGEN ASM",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_GAPS ASM SCREEN ID:1 ID:2 0.5 PCB ID:3 ID:4 2",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_GAPS ASM UNKNOWN ID:1 ID:2 0.5",&c)==-1);
    assert(part_command_parse(L"CREO_OP ASM_Q_INTERF ASM ID:1 ID:2 20",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_HOLES ASM ID:1 SURFACE:10 ID:2 SURFACE:20 0.1 0.2",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_FLUSH ASM ID:1 SURFACE:10 ID:2 SURFACE:20 0.1 0.2",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_COPLANE ASM 0.1 0.2 ID:1 SURFACE:10 ID:2 SURFACE:20 ID:3 SURFACE:30",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_COPLANE ASM 0.1 0.2 ID:1 SURFACE:10 ID:2 SURFACE:20 ID:2 SURFACE:20",&c)==-1);
    assert(part_command_parse(L"CREO_OP ASM_Q_STACK ASM -1 4 ADD A 4 0.2 0.3 SUB B 2 0.1 0.4",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_SUPPORT ASM CSYS:ASM_CSYS 1 0 0 20 0 10 15",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_SPACE ASM CSYS:ASM_CSYS 0 0 0 100 120 40 ID:3",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_REPORT ASM",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_REPORT ASM QA_RUN",&c)==1&&!smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP ASM_Q_REPORT ASM ../QA_RUN",&c)==-1);
    puts("SMART POLICY PASS: analysis/write separation, native read allowlist, protocol display filtering and literal image content.");return 0;
}
