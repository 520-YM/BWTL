#ifndef BWTL_QUICK_PARTS_PROMPT_H
#define BWTL_QUICK_PARTS_PROMPT_H
static const wchar_t quick_prompt[]=
L"\n第231–240项是整件快速生成。仅在本轮明确要求新建相应整件时使用，下列均以 CREO_OP 开头，N 为新零件名，限31位英文字母数字下划线或短横线，单位 mm。不会改当前模型，不自动保存。必要尺寸缺失且用户未授权选值时，询问缺少的参数，不可用普通长方体代替。\n"
L"QUICK_L N X宽 Y底深 厚 折边高 内R 底孔X间距 孔径。一个90°翻边，底面2孔。\n"
L"QUICK_U N X宽 Y底深 厚 折边高 内R 底孔X间距 孔径。两侧90°翻边，底面2孔。\n"
L"QUICK_POWER N X宽 Y底深 厚 折边高 内R 底孔X间距 底孔Y间距 孔径。双翻边托架，底面4孔。\n"
L"QUICK_SCREEN N X宽 Y高 厚 居中窗X宽 窗Y高 孔X间距 孔Y间距 孔径。背板窗口及4孔。\n"
L"QUICK_HINGE N X宽 Y长 厚 孔X间距 孔Y间距 孔径。铰链四孔垫板。\n"
L"QUICK_LOCK N X宽 Y长 厚 两孔Y间距 孔径 缺口Y宽 缺口X深。负X侧居中缺口，另一端两孔。\n"
L"QUICK_CLIP N X沿线束方向长度 Y底深 厚 侧壁高 内R 底孔X间距 孔径。开口U形线束夹，底面2孔。\n"
L"QUICK_SPACER N 大段外径 小段外径 通孔直径 大段长度 小段长度。两段同轴隔套。\n"
L"QUICK_STOP N X长 Y宽 厚 槽总长 槽宽 两槽Y间距。两条平行X的长圆调节槽。\n"
L"QUICK_COVER N X宽 Y长 厚 左右孔距边 上下孔距边 四角孔径 把手两孔X间距 把手孔径。平盖板，四角紧固孔及居中把手孔。\n"
L"QUICK_STOOL N 凳面X长 凳面Y宽 凳面厚 总高 方腿边长 腿外侧距凳面边。适合矩形凳面四条直方腿的简式凳子。用户说尺寸你定、尺寸自己给或自己想象时，你可直接定合理尺寸并输出一条对应协议，例如 320 320 25 450 35 25，不询问已委托的尺寸。不要先创建单个长方体代替整件。\n"
L"QUICK_CHAIR N 凳面X长 凳面Y宽 凳面厚 地面到凳面顶高度 方腿边长 腿外侧距凳面边 靠背宽 靠背厚 靠背高出凳面。要求新画凳子并加靠背时，即使分在两句话，也用 BWTL_INTENT NEW 与一条 CREO_OP QUICK_CHAIR 一次生成四腿和后侧居中的直板靠背。优先沿用已约定尺寸，未约定且已委托时可取 320 320 25 450 35 25 270 20 300。不要拆成长方体或后续草绘步骤。给现有凳子加靠背使用现有模型操作 STOOL_BACK。\n";
#endif
