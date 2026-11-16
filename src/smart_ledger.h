#ifndef BWTL_SMART_LEDGER_H
#define BWTL_SMART_LEDGER_H
#define SMART_PROTOCOL_CAP (PART_ARG_MAX*PART_TOKEN_MAX+1)
typedef struct {
    wchar_t key[SMART_PROTOCOL_CAP];
    int error,generation,read;
} SmartAttempt;
static SmartAttempt smart_attempts[SMART_MAX_STEPS];
static int smart_attempt_count,smart_generation;

static void smart_protocol_key(const PartCommand *cmd,int failed,wchar_t key[SMART_PROTOCOL_CAP]){
    size_t used=0;
    for(int i=0;i<cmd->count;i++){
        if(i==2&&cmd->verb==PART_FOUNDATION){
            if(pn_kind(cmd->arg[0]))continue;
            if(failed&&!wcscmp(cmd->arg[0],L"SKETCH_CREATE"))continue;
        }
        const wchar_t *token=cmd->arg[i];
        for(;*token;token++)key[used++]=i<2?towupper(*token):*token;
        key[used++]=0x1f; /* Tokens cannot contain this separator. */
    }
    key[used]=0;
}

static int smart_protocol_seen(const PartCommand *cmd){
    wchar_t key[SMART_PROTOCOL_CAP],failed_key[SMART_PROTOCOL_CAP];
    smart_protocol_key(cmd,0,key);
    smart_protocol_key(cmd,1,failed_key);
    for(int i=0;i<smart_attempt_count;i++){
        const SmartAttempt *row=&smart_attempts[i];
        if(((!row->error&&!row->read)||row->generation==smart_generation)&&!wcscmp(row->key,row->error?failed_key:key))return 1;
    }
    return 0;
}

static void smart_protocol_remember(const PartCommand *cmd,ProError error){
    if(smart_attempt_count>=_countof(smart_attempts))return;
    SmartAttempt *row=&smart_attempts[smart_attempt_count++];
    smart_protocol_key(cmd,error!=0,row->key);
    row->error=error;
    row->read=smart_read_command(cmd);
    row->generation=smart_generation;
}

static void smart_protocol_display(const PartCommand *cmd,wchar_t *out,size_t cap){
    out[0]=0;
    for(int i=0;i<cmd->count;i++){

        size_t used=wcslen(out);
        int quoted=wcspbrk(cmd->arg[i],L" \t\"")!=NULL;
        if(used+4>=cap)break;
        if(i)out[used++]=L' ';
        if(quoted)out[used++]=L'"';
        for(const wchar_t *p=cmd->arg[i];*p&&used+4<cap;p++){
            if(quoted&&(*p==L'"'||*p==L'\\'))out[used++]=L'\\';
            out[used++]=*p;
        }
        if(quoted)out[used++]=L'"';
        out[used]=0;
    }
}
#endif
