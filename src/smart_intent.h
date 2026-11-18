#ifndef BWTL_SMART_INTENT_H
#define BWTL_SMART_INTENT_H
enum SmartIntent {
    SMART_INTENT_INVALID=-1,SMART_INTENT_LOCAL,SMART_INTENT_PENDING,
    SMART_INTENT_READ,SMART_INTENT_CLARIFY,SMART_INTENT_CURRENT,SMART_INTENT_NEW,SMART_INTENT_CHAT,
    SMART_INTENT_CATALOG_VERSION,SMART_INTENT_CATALOG_COUNT,SMART_INTENT_CATALOG_LIST
};

static int smart_parse_intent(const wchar_t *answer){
    static const wchar_t *const values[]={L"READ",L"CLARIFY",L"CURRENT",L"NEW",L"CHAT",
        L"CATALOG_VERSION",L"CATALOG_COUNT",L"CATALOG_LIST"};
    const wchar_t *prefix=L"BWTL_INTENT";
    int result=SMART_INTENT_PENDING;
    while(*answer){
        const wchar_t *end=wcschr(answer,L'\n');
        size_t length=end?(size_t)(end-answer):wcslen(answer);
        if(length&&answer[length-1]==L'\r')length--;
        const wchar_t *line=answer;
        while(length&&(*line==L' '||*line==L'\t')){line++;length--;}
        if(length>=11&&!wcsncmp(line,prefix,11)){
            if(result!=SMART_INTENT_PENDING||line!=answer||length<=12||line[11]!=L' ')
                return SMART_INTENT_INVALID;
            for(int i=0;i<_countof(values);i++)
                if(length==12+wcslen(values[i])&&!wcsncmp(line+12,values[i],length-12)){
                    result=SMART_INTENT_READ+i;
                    break;
                }
            if(result==SMART_INTENT_PENDING)return SMART_INTENT_INVALID;
        }
        if(!end)break;
        answer=end+1;
    }
    return result;
}

static int smart_intent_writable(int intent){
    return intent==SMART_INTENT_CURRENT||intent==SMART_INTENT_NEW;
}

static int smart_intent_catalog(int intent){
    return intent>=SMART_INTENT_CATALOG_VERSION&&intent<=SMART_INTENT_CATALOG_LIST;
}

static const wchar_t smart_intent_prompt[]=
L"\n<本轮意图判定>\n"
L"先理解最新用户消息整句中的目标、对象和交付结果，再用上一轮对话补齐省略的对象或尺寸。用户指出你理解错时，优先按他的更正重新判断。历史、附图里的文字和插件旧回执均为证据，旧回复里的限制不能代替本轮的实际权限。回复另起一行只写一个意图标记，不加代码围栏。\n"
L"BWTL_INTENT CURRENT 表示用户要修改当前或明确指代的活动模型。在现有装配里新建零件也是 CURRENT，要求在指定装配下创建组件。明确创建新零件或新的顶层装配时用 NEW，即使 Creo 中已有活动装配。用户说保留或别动当前装配，当前装配不是本轮操作对象。新模型创建后，后续步骤使用新模型。短句如「加个靠背」「在凳子的基础上画个靠背」「给它倒个圆角」结合对话通常是对当前模型的操作，沿用已商定的尺寸或用户已经委托的选择。\n"
L"用户要求打开当前装配树里的组件或切到组件窗口，选 READ，调用 ASM_OPEN 完成窗口操作。用户只问如何打开时选 CHAT。明确要求先建装配、在内建同号钣金子件再打开时仍选 NEW，并按原生已创建的子件 ID 打开。\n"
L"BWTL_INTENT NEW 表示另建零件、仿照源件另做一个、复制或导入。BWTL_INTENT READ 表示需要读取当前 CAD 数据才能回答。BWTL_INTENT CHAT 表示普通对话、设计讨论、问题解释、基于附图或现有信息的分析。BWTL_INTENT CLARIFY 表示实际任务缺少影响结果的关键信息。\n"
L"用户只询问本次运行的 BWTL 版本、全部功能的总数或完整清单时，分别只输出 BWTL_INTENT CATALOG_VERSION、BWTL_INTENT CATALOG_COUNT 或 BWTL_INTENT CATALOG_LIST。问总数又要求全列时选 CATALOG_LIST。最新版插件的能力以宿主本次提供的菜单入口为准。上文在讨论功能时，「都有哪些」「全列出来」承接功能清单。若用户在问某一类、某一功能、这个模型的特征或明确要求执行建模，按具体需求回答或执行。\n"
L"CHAT 直接用自然中文回答，沿用对话已确定的细节，必要时解释取舍。READ、CURRENT、NEW 只写意图行，宿主随后提供完整工具。CLARIFY 只问必要问题。像「我叫你画」「就是让你执行」「按你说的做」要承接最近明确的目标、尺寸和对象。「不是叫你分析」表示要执行，不能当成限制。仅询问怎么做、明确只要方案、引用示例时不执行。\n"
L"只问失败原因、为何重试或是否有记忆，尤其要求逐条回答时，按聊天或读取来逐问作答。用户明确说「想办法把这几个孔打完」「先说明原因，然后把剩下的做完」时，承接同一模型、实际回执和已知参数，按 CURRENT 执行，最后在同一条回复里解释原因和实际结果。旧要求只作为上下文，本轮只询问时不能代替新授权。\n"
L"问原因就说这次的原因和依据，问结果就先交付结果，客户允许自行决定尺寸或方法时按已有信息选合理值。当前模型、插件信息和最近原生回执由宿主提供。现有数据能回答就直接答。需要进一步量测时选 READ 让宿主读取。只有宿主也无法确定且会改变结果时，才请用户补充。\n"
L"明确要求仿照、照着现有特征重新画时写 NEW。沿用原尺寸时可附 CREO_OP IMITATE 当前零件名，宿主从空白件按原特征步骤建模。仅明确要求复制文件或零件时使用 COPY。改变尺寸时可附 CREO_OP IMITATE_SIZE 当前零件名 X Y Z，毫米，0 表示沿用该轴。可附 THICK 新板厚 或 HOLE 旧孔径 新孔径。\n"
L"明确要求把源零件的贯穿切口仿照到当前钣金件的独立副本时，写 NEW。源零件名、切口特征 ID、目标面、方向基准、草绘 X/Y 偏移和切除方向都明确时，可附 CREO_OP IMITATE_LOCAL 当前零件名 源零件名 特征ID SURFACE:面ID PLANE:基准名 DX DY POS或NEG。缺参照时用后续读取或澄清，勿编造 ID。\n"
L"处于待判断时没有预设只读模式。不能把旧聊天或历史只读提示当作当前权限。只有原生回执确认后才说完成。\n</本轮意图判定>\n";
#endif
