
#define AX_RESUME_LIMIT 32

typedef struct {
    wchar_t source[PRO_NAME_SIZE],target[PRO_NAME_SIZE],workspace[MAX_PATH];
    char stamp[240],pending_stamp[240];
    FILETIME learned;
    int size,done,pending;
    AxStudyTask task[AX_RESUME_LIMIT];
    int ids[AX_RESUME_LIMIT];
} AxResume;

static int ax_resume_path(const wchar_t *job,wchar_t file[MAX_PATH],int create){
    wchar_t dir[MAX_PATH];
    return or_safe_id(job)&&storage_method_directory(dir,L"装配续做",create)&&
        swprintf_s(file,MAX_PATH,L"%ls\\%ls.tsv",dir,job)>=0;
}

static int ax_resume_write(const wchar_t *path,const AxResume *state,int create){
    wchar_t tmp[MAX_PATH];
    FILE *file=ax_study_atomic(path,tmp);
    if(!file)return 0;
    fwprintf(file,L"BWTL-ASSEMBLY-RESUME-1\n%ls\n%ls %ls %hs %hs %lu %lu %d %d %d\n",state->workspace,
        state->source,state->target,state->stamp,*state->pending_stamp?state->pending_stamp:"-",
        state->learned.dwHighDateTime,state->learned.dwLowDateTime,state->size,state->done,state->pending);
    for(int i=0;i<state->size;i++){
        const AxStudyTask *row=&state->task[i];
        fwprintf(file,L"%ls\t%ls\t%ls\t%d\n",row->key,row->model,*row->base?row->base:L"-",state->ids[i]);
    }
    if(!create)return ax_study_commit(file,tmp,path);
    int good=!ferror(file);
    if(fclose(file))good=0;
    if(good&&MoveFileExW(tmp,path,MOVEFILE_WRITE_THROUGH))return 1;
    DeleteFileW(tmp);
    return 0;
}

static int ax_resume_read(const wchar_t *path,AxResume *state){
    FILE *file=ax_learning_open(path);
    if(!file)return 0;
    wchar_t line[1030],stamp[240],pending[240],from[OR_CELL],target[OR_CELL];
    int good=fgetws(line,_countof(line),file)&&!wcsncmp(line,L"BWTL-ASSEMBLY-RESUME-1",22)&&
        fgetws(state->workspace,_countof(state->workspace),file)&&
        wcschr(state->workspace,L'\n')&&fgetws(line,_countof(line),file)&&
        swscanf_s(line,L"%255ls %255ls %239ls %239ls %lu %lu %d %d %d",from,(unsigned)_countof(from),
            target,(unsigned)_countof(target),stamp,(unsigned)_countof(stamp),pending,(unsigned)_countof(pending),
            &state->learned.dwHighDateTime,&state->learned.dwLowDateTime,&state->size,&state->done,&state->pending)==9;
    if(good)good=ax_study_key(from)&&ax_study_key(target)&&state->size>0&&state->size<=AX_RESUME_LIMIT&&
        state->done>=0&&state->done<=state->size&&state->pending>=0&&(!state->pending||state->done<state->size)&&
        WideCharToMultiByte(CP_UTF8,0,stamp,-1,state->stamp,_countof(state->stamp),NULL,NULL)&&
        (!wcscmp(pending,L"-")||WideCharToMultiByte(CP_UTF8,0,pending,-1,state->pending_stamp,_countof(state->pending_stamp),NULL,NULL));
    if(good){
        state->workspace[wcscspn(state->workspace,L"\r\n")]=0;
        wcscpy_s(state->source,_countof(state->source),from);
        wcscpy_s(state->target,_countof(state->target),target);
    }
    for(int i=0;good&&i<state->size;i++){
        OrRow row={0};
        AxStudyTask *item=&state->task[i];
        ProIdTable ids={0};
        wchar_t *end;
        good=fgetws(line,_countof(line),file)&&or_columns(line,&row,4)&&
            ax_path_parse(row.value[0],ids)>0&&ax_study_key(row.value[1])&&wcslen(row.value[2])<AX_STUDY_PATH;
        if(!good)break;
        long id=wcstol(row.value[3],&end,10);
        good=!*end&&id>=0&&id<=INT_MAX&&(!id==(i>=state->done));
        if(!good)break;
        wcscpy_s(item->key,_countof(item->key),row.value[0]);
        wcscpy_s(item->model,_countof(item->model),row.value[1]);
        if(wcscmp(row.value[2],L"-"))wcscpy_s(item->base,_countof(item->base),row.value[2]);
        state->ids[i]=(int)id;
    }
    if(good)good=!fgetws(line,_countof(line),file)&&!ferror(file);
    fclose(file);
    return good;
}

static ProError ax_resume_context(ProMdl assembly,wchar_t workspace[MAX_PATH],ProMdlName name,PartText *out){
    wchar_t file[MAX_PATH];
    ProMdlType type;
    ProBoolean changed=PRO_B_TRUE;
    if(!assembly)return PRO_TK_BAD_CONTEXT;
    ProError error=ProMdlTypeGet(assembly,&type);
    if(error||type!=PRO_MDL_ASSEMBLY)return error?error:PRO_TK_BAD_CONTEXT;
    if((error=or_location(assembly,workspace,file,name,out)))return error;
    wchar_t *basename=wcsrchr(file,L'\\');
    if(!basename)return PRO_TK_BAD_CONTEXT;
    *basename=0;
    if(_wcsicmp(file,workspace)){
        part_text_add(out,L"续装需要以目标装配所在文件夹作为 Creo 工作目录。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    error=ProMdlModificationVerify(assembly,&changed);
    if(!error&&changed){part_text_add(out,L"请先保存目标装配，再建立或继续装配任务。\n");return PRO_TK_BAD_CONTEXT;}
    return error;
}

static HANDLE ax_resume_lock(const wchar_t *directory,const wchar_t *name){
    wchar_t file[MAX_PATH];
    if(swprintf_s(file,_countof(file),L"%ls\\.%ls.bwtl-resume.lock",directory,name)<0)return INVALID_HANDLE_VALUE;
    return CreateFileW(file,GENERIC_READ|GENERIC_WRITE|DELETE,0,NULL,OPEN_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
}

static ProError ax_resume_begin(ProMdl assembly,const wchar_t *job,const wchar_t *source,const wchar_t *plan,PartText *out){
    wchar_t path[MAX_PATH];
    if(!ax_study_key(source)||!ax_resume_path(job,path,1))return PRO_TK_BAD_INPUTS;
    AxResume *state=calloc(1,sizeof(*state));
    if(!state)return PRO_TK_OUT_OF_MEMORY;
    ProError error=ax_resume_context(assembly,state->workspace,state->target,out);
    HANDLE guard=INVALID_HANDLE_VALUE;
    if(!error)guard=ax_resume_lock(state->workspace,state->target);
    if(!error&&guard==INVALID_HANDLE_VALUE)error=PRO_TK_E_IN_USE;
    if(!error&&(!_wcsicmp(source,state->target)||!ax_study_case_stamp(source,&state->learned)))error=PRO_TK_BAD_CONTEXT;
    if(!error&&GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)error=PRO_TK_E_FOUND;
    if(!error)error=ax_study_verstamp(assembly,state->stamp);

    OrTable rows={0};
    if(!error&&(!or_table_load(&rows,plan,3,out)||rows.size>AX_RESUME_LIMIT))error=PRO_TK_BAD_INPUTS;
    for(int i=0;!error&&i<rows.size;i++){
        const OrRow *r=&rows.rows[i];
        AxStudyTask *task=&state->task[i];
        AxStudyRule rule;
        if(!ax_flow_key(source,r->value[0],task->key,out)||!ax_study_key(r->value[1])||
           wcslen(r->value[2])>=AX_STUDY_PATH){error=PRO_TK_BAD_INPUTS;break;}
        error=ax_study_read(source,task->key,&rule);
        if(error)break;
        if(!rule.count||rule.count!=rule.reported||rule.under){error=PRO_TK_INCOMPLETE;break;}
        for(int j=0;j<i;j++)if(!wcscmp(task->key,state->task[j].key)){error=PRO_TK_E_FOUND;break;}
        wcscpy_s(task->model,_countof(task->model),r->value[1]);
        if(wcscmp(r->value[2],L"-")){
            AxSupportBinding map[AX_STUDY_ROWS];
            int count;
            if(!error)error=ax_support_bindings(r->value[2],&rule,map,&count);
            wcscpy_s(task->base,_countof(task->base),r->value[2]);
        }
        if(error)part_text_add(out,L"续装计划第 %u 行无法采用该组件或支撑条件。\n",r->line);
    }
    if(!error){
        state->size=rows.size;
        wcscpy_s(state->source,_countof(state->source),source);
        if(!ax_resume_write(path,state,1))error=PRO_TK_CANT_WRITE;
        else part_text_add(out,L"已登记 %d 步。续装任务绑定 %ls，位置 %ls。\n",state->size,state->target,path);
    }
    or_free(&rows);
    if(guard!=INVALID_HANDLE_VALUE)CloseHandle(guard);
    free(state);
    return error;
}

static ProError ax_resume_completed(ProMdl assembly,const AxStudyTask *task,int id){
    AxStudyReceipt observed={0};
    ProError error=ax_study_inspect(assembly,id,&observed);
    if(!error&&_wcsicmp(observed.child,task->model))error=PRO_TK_BAD_CONTEXT;
    return error;
}

static ProError ax_resume_step(ProMdl assembly,AxResume *state,const wchar_t *file,PartText *out){
    AxStudyTask *task=&state->task[state->done];
    AxStudyRule rule;
    ProError error=ax_study_read(state->source,task->key,&rule);
    ProMdl child=NULL;
    if(!error)error=ax_study_find_child(assembly,&rule,task->model,&child,out);
    ProBoolean changed=PRO_B_TRUE;
    if(!error)error=ProMdlModificationVerify(child,&changed);
    if(!error&&changed){part_text_add(out,L"%ls 有未保存修改。先保存组件后再续装。\n",task->model);return PRO_TK_BAD_CONTEXT;}
    if(!error)error=ax_order_saved_dependencies(assembly,out);
    if(!error&&rule.type==PRO_MDL_ASSEMBLY)error=ax_order_saved_dependencies(child,out);
    if(error)return error;
    int id=0;
    error=ax_study_place(assembly,child,&rule,task->base,&id,out);
    if(!error)error=ax_resume_completed(assembly,task,id);
    if(!error)error=ax_study_verstamp(assembly,state->pending_stamp);
    if(error)return error;
    state->pending=id;
    if(!ax_resume_write(file,state,0))return PRO_TK_CANT_WRITE;
    error=ProMdlSave(assembly);
    if(!error)error=ax_study_verstamp(assembly,state->stamp);
    if(error)return error;
    state->ids[state->done++]=id;
    state->pending=0;
    state->pending_stamp[0]=0;
    if(!ax_resume_write(file,state,0))return PRO_TK_CANT_WRITE;
    part_text_add(out,L"第 %d 步保存并核对完成，组件 %ls，ID:%d。\n",state->done,task->model,id);
    return PRO_TK_NO_ERROR;
}

static ProError ax_resume_run(ProMdl assembly,const wchar_t *job,int steps,PartText *out){
    wchar_t path[MAX_PATH],work[MAX_PATH];
    ProMdlName target;
    if(steps<0||steps>AX_RESUME_LIMIT||!ax_resume_path(job,path,0))return PRO_TK_BAD_INPUTS;
    ProError error=ax_resume_context(assembly,work,target,out);
    if(error)return error;
    HANDLE guard=ax_resume_lock(work,target);
    if(guard==INVALID_HANDLE_VALUE)return PRO_TK_E_IN_USE;
    AxResume *state=calloc(1,sizeof(*state));
    if(!state){CloseHandle(guard);return PRO_TK_OUT_OF_MEMORY;}
    char stamp[240];
    FILETIME learned;
    if(!ax_resume_read(path,state)||_wcsicmp(work,state->workspace)||_wcsicmp(target,state->target)||
       !ax_study_case_stamp(state->source,&learned)||CompareFileTime(&learned,&state->learned))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ax_study_verstamp(assembly,stamp);
    if(!error&&strcmp(stamp,state->stamp)){
        if(!state->pending||strcmp(stamp,state->pending_stamp)||
           ax_resume_completed(assembly,&state->task[state->done],state->pending))error=PRO_TK_BAD_CONTEXT;
        else{
            state->ids[state->done++]=state->pending;
            strcpy_s(state->stamp,_countof(state->stamp),stamp);
        }
    }

    if(!error&&state->pending){
        state->pending=0;
        state->pending_stamp[0]=0;
        if(!ax_resume_write(path,state,0))error=PRO_TK_CANT_WRITE;
    }
    for(int i=0;!error&&i<state->done;i++)error=ax_resume_completed(assembly,&state->task[i],state->ids[i]);
    if(error)part_text_add(out,L"续装记录与当前工作目录、模型版本、案例或已装实例不一致；模型保留现状。\n");
    for(int i=0;!error&&i<steps&&state->done<state->size;i++){
        part_text_add(out,L"续做第 %d 步 %ls。\n",state->done+1,state->task[state->done].model);
        error=ax_resume_step(assembly,state,path,out);
    }
    if(state->size)part_text_add(out,L"已保存 %d/%d 步，待续装 %d 步。\n",state->done,state->size,state->size-state->done);
    CloseHandle(guard);
    free(state);
    return error;
}

