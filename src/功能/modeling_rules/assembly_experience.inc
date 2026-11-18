/* Explicit owner-requested experience notes, shared by managed BWTL profiles. */
#define EXPERIENCE_TEXT_MAX 12000
static wchar_t experience_last_reply[EXPERIENCE_TEXT_MAX];
static int experience_managing;

static void experience_capture(const wchar_t *role,const wchar_t *text){
    if(experience_managing||wcscmp(role,L"助手")&&wcscmp(role,L"AI"))return;
    if(wcslen(text)>=_countof(experience_last_reply)){experience_last_reply[0]=0;return;}
    wcscpy_s(experience_last_reply,_countof(experience_last_reply),text);
}

static HANDLE experience_directory(wchar_t path[MAX_PATH],int create){
    path[0]=0;
    wchar_t root[MAX_PATH],rules[MAX_PATH];
    HANDLE parent=modeling_rule_open_root(root);
    if(parent==INVALID_HANDLE_VALUE)return parent;
    *wcsrchr(root,L'\\')=0;
    HANDLE handles[2]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE};
    const wchar_t *folders[]={L"建模规则",L"装配经验"};
    for(int i=0;i<2;i++){
        if(swprintf_s(rules,MAX_PATH,L"%ls\\%ls",root,folders[i])<0)break;
        if(create&&!CreateDirectoryW(rules,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)break;
        handles[i]=CreateFileW(rules,FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
            NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        BY_HANDLE_FILE_INFORMATION info;
        if(handles[i]==INVALID_HANDLE_VALUE||!modeling_rule_file_matches(handles[i],rules,1,&info))break;
        wcscpy_s(root,MAX_PATH,rules);
        if(i==1)wcscpy_s(path,MAX_PATH,rules);
    }
    HANDLE result=path[0]?handles[1]:INVALID_HANDLE_VALUE;
    if(handles[0]!=INVALID_HANDLE_VALUE)CloseHandle(handles[0]);
    if(handles[1]!=INVALID_HANDLE_VALUE&&result==INVALID_HANDLE_VALUE)CloseHandle(handles[1]);
    CloseHandle(parent);
    return result;
}

static int experience_read(const wchar_t *path,wchar_t *text,size_t capacity){
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    BY_HANDLE_FILE_INFORMATION info;
    char *bytes=NULL;
    int ok=0;
    if(modeling_rule_file_matches(file,path,0,&info)&&!info.nFileSizeHigh&&info.nFileSizeLow&&
       info.nFileSizeLow<EXPERIENCE_TEXT_MAX*4){
        bytes=malloc(info.nFileSizeLow);
        DWORD read;
        if(bytes&&ReadFile(file,bytes,info.nFileSizeLow,&read,NULL)&&read==info.nFileSizeLow){
            int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes,read,NULL,0);
            if(length>0&&(size_t)length<capacity&&
               MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes,read,text,length)==length){
                text[length]=0;
                ok=1;
            }
        }
    }
    free(bytes);
    CloseHandle(file);
    return ok;
}

static int experience_save(const wchar_t *content,wchar_t path[MAX_PATH]){
    wchar_t directory[MAX_PATH]=L"";
    HANDLE folder=experience_directory(directory,1);
    if(folder==INVALID_HANDLE_VALUE)return 0;
    SYSTEMTIME now;
    GetLocalTime(&now);
    static LONG serial;
    int ok=swprintf_s(path,MAX_PATH,L"%ls\\%04u%02u%02u-%02u%02u%02u-%lu-%ld.txt",directory,
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
        GetCurrentProcessId(),InterlockedIncrement(&serial))>=0;
    wchar_t *document=calloc(EXPERIENCE_TEXT_MAX,sizeof(wchar_t));
    if(!document)ok=0;
    if(ok)ok=swprintf_s(document,EXPERIENCE_TEXT_MAX,
        L"BWTL 装配经验规则\n保存时间 %04u-%02u-%02u %02u:%02u:%02u\n"
        L"来源为用户明确保存的本次对话内容。用于后续设计参考，具体模型、尺寸和定位参照在执行时核对。\n\n%ls",
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,content)>=0;
    int size=ok?WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,document,-1,NULL,0,NULL,NULL):0;
    char *bytes=size>0?malloc(size):NULL;
    wchar_t pending[MAX_PATH];
    int pending_valid=swprintf_s(pending,MAX_PATH,L"%ls.pending",path)>=0;
    HANDLE file=INVALID_HANDLE_VALUE;
    if(pending_valid&&bytes&&WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,document,-1,bytes,size,NULL,NULL)==size)
        file=CreateFileW(pending,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    DWORD written;
    ok=file!=INVALID_HANDLE_VALUE&&WriteFile(file,bytes,size-1,&written,NULL)&&
        written==(DWORD)size-1&&FlushFileBuffers(file);
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    if(ok){
        wchar_t *readback=calloc(EXPERIENCE_TEXT_MAX,sizeof(wchar_t));
        ok=readback&&experience_read(pending,readback,EXPERIENCE_TEXT_MAX)&&!wcscmp(readback,document);
        free(readback);
    }
    if(ok)ok=MoveFileExW(pending,path,MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok&&file!=INVALID_HANDLE_VALUE)DeleteFileW(pending);
    free(bytes);
    free(document);
    CloseHandle(folder);
    return ok;
}

static int experience_recent(wchar_t paths[12][MAX_PATH],wchar_t directory[MAX_PATH],unsigned *total){
    *total=0;
    HANDLE folder=experience_directory(directory,0);
    if(folder==INVALID_HANDLE_VALUE)return 0;
    wchar_t pattern[MAX_PATH];
    int count=0;
    if(swprintf_s(pattern,MAX_PATH,L"%ls\\*.txt",directory)<0){CloseHandle(folder);return 0;}
    WIN32_FIND_DATAW entry;
    HANDLE find=FindFirstFileW(pattern,&entry);
    if(find!=INVALID_HANDLE_VALUE){
        do{
            if(entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
            wchar_t path[MAX_PATH];
            if(swprintf_s(path,MAX_PATH,L"%ls\\%ls",directory,entry.cFileName)<0)continue;
            (*total)++;
            int at=0;
            while(at<count&&_wcsicmp(paths[at],path)>0)at++;
            if(at==12)continue;
            if(count<12)count++;
            for(int j=count-1;j>at;j--)wcscpy_s(paths[j],MAX_PATH,paths[j-1]);
            wcscpy_s(paths[at],MAX_PATH,path);
        }while(FindNextFileW(find,&entry));
        FindClose(find);
    }
    CloseHandle(folder);
    return count;
}

static int experience_command(const wchar_t *text,const wchar_t **body){
    static const wchar_t *phrases[]={
        L"把装配经验写入建模规则",L"把这个装配经验写入建模规则",L"把这条装配经验写入建模规则",
        L"将装配经验写入建模规则",L"保存装配经验",L"写入装配经验",
        L"把这个写入到BWTL建模规则",L"对这个写入到BWTL建模规则"
    };
    wchar_t compact[6001];
    size_t n=0;
    for(const wchar_t *p=model_request_action_start(text);*p;p++){
        if(iswspace(*p))continue;
        if(n+1==_countof(compact))return 0;
        compact[n++]=*p;
    }
    compact[n]=0;
    for(size_t i=0;i<_countof(phrases);i++){
        const wchar_t *tail=compact;
        if(!assembly_take(&tail,phrases[i]))continue;
        if(!assembly_take(&tail,L"里面去")&&!assembly_take(&tail,L"里面"))assembly_take(&tail,L"里");
        if(*tail==L'：'||*tail==L':'){
            const wchar_t *colon=wcschr(text,L'：');
            if(!colon)colon=wcschr(text,L':');
            *body=colon?colon+1:NULL;
            return 1;
        }
        while(*tail&&wcschr(L"。！! ",*tail))tail++;
        if(!*tail){*body=NULL;return 1;}
    }
    return 0;
}

static int experience_chat(const wchar_t *text){
    const wchar_t *body=NULL;
    int view=!wcscmp(text,L"查看装配经验规则")||!wcscmp(text,L"查看装配经验");
    if(!view&&!experience_command(text,&body))return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务结束再保存装配经验。");return 1;}
    wchar_t reply[1800]=L"",path[MAX_PATH]=L"";
    if(view){
        wchar_t paths[12][MAX_PATH],directory[MAX_PATH]=L"";
        unsigned total;
        int count=experience_recent(paths,directory,&total);
        swprintf_s(reply,_countof(reply),L"已保存 %u 条装配经验规则。%ls%ls",total,
            count?L"\n目录：":L"",count?directory:L"");
        if(count){
            wchar_t *record=calloc(EXPERIENCE_TEXT_MAX,sizeof(wchar_t));
            if(record&&experience_read(paths[0],record,EXPERIENCE_TEXT_MAX)){
                size_t used=wcslen(reply);
                swprintf_s(reply+used,_countof(reply)-used,L"\n最近文件：%ls\n",paths[0]);
                used=wcslen(reply);
                wcsncpy_s(reply+used,_countof(reply)-used,record,_TRUNCATE);
            }
            free(record);
        }
    }else{
        if(body)while(iswspace(*body))body++;
        else body=experience_last_reply;
        if(!body||!*body||!wcsstr(body,L"装配")||wcslen(body)>10000){
            wcscpy_s(reply,_countof(reply),
                L"请补充要保存的装配经验，例如「把装配经验写入建模规则：螺母柱装配时先核对孔轴和安装面，再建立同轴与贴合定位」。");
        }else if(experience_save(body,path)){
            if(body!=experience_last_reply)wcscpy_s(experience_last_reply,_countof(experience_last_reply),body);
            swprintf_s(reply,_countof(reply),L"装配经验已保存到 E 盘并读回核对成功。\n文件：%ls\n后续装配任务会读取已保存的经验，并按实际模型核对参照。",path);
        }else wcscpy_s(reply,_countof(reply),L"装配经验保存或读回核对失败。请检查 E 盘建模规则目录权限；本次未确认保存成功。");
    }
    experience_managing=1;
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    append_card(L"AI",reply);
    save_turn(L"助手",reply);
    experience_managing=0;
    return 1;
}

static void experience_prompt_write(FILE *prompt,const wchar_t *request){
    if(!wcsstr(request,L"装配")&&!wcsstr(request,L"装上")&&!wcsstr(request,L"螺母柱")&&!wcsstr(request,L"紧固件"))return;
    wchar_t paths[12][MAX_PATH],directory[MAX_PATH]=L"";
    unsigned total;
    int count=experience_recent(paths,directory,&total);
    wchar_t *record=calloc(EXPERIENCE_TEXT_MAX,sizeof(wchar_t));
    if(!record)return;
    for(int i=0;i<count;i++)if(experience_read(paths[i],record,EXPERIENCE_TEXT_MAX)){
        fwprintf(prompt,L"\n<用户保存的装配经验 文件=\"%ls\">\n%.*ls\n</用户保存的装配经验>\n",
            paths[i],1600,record);
    }
    free(record);
}
