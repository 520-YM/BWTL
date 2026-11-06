static UINT pending_workdir_action;

static void native_workdir_action(UINT action){
    PostMessageW(native_backend_window,BWTL_WORKDIR_REQUEST,action,0);
}

static void queue_workdir_command(UINT action){
    if(action<WORKDIR_VIEW||action>WORKDIR_PICK||pending_workdir_action||native_busy)return;
    pending_workdir_action=action;
    if(ProMacroLoad(L"~ Command `BWTLWorkingDirectory`;")){
        pending_workdir_action=0;
        append(L"无法调度工作目录操作。");
    }
}

static int copy_workdir_path(const wchar_t *path){
    size_t bytes=(wcslen(path)+1)*sizeof(*path);
    HGLOBAL handle=GlobalAlloc(GMEM_MOVEABLE,bytes);
    void *data;
    int copied=0;
    if(!handle)return 0;
    data=GlobalLock(handle);
    if(data){
        memcpy(data,path,bytes);
        GlobalUnlock(handle);
        if(OpenClipboard(native_window)){
            copied=EmptyClipboard()&&SetClipboardData(CF_UNICODETEXT,handle)!=NULL;
            CloseClipboard();
        }
    }
    if(!copied)GlobalFree(handle);
    return copied;
}

static int working_directory_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProPath current,selected;
    ProName label=L"选择 Creo 工作目录";
    wchar_t message[PRO_PATH_SIZE+100];
    UINT action=pending_workdir_action;
    ProError error;
    pending_workdir_action=0;
    if(!action||native_busy||smart_active||ai_process||development_process)return 0;
    error=ProDirectoryCurrentGet(current);
    if(error){append(L"无法读取当前 Creo 工作目录。");return 0;}
    if(action==WORKDIR_OPEN){
        if((INT_PTR)ShellExecuteW(native_window,L"open",current,NULL,NULL,SW_SHOWNORMAL)>32)return 0;
        swprintf_s(message,_countof(message),L"无法在资源管理器打开工作目录：\n%ls",current);
    }else if(action==WORKDIR_COPY){
        swprintf_s(message,_countof(message),L"%ls\n%ls",copy_workdir_path(current)?L"已复制工作目录路径。":L"剪贴板暂不可用，当前工作目录为：",current);
    }else if(action==WORKDIR_PICK){
        native_set_busy(2);
        error=ProDirectoryChoose(label,NULL,NULL,current,selected);
        if(!error){
            error=ProDirectoryChange(selected);
            if(!error){
                /* A pending continuation or snapshot belongs to the old directory. */
                smart_waiting_for_user=0;
                pending_plan.ready=0;
                memset(&pending_part,0,sizeof(pending_part));
                part_snapshot_clear(&request_snapshot);
                foundation_reset();
                error=ProDirectoryCurrentGet(current);
                swprintf_s(message,_countof(message),L"%ls\n%ls",error?L"Creo 已接受目录切换，当前路径读回失败。选择的目录为：":L"Creo 当前工作目录已切换为：",error?selected:current);
            }else swprintf_s(message,_countof(message),L"无法切换工作目录（Creo 错误码 %d）：\n%ls",error,selected);
        }else if(error!=PRO_TK_USER_ABORT)swprintf_s(message,_countof(message),L"无法打开 Creo 目录选择器（错误码 %d）。",error);
        native_set_busy(0);
        if(error==PRO_TK_USER_ABORT)return 0;
    }else swprintf_s(message,_countof(message),L"Creo 当前工作目录：\n%ls",current);
    append(message);
    return 0;
}
