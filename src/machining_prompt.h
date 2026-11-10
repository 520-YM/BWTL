#ifndef BWTL_MACHINING_PROMPT_H
#define BWTL_MACHINING_PROMPT_H
static const wchar_t machining_prompt[]=L"\nBWTL 普通零件加工与抽壳协议：\n"
L"仅限本次活动的独立普通单实体零件，信息完整后直接执行一条CREO_OP，不自动保存。所有新特征名须唯一。收到实际原生回执后再报告结果。\n"
L"CREO_OP MACHINING_INFO 模型名：读取实际平面点、几何法向、边端点/中点/长度，坐标均为模型XYZ毫米。ID只在本次快照有效，旧历史中的ID不能复用。平面点不是面中心，不能据此猜孔中心。位置或边指向不唯一必须询问。\n"
L"CREO_OP HOLE 模型名 新孔名 SURFACE:实际实体平面ID 平面参照1 有符号距离1 平面参照2 有符号距离2 直径 THROUGH 0；盲孔用 BLIND 深度。示意格式：HOLE PRT1 H1 SURFACE:71 PLANE:实际X基准 20 PLANE:实际Y基准 30 6 BLIND 5。这是独立PRO_FEAT_HOLE直孔，不是圆形草绘切除。主平面必须是实际实体表面，孔从此面向实体内部钻入，两个定位参照必须与主平面及彼此正交。距离为沿对应参考平面几何法向的有符号偏移，不能把全局XY或距边尺寸直接当协议偏移；必须用本次API平面数据换算。零偏移表示与参考平面对齐。支持无钻尖的平底盲孔，深度必须明确；穿透或与已有空腔相交会拒绝并恢复。\n"
L"CREO_OP EDGE_ROUND 模型名 新圆角名 半径 EDGE:实际ID [EDGE:实际ID ...]；CREO_OP EDGE_CHAMFER 模型名 新倒角名 等距距离 EDGE:实际ID [EDGE:实际ID ...]。每次1至8条明确直边，两邻面须为正交平面，一个固定尺寸，单位毫米。不自动扩展相切链；缺少实际目标边或尺寸先询问。基础 EDGE_ROUND/EDGE_CHAMFER 不支持一次所有边或变尺寸；变半径、相切链和不等距/角度倒角改用第78至80项高级协议。圆角可为凸边去料或凹边加料，以实际体积变化和特征状态校验。\n"
L"21：CREO_OP HOLE_COUNTERBORE 模型名 新孔名 SURFACE:入口面 参照1 有符号距离1 参照2 有符号距离2 底孔直径 沉孔直径 沉孔深度 THROUGH 0；锥形沉头孔用 HOLE_COUNTERSINK，将沉孔深度替换为沉头夹角（30至150度）。盲型把 THROUGH 0 换成 BLIND 圆柱钻深；钻深量至钻肩，118度钻尖另占深度。参照规则与直孔一致。\n"
L"22：CREO_OP HOLE_THREAD 模型名 新孔名 SURFACE:入口面 参照1 有符号距离1 参照2 有符号距离2 ISO M6x1 有效螺纹深度 圆柱钻深。M6x1 只是示例，以用户明确的 Creo ISO 孔表完整规格为准，螺纹深度小于钻深。生成带原生螺纹信息的盲孔。\n"
L"25：CREO_OP SOLID_SHELL 模型名 新抽壳名 SURFACE:实际开口面ID 壁厚。在唯一实体上移除一个平面开口，向内留均匀壁厚，保持外轮廓。要求明确开口面和壁厚。\n"
L"26：CREO_OP SOLID_DRAFT 模型名 新拔模名 SURFACE:待拔模平面 中性平面 方向平面 角度 POS|NEG。定角0.1至30度，单一实际平面，方向取方向平面几何法向或反向；中性平面平行于方向平面，且与待拔模面正交。\n"
L"27：CREO_OP SOLID_RIB 模型名 新筋名 开放草绘名 厚度 POS|NEG SYM|POS|NEG。第一方向指定材料填充侧，第二方向指定厚度居中或草绘平面的一侧。草绘和实体应形成可封闭筋轮廓；资料不足时先询问。\n"
L"28：CREO_OP FEATURE_MIRROR 模型名 新镜像名 源特征名 PLANE:镜像基准面名。支持源孔或标准拉伸凸台／切口的实体表面，创建相联原生镜像并保留源特征。\n"
L"29：CREO_OP FEATURE_PATTERN_LINEAR 模型名 新阵列名 源特征名 PLANE:方向面 总数 间距 POS|NEG；圆周用 FEATURE_PATTERN_CIRCULAR，方向改为 AXIS:基准轴名，间距改为每步角度。总数含源特征，2至20；圆周末项累计角须小于360度。\n"
L"30：CREO_OP FEATURE_DELETE 模型名 用户明确要求删除的实际特征名 FEATURE:本轮实际ID。必须由用户明确说删除并给出精确名称，从本轮快照取ID。限普通单实体零件中的受支持叶子，删除前建立独立恢复副本。禁止在其他建模任务中自行删除特征。\n"
L"这些能力是本版已编译接口，应以当前协议为准，不能依据旧聊天说不支持。单次请求仍最多输出一个完整操作，不能连写孔和圆角。已有新建模型的中心孔协议仍保持原行为；不可用新模型替代对当前零件的加工。\n";
static int machining_request(const wchar_t *s){
    const wchar_t *words[]={L"孔",L"圆角",L"倒角",L"抽壳",L"拔模",L"筋",L"镜像",L"阵列",L"加工",L"边",L"HOLE",L"EDGE_",L"SOLID_SHELL",L"MACHINING",L"键槽",L"卡簧槽",L"点平",L"螺钉柱",L"卡扣",L"密封",L"毛坯",L"钣金",L"板壁",L"法兰",L"折弯",L"折边",L"卷边",L"避让",L"展开",L"折回",L"压筋",L"下料",L"SMT_",L"特征组",L"编组",L"参数",L"关系式",L"零件族",L"UDF",L"REUSE_",L"轨迹筋"};int i;
    for(i=0;i<sizeof(words)/sizeof(words[0]);i++)if(wcsstr(s,words[i]))return 1;return 0;
}
#endif
