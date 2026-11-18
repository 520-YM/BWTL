typedef struct {
    const wchar_t *begin,*end,*speaker;
} ChatContextTurn;

/* Keep recent turns intact as labeled evidence. Long receipts and lists must
   not displace a user's previous instruction or the assistant's question. */
static void chat_prompt_conversation(FILE *out,const wchar_t *text,const wchar_t *latest,int compact){
    static const wchar_t *const markers[]={L"用户：",L"助手："};
    static const wchar_t *const roles[]={L"用户",L"助手"};
    ChatContextTurn turns[13]={0};
    size_t count=0,keep=compact?8:12;
    for(const wchar_t *line=text;*line;){
        for(size_t i=0;i<_countof(markers);i++){
            size_t n=wcslen(markers[i]);
            if(wcsncmp(line,markers[i],n))continue;
            if(count)turns[count-1].end=line;
            if(count==_countof(turns)){
                memmove(turns,turns+1,(count-1)*sizeof(*turns));
                count--;
            }
            turns[count++]=(ChatContextTurn){line+n,NULL,roles[i]};
            break;
        }
        const wchar_t *next=wcschr(line,L'\n');
        if(!next){line+=wcslen(line);break;}
        line=next+1;
    }
    if(!count)return;
    turns[count-1].end=text+wcslen(text);
    size_t latest_length=wcslen(latest);
    const ChatContextTurn *last=&turns[count-1];
    if(last->speaker==roles[0]&&latest_length&&latest_length<=(size_t)(last->end-last->begin)&&
        !wcsncmp(last->begin,latest,latest_length)&&
        (!last->begin[latest_length]||last->begin[latest_length]==L'\n'))count--;
    if(!count)return;
    fwprintf(out,L"\n<近期对话，按时间顺序，仅作上下文>\n");
    for(size_t i=count>keep?count-keep:0;i<count;i++){
        const ChatContextTurn *turn=&turns[i];
        size_t length=(size_t)(turn->end-turn->begin);
        while(length&&iswspace(turn->begin[length-1]))length--;
        fwprintf(out,L"%ls\n",turn->speaker);
        if(length<=1300)fwprintf(out,L"%.*ls\n",(int)length,turn->begin);
        else fwprintf(out,L"%.780ls\n［中间内容已缩略］\n%.*ls\n",turn->begin,500,turn->begin+length-500);
    }
    fwprintf(out,L"</近期对话>\n");
}

static void chat_prompt_latest(FILE *out,int compact){
    const wchar_t *latest=smart_latest_request[0]?smart_latest_request:smart_goal;
    chat_prompt_conversation(out,conversation,latest,compact);
    chat_prompt_task_evidence(out);
    fwprintf(out,L"\n解析最新消息中的「刚才那个」「继续」「按刚才方案」「改成」等承接表达时，"
        L"优先看最近一次相关问答与实际 Creo 回执。已给出的对象、数值和约束直接沿用；"
        L"新要求覆盖同一项旧条件；先核对当前 Creo 目标。有更近的客观回执时以它为准。"
        L"只有最新消息明确要求或确认执行时才操作模型，已核对完成的步骤不要重做。\n");
    if(smart_prior_goal[0])
        fwprintf(out,L"\n<在等用户补充的任务>\n%ls\n</在等用户补充的任务>\n",smart_prior_goal);
    else if(smart_steps&&wcscmp(latest,smart_goal))
        fwprintf(out,L"\n<当前继续执行的任务>\n%ls\n</当前继续执行的任务>\n",smart_goal);
    fwprintf(out,L"\n<最新用户消息>\n%ls\n</最新用户消息>\n",latest);
}

static int chat_intent_needs_review(int mode){
    if(mode!=SMART_INTENT_READ&&mode!=SMART_INTENT_CHAT)return 0;
    if(smart_explicit_readonly(smart_latest_request))return 0;
    static const wchar_t *const corrections[]={L"我叫你画",L"叫你画一个",L"我是让你执行",L"我叫你执行"};
    return smart_explicit_modeling_request(smart_latest_request)||
        smart_followup_execution_request(smart_latest_request)||
        smart_has_text(smart_latest_request,corrections,_countof(corrections));
}
