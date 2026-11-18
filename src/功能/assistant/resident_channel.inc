/* The poll timer owns this channel. Creo Toolkit calls stay on Creo's
   command thread; the worker only speaks JSON-RPC to Codex. */
#define BWTL_RESIDENT_CHANNEL 1
#define RESIDENT_OPERATION_MAX 14010
static HANDLE resident_process,resident_job;
static unsigned long resident_sequence;
static int resident_reset_thread,resident_tool_queued,resident_tool_inflight,resident_used_tool;
static wchar_t resident_operation[RESIDENT_OPERATION_MAX];

static int resident_path(wchar_t path[MAX_PATH],const wchar_t *name){
    return swprintf_s(path,MAX_PATH,L"%ls\\%ls",ai_dir,name)>=0;
}

static int resident_write(const wchar_t *name,const wchar_t *text){
    wchar_t path[MAX_PATH],temp[MAX_PATH];
    if(!resident_path(path,name)||swprintf_s(temp,MAX_PATH,L"%ls.native-pending",path)<0)return 0;
    int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);
    if(!bytes)return 0;
    char *encoded=malloc((size_t)bytes);
    if(!encoded)return 0;
    int ok=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,encoded,bytes,NULL,NULL)==bytes;
    FILE *stream=ok?_wfopen(temp,L"wb"):NULL;
    if(stream){
        ok=fwrite(encoded,1,(size_t)bytes-1,stream)==(size_t)bytes-1;
        if(fclose(stream))ok=0;
        if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        if(!ok)DeleteFileW(temp);
    }else ok=0;
    free(encoded);
    return ok;
}

static int resident_read(const wchar_t *name,wchar_t *text,int capacity){
    wchar_t path[MAX_PATH];
    if(!resident_path(path,name))return -1;
    FILE *stream=_wfopen(path,L"rb");
    if(!stream)return 0;
    size_t maximum=(size_t)capacity*4;
    char *bytes=malloc(maximum);
    if(!bytes){fclose(stream);return -1;}
    size_t count=fread(bytes,1,maximum,stream);
    int ok=!ferror(stream)&&count<maximum&&(!count||!memchr(bytes,0,count));
    fclose(stream);
    if(ok){
        int length=count?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes,(int)count,text,capacity-1):0;
        ok=length>0||count==0;
        if(ok)text[length]=0;
    }
    free(bytes);
    return ok?1:-1;
}

static void resident_remove(const wchar_t *name){
    wchar_t path[MAX_PATH];
    if(resident_path(path,name))DeleteFileW(path);
}

static void resident_shutdown(void){
    if(resident_process&&WaitForSingleObject(resident_process,0)==WAIT_TIMEOUT){
        resident_write(L"resident-stop.txt",L"1");
        WaitForSingleObject(resident_process,1500);
    }
    if(resident_job){CloseHandle(resident_job);resident_job=NULL;}
    if(resident_process){
        if(WaitForSingleObject(resident_process,0)==WAIT_TIMEOUT)TerminateProcess(resident_process,1);
        CloseHandle(resident_process);
        resident_process=NULL;
    }
}

static int resident_connect(void){
    if(resident_process&&WaitForSingleObject(resident_process,0)==WAIT_TIMEOUT)return 1;
    resident_shutdown();
    resident_remove(L"resident-stop.txt");
    wchar_t codex[MAX_PATH],script[MAX_PATH],powershell[MAX_PATH],command[MAX_PATH*4+150];
    if(!runtime_codex(codex)||
       swprintf_s(script,MAX_PATH,L"%ls\\scripts\\Start-BWTLCodexSession.ps1",runtime_root)<0||
       GetFileAttributesW(script)==INVALID_FILE_ATTRIBUTES)return 0;
    UINT length=GetSystemDirectoryW(powershell,MAX_PATH);
    if(!length||length>=MAX_PATH||
       wcscat_s(powershell,MAX_PATH,L"\\WindowsPowerShell\\v1.0\\powershell.exe")||
       swprintf_s(command,_countof(command),L"\"%ls\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"%ls\" -Exchange \"%ls\" -Codex \"%ls\" -OwnerPid %lu",
            powershell,script,ai_dir,codex,GetCurrentProcessId())<0)return 0;
    STARTUPINFOW startup={sizeof(startup)};
    PROCESS_INFORMATION process={0};
    if(!CreateProcessW(powershell,command,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,ai_dir,&startup,&process))
        return 0;
    resident_process=process.hProcess;
    resident_job=CreateJobObjectW(NULL,NULL);
    if(resident_job){
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION policy={0};
        policy.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(resident_job,JobObjectExtendedLimitInformation,&policy,sizeof(policy))||
           !AssignProcessToJobObject(resident_job,resident_process)){
            CloseHandle(resident_job);
            resident_job=NULL;
        }
    }
    if(ResumeThread(process.hThread)==(DWORD)-1){CloseHandle(process.hThread);resident_shutdown();return 0;}
    CloseHandle(process.hThread);
    trace("Codex App Server worker connected",(int)process.dwProcessId);
    return 1;
}

static int resident_submit(const wchar_t *prompt,int routing,const NativeImage *images,int count,const NativeFile *files,int file_count){
    wchar_t path[MAX_PATH],copy[MAX_PATH],name[64],header[64+NATIVE_IMAGE_MAX*(MAX_PATH+1)+NATIVE_ATTACHMENT_MAX*(MAX_PATH+32)];
    if(!storage_output_directory(ai_dir)||!resident_path(path,L"resident-request.txt")||
       GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES||!resident_connect())return 0;
    resident_sequence++;
    if(!resident_sequence)resident_sequence++;
    swprintf_s(name,_countof(name),L"prompt-%lu.txt",resident_sequence);
    if(!resident_path(copy,name)||!CopyFileW(prompt,copy,FALSE))return 0;
    swprintf_s(header,_countof(header),L"%lu\n%d\n%d",resident_sequence,routing,resident_reset_thread);
    for(int i=0;i<count;i++){
        if(wcscat_s(header,_countof(header),L"\n")||
           wcscat_s(header,_countof(header),images[i].path))return 0;
    }
    for(int i=0;i<file_count;i++){
        const wchar_t *kind=files[i].kind==ATTACH_PDF?L"PDF":files[i].kind==ATTACH_TEXT?L"TEXT":NULL;
        if(!kind)continue;
        wchar_t row[MAX_PATH+32];
        if(swprintf_s(row,_countof(row),L"\n%ls|%d|%ls",kind,i+1,files[i].path)<0||
           wcscat_s(header,_countof(header),row)){
            DeleteFileW(copy);
            return 0;
        }
    }
    if(!resident_write(L"resident-request.txt",header)){
        DeleteFileW(copy);
        return 0;
    }
    resident_reset_thread=resident_used_tool=0;
    ai_process=resident_process;
    return 1;
}

static int resident_frame(wchar_t *frame,wchar_t **body){
    wchar_t *end;
    if(*frame<L'0'||*frame>L'9')return 0;
    unsigned long got=wcstoul(frame,&end,10);
    if(*end!=L'\n'||got!=resident_sequence)return 0;
    *body=end+1;
    return 1;
}

/* 0 waits, 1 has an answer, -1 lost connection, -2 failed turn. */
static int resident_answer(wchar_t *answer,int capacity){
    wchar_t frame[64],*body;
    if(resident_read(L"resident-result.txt",frame,_countof(frame))==1&&resident_frame(frame,&body)){
        resident_remove(L"resident-result.txt");
        ai_process=NULL;
        if(wcscmp(body,L"0"))return -2;
        return resident_read(L"answer.txt",answer,capacity)==1&&answer[0]?1:-2;
    }
    if(!resident_process||WaitForSingleObject(resident_process,0)!=WAIT_TIMEOUT){
        ai_process=NULL;
        resident_remove(L"resident-request.txt");
        resident_shutdown();
        return -1;
    }
    return 0;
}

static int resident_take_tool(void){
    if(resident_tool_queued||resident_tool_inflight)return 0;
    static wchar_t frame[RESIDENT_OPERATION_MAX+32];
    if(resident_read(L"resident-tool.txt",frame,_countof(frame))!=1)return 0;
    resident_remove(L"resident-tool.txt");
    wchar_t *body;
    if(!resident_frame(frame,&body)||!body[0]||wcslen(body)>=_countof(resident_operation))return 0;
    wcscpy_s(resident_operation,_countof(resident_operation),body);
    resident_tool_queued=1;
    return 1;
}

static void resident_tool_result(int code,const wchar_t *result){
    if(!resident_tool_inflight)return;
    resident_tool_inflight=0;
    size_t capacity=wcslen(result)+64;
    wchar_t *frame=malloc(capacity*sizeof(*frame));
    if(frame){
        if(swprintf_s(frame,capacity,L"%lu\n%d\n%ls",resident_sequence,code,result)>=0)
            resident_write(L"resident-tool-result.txt",frame);
        free(frame);
    }
}

static int resident_tool_running(void){return resident_tool_inflight;}
