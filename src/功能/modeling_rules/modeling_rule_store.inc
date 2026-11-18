#include <limits.h>
#include <string.h>

enum { MODELING_RULE_COMPANY=1, MODELING_RULE_INDEPENDENT };
enum {
    MODELING_RULE_OK, MODELING_RULE_UNAVAILABLE, MODELING_RULE_CHANGED,
    MODELING_RULE_INVALID, MODELING_RULE_NO_PREVIOUS, MODELING_RULE_BUSY
};
typedef struct {
    unsigned mode;
    wchar_t revision[4];
} ModelingRuleValues;
typedef struct {
    ModelingRuleValues current,previous;
    unsigned generation;
    int has_previous;
    FILETIME last_write;
} ModelingRuleState;

static const ModelingRuleState modeling_rule_default={{MODELING_RULE_COMPANY,L"V00"},{0},1,0,{0}};

static int modeling_rule_root(wchar_t directory[MAX_PATH]){
    const wchar_t *input=runtime_data;
    if(!*input||input[1]!=L':'||(input[2]!=L'\\'&&input[2]!=L'/'))return 0;
    wchar_t configured_drive=storage_output_drive();
    DWORD length=GetFullPathNameW(input,MAX_PATH,directory,NULL);
    if(length<4||length>=MAX_PATH||(configured_drive&&towupper(directory[0])!=configured_drive))return 0;
    while(length>3&&directory[length-1]==L'\\')directory[--length]=0;

    /* Managed secondary installations share this single rules document.
       Their conversation, runtime and project data still use runtime_data. */
    wchar_t managed[MAX_PATH];
    wcscpy_s(managed,MAX_PATH,directory);
    wchar_t *data=wcsrchr(managed,L'\\');
    if(!data||_wcsicmp(data+1,L"data"))return 1;
    *data=0;
    wchar_t *index=wcsrchr(managed,L'\\');
    if(!index||index[1]<L'2'||index[1]>L'8'||index[2])return 1;
    *index=0;
    wchar_t *instances=wcsrchr(managed,L'\\');
    if(!instances||_wcsicmp(instances+1,L"BWTL-Instances"))return 1;
    *instances=0;
    return swprintf_s(directory,MAX_PATH,L"%ls\\BWTL\\data",managed)>=0;
}

static int modeling_rule_path(wchar_t path[MAX_PATH]){
    wchar_t directory[MAX_PATH];
    return modeling_rule_root(directory)&&swprintf_s(path,MAX_PATH,L"%ls\\建模规则.bwtl",directory)>=0;
}

static int modeling_rule_file_matches(HANDLE file,const wchar_t *path,int directory,
        BY_HANDLE_FILE_INFORMATION *info){
    wchar_t resolved[MAX_PATH+4];
    DWORD length=GetFinalPathNameByHandleW(file,resolved,_countof(resolved),VOLUME_NAME_DOS);
    return length>=7&&length<_countof(resolved)&&!wcsncmp(resolved,L"\\\\?\\",4)&&
        !_wcsicmp(resolved+4,path)&&GetFileType(file)==FILE_TYPE_DISK&&
        GetFileInformationByHandle(file,info)&&!(info->dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&
        (!!(info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==directory)&&(directory||info->nNumberOfLinks==1);
}

static HANDLE modeling_rule_open_root(wchar_t path[MAX_PATH]){
    if(!modeling_rule_path(path))return INVALID_HANDLE_VALUE;
    wchar_t directory[MAX_PATH];
    wcscpy_s(directory,MAX_PATH,path);
    *wcsrchr(directory,L'\\')=0;
    HANDLE handle=CreateFileW(directory,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    BY_HANDLE_FILE_INFORMATION info;
    if(handle!=INVALID_HANDLE_VALUE&&!modeling_rule_file_matches(handle,directory,1,&info)){
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }
    return handle;
}

static const char *modeling_rule_mode_name(unsigned mode){
    switch(mode){
    case MODELING_RULE_COMPANY:return "company";
    case MODELING_RULE_INDEPENDENT:return "independent";
    default:return "none";
    }
}

static int modeling_rule_values_valid(const ModelingRuleValues *values){
    return values&&(values->mode==MODELING_RULE_COMPANY||values->mode==MODELING_RULE_INDEPENDENT)&&
        values->revision[0]==L'V'&&values->revision[1]>=L'0'&&values->revision[1]<=L'9'&&
        values->revision[2]>=L'0'&&values->revision[2]<=L'9'&&!values->revision[3];
}

static int modeling_rule_take(const char **text,const char *token){
    size_t length=strlen(token);
    if(strncmp(*text,token,length))return 0;
    *text+=length;
    return 1;
}

static int modeling_rule_parse_values(const char **text,ModelingRuleValues *values,int previous){
    const char *mode_label=previous?"previous.mode=":"current.mode=";
    const char *revision_label=previous?"previous.revision=":"current.revision=";
    if(!modeling_rule_take(text,mode_label))return 0;
    if(modeling_rule_take(text,"company\n"))values->mode=MODELING_RULE_COMPANY;
    else if(modeling_rule_take(text,"independent\n"))values->mode=MODELING_RULE_INDEPENDENT;
    else if(!previous||!modeling_rule_take(text,"none\n"))return 0;
    if(!modeling_rule_take(text,revision_label))return 0;
    if(!values->mode)return modeling_rule_take(text,"---\n");
    const char *p=*text;
    if(p[0]!='V'||p[1]<'0'||p[1]>'9'||p[2]<'0'||p[2]>'9'||p[3]!='\n')return 0;
    for(int i=0;i<3;i++)values->revision[i]=(wchar_t)p[i];
    *text+=4;
    return 1;
}

static int modeling_rule_parse(const char *text,ModelingRuleState *state){
    if(!modeling_rule_take(&text,"BWTL_MODELING_RULES 1\n"
                                "generation="))return 0;
    unsigned generation=0;
    while(*text>='0'&&*text<='9'){
        unsigned digit=(unsigned)(*text++-'0');
        if(generation>(UINT_MAX-digit)/10)return 0;
        generation=generation*10+digit;
    }
    if(!generation||*text!='\n')return 0;
    text++;
    ModelingRuleState parsed={0};
    parsed.generation=generation;
    if(!modeling_rule_parse_values(&text,&parsed.current,0)||
       !modeling_rule_parse_values(&text,&parsed.previous,1)||*text)return 0;
    parsed.has_previous=parsed.previous.mode!=0;
    if(parsed.has_previous!=(generation>1))return 0;
    *state=parsed;
    return 1;
}

static int modeling_rule_load(const wchar_t *path,ModelingRuleState *state,int *exists){
    *exists=0;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,
        OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE){
        if(GetLastError()!=ERROR_FILE_NOT_FOUND)return MODELING_RULE_UNAVAILABLE;
        *state=modeling_rule_default;
        return MODELING_RULE_OK;
    }
    char text[256];
    DWORD read_count;
    BY_HANDLE_FILE_INFORMATION info;
    int status=MODELING_RULE_UNAVAILABLE;
    if(modeling_rule_file_matches(file,path,0,&info)){
        if(info.nFileSizeHigh||!info.nFileSizeLow||info.nFileSizeLow>=sizeof(text))
            status=MODELING_RULE_INVALID;
        else if(ReadFile(file,text,info.nFileSizeLow,&read_count,NULL)&&read_count==info.nFileSizeLow){
            text[read_count]=0;
            status=!memchr(text,0,read_count)&&modeling_rule_parse(text,state)?
                MODELING_RULE_OK:MODELING_RULE_INVALID;
            if(status==MODELING_RULE_OK){
                state->last_write=info.ftLastWriteTime;
                *exists=1;
            }
        }
    }
    CloseHandle(file);
    return status;
}

static int modeling_rule_read(ModelingRuleState *state){
    wchar_t path[MAX_PATH];
    HANDLE directory=modeling_rule_open_root(path);
    if(directory==INVALID_HANDLE_VALUE)return MODELING_RULE_UNAVAILABLE;
    int exists,status=modeling_rule_load(path,state,&exists);
    CloseHandle(directory);
    return status;
}

static int modeling_rule_write(const wchar_t *path,ModelingRuleState *state,int exists){
    char text[256],previous_revision[]="---";
    if(state->has_previous)
        for(int i=0;i<3;i++)previous_revision[i]=(char)state->previous.revision[i];
    int length=snprintf(text,sizeof(text),"BWTL_MODELING_RULES 1\n"
        "generation=%u\ncurrent.mode=%s\ncurrent.revision=%c%c%c\n"
        "previous.mode=%s\nprevious.revision=%s\n",state->generation,
        modeling_rule_mode_name(state->current.mode),(int)state->current.revision[0],
        (int)state->current.revision[1],(int)state->current.revision[2],
        modeling_rule_mode_name(state->previous.mode),previous_revision);
    if(length<=0||length>=sizeof(text))return MODELING_RULE_INVALID;

    static LONG serial;
    wchar_t temporary[MAX_PATH];
    if(swprintf_s(temporary,MAX_PATH,L"%ls.tmp.%lu.%llu.%ld",path,GetCurrentProcessId(),
        GetTickCount64(),InterlockedIncrement(&serial))<0)return MODELING_RULE_UNAVAILABLE;
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return MODELING_RULE_UNAVAILABLE;
    DWORD written;
    int ok=WriteFile(file,text,(DWORD)length,&written,NULL)&&written==(DWORD)length&&FlushFileBuffers(file);
    CloseHandle(file);
    if(ok&&MoveFileExW(temporary,path,MOVEFILE_WRITE_THROUGH|(exists?MOVEFILE_REPLACE_EXISTING:0))){
        /* Timestamp comes from the committed file, after the writer closes. */
        int committed;
        int status=modeling_rule_load(path,state,&committed);
        return status==MODELING_RULE_OK&&!committed?MODELING_RULE_UNAVAILABLE:status;
    }
    DeleteFileW(temporary);
    return MODELING_RULE_UNAVAILABLE;
}

enum { MODELING_RULE_SEED, MODELING_RULE_SAVE, MODELING_RULE_RESTORE };
static int modeling_rule_change(int operation,const ModelingRuleValues *values,
        unsigned expected_generation,ModelingRuleState *state){
    ModelingRuleValues requested;
    if(operation==MODELING_RULE_SAVE){
        if(!modeling_rule_values_valid(values))return MODELING_RULE_INVALID;
        requested=*values;
        values=&requested;
    }
    wchar_t path[MAX_PATH],lock_path[MAX_PATH];
    HANDLE directory=modeling_rule_open_root(path);
    if(directory==INVALID_HANDLE_VALUE)return MODELING_RULE_UNAVAILABLE;
    int status=MODELING_RULE_UNAVAILABLE;
    HANDLE lock=INVALID_HANDLE_VALUE;
    if(swprintf_s(lock_path,MAX_PATH,L"%ls.lock",path)<0)goto done;
    lock=CreateFileW(lock_path,GENERIC_READ|GENERIC_WRITE,0,NULL,
        OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(lock==INVALID_HANDLE_VALUE){
        if(GetLastError()==ERROR_SHARING_VIOLATION)status=MODELING_RULE_BUSY;
        goto done;
    }
    BY_HANDLE_FILE_INFORMATION info;
    if(!modeling_rule_file_matches(lock,lock_path,0,&info))goto done;
    int exists;
    status=modeling_rule_load(path,state,&exists);
    if(status!=MODELING_RULE_OK)goto done;

    if(operation==MODELING_RULE_SEED){
        if(!exists)status=modeling_rule_write(path,state,0);
        goto done;
    }
    if(expected_generation!=state->generation){status=MODELING_RULE_CHANGED;goto done;}
    if(operation==MODELING_RULE_RESTORE){
        if(!state->has_previous){status=MODELING_RULE_NO_PREVIOUS;goto done;}
        values=&state->previous;
    }
    if(values->mode==state->current.mode&&!wcscmp(values->revision,state->current.revision)){
        if(!exists)status=modeling_rule_write(path,state,0);
        goto done;
    }
    if(state->generation==UINT_MAX){status=MODELING_RULE_INVALID;goto done;}
    ModelingRuleState next=*state;
    next.previous=state->current;
    next.current=*values;
    next.has_previous=1;
    next.generation++;
    status=modeling_rule_write(path,&next,exists);
    if(status==MODELING_RULE_OK)*state=next;
done:
    if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);
    CloseHandle(directory);
    return status;
}

static int modeling_rule_seed(void){
    ModelingRuleState state;
    return modeling_rule_change(MODELING_RULE_SEED,NULL,0,&state);
}

static int modeling_rule_save(const ModelingRuleValues *values,unsigned expected_generation,ModelingRuleState *state){
    return modeling_rule_change(MODELING_RULE_SAVE,values,expected_generation,state);
}

static int modeling_rule_restore(unsigned expected_generation,ModelingRuleState *state){
    return modeling_rule_change(MODELING_RULE_RESTORE,NULL,expected_generation,state);
}
