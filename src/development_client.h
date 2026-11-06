#ifndef BWTL_DEVELOPMENT_CLIENT_H
#define BWTL_DEVELOPMENT_CLIENT_H
static HANDLE development_process,development_job;
static int development_is_apply;
static int development_exit_notice;
static wchar_t development_last_status[6000];
static wchar_t development_previous_status[6000],development_previous_job[MAX_PATH];
static int development_read(const wchar_t *path,wchar_t *out,size_t cap){
    FILE *f=_wfopen(path,L"rt, ccs=UTF-8");size_t n=0;wint_t c;out[0]=0;if(!f)return 0;
    while(n+1<cap&&(c=fgetwc(f))!=WEOF)out[n++]=(wchar_t)c;out[n]=0;fclose(f);return 1;
}
static void development_paths(wchar_t root[MAX_PATH],wchar_t script[MAX_PATH]){
    wchar_t *p;wcscpy_s(root,MAX_PATH,runtime_data);p=wcsrchr(root,L'\\');if(p)*p=0;
    swprintf_s(script,MAX_PATH,L"%ls\\scripts\\Start-BWTLDevelopment.ps1",runtime_root);
}
static int development_job_path(wchar_t path[MAX_PATH]){
    wchar_t root[MAX_PATH],script[MAX_PATH],latest[MAX_PATH],id[100];size_t i;
    development_paths(root,script);
    swprintf_s(latest,MAX_PATH,L"%ls\\development\\latest-job.txt",root);
    if(!development_read(latest,id,100)||!id[0])return 0;
    for(i=0;id[i];i++)if(!iswalnum(id[i])&&id[i]!=L'-')return 0;
    return swprintf_s(path,MAX_PATH,L"%ls\\development\\%ls",root,id)>0;
}
static int development_job_status(const wchar_t *job,wchar_t text[6000]){
    wchar_t path[MAX_PATH];
    return swprintf_s(path,MAX_PATH,L"%ls\\ui-status.txt",job)>0&&development_read(path,text,6000);
}
static const wchar_t *development_status_body(const wchar_t *text){
    const wchar_t *body=wcschr(text,L'\n');
    return body?body+1:text;
}
static int development_launch(const wchar_t *action){
    wchar_t root[MAX_PATH],script[MAX_PATH],exe[MAX_PATH],command[2048],request[MAX_PATH],images[MAX_PATH];
    STARTUPINFOW startup={sizeof(startup)};PROCESS_INFORMATION process={0};JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0};
    if(development_process){append(L"已有开发或升级任务在运行，可先停止或等待完成。");return 0;}
    development_previous_job[0]=development_previous_status[0]=0;
    if(development_job_path(development_previous_job))development_job_status(development_previous_job,development_previous_status);
    development_paths(root,script);GetSystemDirectoryW(exe,MAX_PATH);wcscat_s(exe,MAX_PATH,L"\\WindowsPowerShell\\v1.0\\powershell.exe");
    swprintf_s(request,MAX_PATH,L"%ls\\development-request.txt",runtime_data);swprintf_s(images,MAX_PATH,L"%ls\\development-images.txt",runtime_data);
    swprintf_s(command,2048,L"\"%ls\" -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%ls\" -InstallRoot \"%ls\" -Action %ls -RequestFile \"%ls\" -ImageListFile \"%ls\"",exe,script,root,action,request,images);
    startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    development_job=CreateJobObjectW(NULL,NULL);limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!development_job||!SetInformationJobObject(development_job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))goto fail;
    if(!CreateProcessW(exe,command,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,runtime_root,&startup,&process))goto fail;
    if(!AssignProcessToJobObject(development_job,process.hProcess)){TerminateProcess(process.hProcess,1);CloseHandle(process.hThread);CloseHandle(process.hProcess);goto fail;}
    development_process=process.hProcess;development_is_apply=!wcscmp(action,L"Apply");development_exit_notice=0;
    ResumeThread(process.hThread);CloseHandle(process.hThread);
    development_last_status[0]=0;native_set_busy(1);native_schedule_poll();return 1;
fail:if(development_job){CloseHandle(development_job);development_job=NULL;}append(L"独立开发任务未能启动，请检查安装脚本和日志。");native_set_busy(0);return 0;
}
static void development_start(const wchar_t *value){
    wchar_t request[MAX_PATH],images[MAX_PATH];NativeImage attached[4];int i,n;FILE *f;
    if(ai_process||smart_active||development_process){append(L"请先停止或完成当前任务。");return;}
    if(wcslen(value)>6000){append(L"开发需求请控制在6000字以内。");return;}
    swprintf_s(request,MAX_PATH,L"%ls\\development-request.txt",runtime_data);f=_wfopen(request,L"wt, ccs=UTF-8");if(!f){append(L"无法保存开发需求。");return;}fwprintf(f,L"%ls",value);fclose(f);
    swprintf_s(images,MAX_PATH,L"%ls\\development-images.txt",runtime_data);f=_wfopen(images,L"wt, ccs=UTF-8");if(!f){append(L"无法保存开发附件列表。");return;}
    n=native_images_snapshot(attached);for(i=0;i<n;i++)fwprintf(f,L"%ls\n",attached[i].path);fclose(f);
    append_card(L"你",value);save_turn(L"用户",value);native_prompt_set(L"");
    development_launch(L"Start");
}
static void development_apply(void){
    if(ai_process||smart_active){append(L"请先停止当前分析或建模任务。");return;}
    development_launch(L"Apply");
}
static int development_poll(void){
    wchar_t job[MAX_PATH]=L"",text[6000];DWORD code=1;
    int running,current_job;
    if(!development_process)return 0;
    running=WaitForSingleObject(development_process,0)==WAIT_TIMEOUT;
    current_job=development_job_path(job)&&(development_is_apply||wcscmp(job,development_previous_job));
    if(current_job&&development_job_status(job,text)&&
        (wcscmp(job,development_previous_job)||wcscmp(text,development_previous_status))){
        wcscpy_s(development_last_status,6000,text);
        if(!wcsncmp(text,L"installing",10)||!wcsncmp(text,L"installed",9))native_set_busy(2);
        if(running&&development_is_apply&&!development_exit_notice&&!wcsncmp(text,L"waiting-exit",12)){
            development_exit_notice=1;
            append_card(L"AI",development_status_body(text));
        }
    }
    if(running){
        native_schedule_poll();return 1;
    }
    GetExitCodeProcess(development_process,&code);CloseHandle(development_process);development_process=NULL;
    if(development_job){CloseHandle(development_job);development_job=NULL;}native_set_busy(0);
    {
        wchar_t answer[6000],path[MAX_PATH];static wchar_t result[12000];
        const wchar_t *state=development_last_status;
        const wchar_t *body=development_status_body(state);
        int no_changes=!wcsncmp(state,L"no-changes",10);
        if(code&&wcsncmp(state,L"failed",6)&&wcsncmp(state,L"installed-waiting-startup",25))
            body=L"开发或升级未完成。请查看本机 development 目录中的任务日志。";
        if(!*body)body=L"开发流程未返回结果，请查看任务日志。";
        wcsncpy_s(result,12000,body,_TRUNCATE);
        if(!code&&!development_is_apply&&current_job&&
            (no_changes||!wcsncmp(state,L"ready",5)||!wcsncmp(state,L"await-native",12))&&
            swprintf_s(path,MAX_PATH,L"%ls\\answer.md",job)>0&&development_read(path,answer,6000)&&answer[0]){
            if(no_changes)wcscpy_s(result,12000,answer);
            else swprintf_s(result,12000,L"%ls\n\n%ls",answer,body);
        }
        append_card(L"AI",result);save_turn(L"助手",result);
    }
    return 1;
}
static void development_cancel(void){
    if(development_job){CloseHandle(development_job);development_job=NULL;}
    if(development_process){TerminateProcess(development_process,1);CloseHandle(development_process);development_process=NULL;}
}
static void development_on_exit(void){
    if(development_is_apply&&development_process){
        /* The user saved and exited Creo. Keep the already requested trusted
           installer alive so it can finish after this DLL is unloaded. */
        if(development_job){JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0};SetInformationJobObject(development_job,JobObjectExtendedLimitInformation,&limits,sizeof(limits));CloseHandle(development_job);development_job=NULL;}
        CloseHandle(development_process);development_process=NULL;
    }else development_cancel();
}
#endif
