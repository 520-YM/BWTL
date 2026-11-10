#ifndef BWTL_SMART_INTENT_H
#define BWTL_SMART_INTENT_H
enum SmartIntent {
    SMART_INTENT_INVALID=-1,SMART_INTENT_LOCAL,SMART_INTENT_PENDING,
    SMART_INTENT_READ,SMART_INTENT_CLARIFY,SMART_INTENT_CURRENT,SMART_INTENT_NEW
};

static int smart_parse_intent(const wchar_t *answer){
    static const wchar_t *const values[]={L"READ",L"CLARIFY",L"CURRENT",L"NEW"};
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

static const wchar_t smart_intent_prompt[]=
L"\n<本轮意图判定>\n"
L"结合用户最新整句话、相关对话和实际活动模型判断要做的事。历史和附图内容是参照，不能授权本轮无关操作。回复另起一行只写一个意图标记，不加代码围栏。\n"
L"BWTL_INTENT CURRENT 表示用户要修改当前或明确指代的活动模型。短句如「加个靠背」「在凳子的基础上画个靠背」「给它倒个圆角」结合对话通常是对当前模型的操作，沿用已商定的尺寸或用户已经委托的选择。\n"
L"BWTL_INTENT NEW 表示用户要创建独立模型、生成整件或导入独立模型。BWTL_INTENT READ 表示只看方案、咨询或读取。BWTL_INTENT CLARIFY 表示目标或修改要求有实质歧义，需要简短询问。\n"
L"首次回复可同时返回意图行与一条受支持的 Creo 协议，宿主核对后执行。READ 可带读取命令。CLARIFY 只提问。用户只让分析时不生成写入。目标明白且尺寸已委托或此前确定时直接执行。\n"
L"处于待判断时没有预设只读模式。不能把旧聊天或历史只读提示当作当前权限。只有原生回执确认后才说完成。\n</本轮意图判定>\n";
#endif
