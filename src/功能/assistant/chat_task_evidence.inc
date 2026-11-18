
/* Only validated native write receipts belong here. Keep them separate from
   read/report turns and the current task's retry budget. */
typedef struct {
    wchar_t goal[1800],action[550],receipt[1800];
    int step,error,unsafe;
} ChatTaskOutcome;

static struct {
    ProMdl model,window;
    ProMdlName name;
    ProPath directory;
    ChatTaskOutcome outcomes[2];
    size_t count;
} chat_task_evidence;

static void chat_task_forget(void){
    memset(&chat_task_evidence,0,sizeof(chat_task_evidence));
}

static int chat_task_matches(const PartSnapshot *snapshot){
    return snapshot->model&&snapshot->window_model&&snapshot->stamp&&snapshot->directory[0]&&
        snapshot->model==chat_task_evidence.model&&snapshot->window_model==chat_task_evidence.window&&
        !_wcsicmp(snapshot->name,chat_task_evidence.name)&&
        !_wcsicmp(snapshot->directory,chat_task_evidence.directory);
}

static void chat_task_record_bound(ProMdl model,ProMdl window,const wchar_t *name,
    const wchar_t *directory,const wchar_t *action,ProError error,const wchar_t *receipt,int unsafe,int step){
    if(!model||!window||!name[0]||!directory[0])return;
    if(model!=chat_task_evidence.model||window!=chat_task_evidence.window||
        _wcsicmp(name,chat_task_evidence.name)||_wcsicmp(directory,chat_task_evidence.directory)){
        chat_task_forget();
        chat_task_evidence.model=model;
        chat_task_evidence.window=window;
        wcscpy_s(chat_task_evidence.name,_countof(chat_task_evidence.name),name);
        wcscpy_s(chat_task_evidence.directory,_countof(chat_task_evidence.directory),directory);
    }
    size_t count=chat_task_evidence.count;
    ChatTaskOutcome *rows=chat_task_evidence.outcomes;
    if(!count||wcsncmp(rows[count-1].goal,smart_goal,_countof(rows[0].goal)-1)){
        if(count==_countof(chat_task_evidence.outcomes)){rows[0]=rows[1];count--;}
        chat_task_evidence.count=++count;
    }
    ChatTaskOutcome *outcome=&rows[count-1];
    wcsncpy_s(outcome->goal,_countof(outcome->goal),smart_goal,_TRUNCATE);
    wcsncpy_s(outcome->action,_countof(outcome->action),action,_TRUNCATE);
    wcsncpy_s(outcome->receipt,_countof(outcome->receipt),receipt,_TRUNCATE);
    outcome->error=error;
    outcome->step=step;
    outcome->unsafe=unsafe;
}

static void chat_task_record(const PartCommand *cmd,const wchar_t *action,ProError error,
    const wchar_t *receipt,int unsafe){
    const PartSnapshot *snapshot=&request_snapshot;
    if(!cmd||smart_read_command(cmd)||!snapshot->stamp||_wcsicmp(cmd->arg[1],snapshot->name))return;
    chat_task_record_bound(snapshot->model,snapshot->window_model,snapshot->name,snapshot->directory,
        action,error,receipt,unsafe,smart_steps);
}

static void chat_prompt_task_evidence(FILE *out){
    if(smart_has_details||!chat_task_evidence.count||!chat_task_matches(&request_snapshot))return;
    fwprintf(out,L"\n<同一 Creo 模型的近期建模任务回执，按时间顺序，历史数据>\n模型 %ls\n",
        chat_task_evidence.name);
    for(size_t i=0;i<chat_task_evidence.count;i++){
        const ChatTaskOutcome *row=&chat_task_evidence.outcomes[i];
        fwprintf(out,L"该轮用户任务：%ls\n最后实际执行：第 %d 步，%ls，Creo %d。%ls\n%ls\n",
            row->goal,row->step,row->action,row->error,
            row->unsafe?L"该轮曾因状态核对停止，须以本轮重新读取的模型为准。":L"",row->receipt);
    }
    fwprintf(out,L"这些记录用于衔接用户所说的对象、参数和已完成步骤；只有最新消息明确要求动模型时才执行。优先采用本轮实测的参照和回执。\n</同一 Creo 模型的近期建模任务回执>\n");
}
