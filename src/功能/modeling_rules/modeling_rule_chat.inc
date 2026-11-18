/* Local owner commands. Rules never originate in model output, attachments or history. */
#define BWTL_MODELING_RULE_VIEW (WM_APP+72)

typedef struct {
    unsigned mode;
    wchar_t revision[4];
} ModelingRuleChange;

static ModelingRuleChange modeling_rule_proposal;

static const wchar_t *modeling_rule_flow(unsigned mode){
    return mode==MODELING_RULE_COMPANY?
        L"新建空装配 → 默认定位装入同图号、同版本的空钣金件 → 自动打开钣金件":
        L"默认创建独立零件；明确要求按公司流程时使用装配与同号钣金件";
}

static const wchar_t *modeling_rule_status(int status){
    switch(status){
    case MODELING_RULE_CHANGED:return L"另一个 BWTL 窗口刚更新了规则。请查看当前规则，再发送本次修改。";
    case MODELING_RULE_BUSY:return L"另一个 BWTL 窗口正在写入规则，稍后可以重发。";
    case MODELING_RULE_INVALID:return L"建模规则文件格式无效。请核对数据目录中的建模规则文件。";
    case MODELING_RULE_NO_PREVIOUS:return L"尚无上一版建模规则可恢复。";
    default:return L"无法读写建模规则。请核对本机 BWTL 数据盘和目录权限。";
    }
}

static void modeling_rule_describe(wchar_t *reply,size_t capacity){
    ModelingRuleState state;
    int status=modeling_rule_read(&state);
    if(status!=MODELING_RULE_OK){
        wcscpy_s(reply,capacity,modeling_rule_status(status));
        return;
    }
    PartText out={reply,capacity,0};
    part_text_add(&out,L"「创建零件」规则 · 第 %u 版\n%ls。\n默认图号版本 %ls，给了完整版本就沿用你给的。",
        state.generation,modeling_rule_flow(state.current.mode),state.current.revision);
    part_text_add(&out,L"\n明确说「单独创建」时创建独立零件；指定在当前装配里建子件时，以当前装配为目标。");
    if(state.has_previous)part_text_add(&out,L"\n上一版：%ls，默认图号版本 %ls。",
        state.previous.mode==MODELING_RULE_COMPANY?L"公司建模流程":L"独立零件流程",state.previous.revision);
    SYSTEMTIME local;
    FILETIME time;
    if(FileTimeToLocalFileTime(&state.last_write,&time)&&FileTimeToSystemTime(&time,&local)&&local.wYear>1601)
        part_text_add(&out,L"\n最近写入：%04u-%02u-%02u %02u:%02u。",local.wYear,local.wMonth,
            local.wDay,local.wHour,local.wMinute);
    wchar_t paths[12][MAX_PATH],experience_path[MAX_PATH]=L"";
    unsigned experience_count;
    experience_recent(paths,experience_path,&experience_count);
    part_text_add(&out,L"\n装配经验规则：%u 条。可发送「查看装配经验规则」查看。",experience_count);
    wchar_t path[MAX_PATH];
    if(modeling_rule_path(path))part_text_add(&out,L"\n规则文件：%ls",path);
}

static int modeling_rule_parse_clauses(const wchar_t *p,ModelingRuleChange *change){
    *change=(ModelingRuleChange){0};
    for(;;){
        p=modeling_part_skip(p);
        if(!*p)return change->mode||change->revision[0];
        if(!change->revision[0]&&(assembly_take(&p,L"默认图号版本")||
            assembly_take(&p,L"默认版本号")||assembly_take(&p,L"默认版本"))){
            p=modeling_part_skip(p);
            if(assembly_take(&p,L"为")||assembly_take(&p,L"是")||assembly_take(&p,L"改为"))
                p=modeling_part_skip(p);
            if(!modeling_part_revision(&p,change->revision))return 0;
        }else if(!change->mode&&assembly_take(&p,L"默认流程")){
            p=modeling_part_skip(p);
            if(assembly_take(&p,L"改为")||assembly_take(&p,L"为")||assembly_take(&p,L"是"))
                p=modeling_part_skip(p);
            if(assembly_take(&p,L"公司建模流程")||assembly_take(&p,L"公司流程")||assembly_take(&p,L"公司"))
                change->mode=MODELING_RULE_COMPANY;
            else if(assembly_take(&p,L"独立零件流程")||assembly_take(&p,L"独立零件")||assembly_take(&p,L"独立"))
                change->mode=MODELING_RULE_INDEPENDENT;
            else return 0;
        }else return 0;
    }
}

static int modeling_rule_company_sentence(const wchar_t *line){
    const wchar_t *only=wcsstr(line,L"只有");
    if(!only)only=wcsstr(line,L"只在");
    if(!only)only=wcsstr(line,L"仅当");
    if(only&&wcsstr(only,L"单独")&&wcsstr(only,L"装配")&&
        (wcsstr(only,L"创建")||wcsstr(only,L"新建")))return 1;
    return (wcsstr(line,L"先建")||wcsstr(line,L"先创建")||wcsstr(line,L"先新建"))&&wcsstr(line,L"装配")&&wcsstr(line,L"同")&&
        wcsstr(line,L"钣金")&&wcsstr(line,L"打开");
}

static int modeling_rule_reference_write(const wchar_t *text){
    static const wchar_t *const phrases[]={
        L"把这条写入BWTL",L"把这个写入BWTL",L"把这条规则写入BWTL",L"把这条写入到BWTL",
        L"把这条规则写入到BWTL",L"这个写入到BWTL",L"把这条保存到BWTL",L"保存这条建模规则",
        L"把这条写入插件",L"保存这条规则",L"把这条登记到BWTL"
    };
    for(size_t i=0;i<_countof(phrases);i++){
        const wchar_t *tail=text;
        if(!assembly_take(&tail,phrases[i]))continue;
        if(!assembly_take(&tail,L"里面"))assembly_take(&tail,L"里");
        if(!*modeling_part_skip(tail))return 1;
    }
    return 0;
}

static int modeling_rule_chat(const wchar_t *text){
    enum { RULE_CHAT_VIEW=1,RULE_CHAT_SAVE,RULE_CHAT_RESTORE,RULE_CHAT_PROPOSE };
    static const wchar_t *const writes[]={L"写入建模规则",L"更新建模规则",L"修改建模规则",L"保存建模规则"};
    wchar_t normalized[1000];
    if(wcslen(text)>=_countof(normalized)||wcspbrk(text,L"\r\n"))return 0;
    wcscpy_s(normalized,_countof(normalized),model_request_action_start(text));
    size_t len=wcslen(normalized);
    while(len&&(iswspace(normalized[len-1])||wcschr(L"。！.! ",normalized[len-1])))normalized[--len]=0;
    if(!len)return 0;
    const wchar_t *raw=normalized;
    int action=0;
    ModelingRuleChange change={0};
    if(!wcscmp(raw,L"查看建模规则")||!wcscmp(raw,L"查看生效建模规则"))action=RULE_CHAT_VIEW;
    else if(!wcscmp(raw,L"恢复上一次建模规则")||!wcscmp(raw,L"恢复上一版建模规则"))action=RULE_CHAT_RESTORE;
    else if(modeling_part_take_any(&raw,writes,_countof(writes))){
        if(!*raw||iswspace(*raw)||wcschr(L"：:",*raw)){
            raw=modeling_part_skip(raw);
            action=RULE_CHAT_SAVE;
            if(!*raw)change=modeling_rule_proposal;
            else if(!modeling_rule_parse_clauses(raw,&change))
                change=(ModelingRuleChange){.mode=modeling_rule_company_sentence(raw)?MODELING_RULE_COMPANY:0};
        }
    }else{
        wchar_t compact[1000];
        size_t n=0;
        for(const wchar_t *p=raw;*p;p++)if(!iswspace(*p))compact[n++]=*p;
        compact[n]=0;
        const wchar_t *last=compact;
        for(const wchar_t *p=compact;*p;p++)if(wcschr(L"。；;？?",*p))last=p+1;
        int typed_company=modeling_rule_company_sentence(raw);
        if(modeling_rule_reference_write(compact)||(typed_company&&modeling_rule_reference_write(last))){
            action=RULE_CHAT_SAVE;
            change=typed_company?
                (ModelingRuleChange){.mode=MODELING_RULE_COMPANY}:modeling_rule_proposal;
        }else if(typed_company&&!wcspbrk(raw,L"?？")&&
            (!wcsncmp(raw,L"以后",2)||!wcsncmp(raw,L"创建零件的规则",7)||
             !wcsncmp(raw,L"规则",2))){
            change.mode=MODELING_RULE_COMPANY;
            action=RULE_CHAT_PROPOSE;
        }
    }
    if(!action)return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    wchar_t reply[1500]=L"";
    if(action==RULE_CHAT_VIEW)modeling_rule_describe(reply,_countof(reply));
    else if(action==RULE_CHAT_PROPOSE){
        modeling_rule_proposal=change;
        wcscpy_s(reply,_countof(reply),L"已识别你的公司建模流程。发送「把这条写入 BWTL」即可长期保存。");
    }else if(action==RULE_CHAT_SAVE&&!change.mode&&!change.revision[0]){
        wcscpy_s(reply,_countof(reply),
            L"请写明要更新哪项，例如「写入建模规则：默认流程 公司」或「写入建模规则：默认版本 V07」。");
    }else{
        ModelingRuleState state;
        int status=modeling_rule_read(&state);
        if(status==MODELING_RULE_OK){
            if(action==RULE_CHAT_RESTORE)status=modeling_rule_restore(state.generation,&state);
            else {
                ModelingRuleValues updated=state.current;
                if(change.mode)updated.mode=change.mode;
                if(change.revision[0])wcscpy_s(updated.revision,_countof(updated.revision),change.revision);
                status=modeling_rule_save(&updated,state.generation,&state);
            }
        }
        if(status!=MODELING_RULE_OK)wcscpy_s(reply,_countof(reply),modeling_rule_status(status));
        else{
            modeling_rule_proposal=(ModelingRuleChange){0};
            swprintf_s(reply,_countof(reply),L"%ls建模规则第 %u 版。%ls。默认图号版本 %ls。",
                action==RULE_CHAT_RESTORE?L"已恢复上一版并生成":L"已写入",state.generation,
                modeling_rule_flow(state.current.mode),state.current.revision);
        }
    }
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    append_card(L"AI",reply);
    save_turn(L"助手",reply);
    return 1;
}

static void modeling_rule_prompt_write(FILE *prompt){
    ModelingRuleState state;
    if(modeling_rule_read(&state)!=MODELING_RULE_OK)return;
    fwprintf(prompt,L"\n<BWTL生效建模规则>\n规则第 %u 版。空白新零件默认：%ls。默认图号版本 %ls。"
        L"明确说单独创建按独立件处理；明确指定在当前装配里建子件时绑定该装配。"
        L"写入、修改和恢复规则由 BWTL 本机聊天命令执行，禁止在对话中假称保存成功。\n</BWTL生效建模规则>\n",
        state.generation,modeling_rule_flow(state.current.mode),state.current.revision);
}
