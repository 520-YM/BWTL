#include "../workspace_projects/project_copy.inc"

static UINT pending_workdir_action;
static wchar_t *pending_project_name;

static void native_workdir_action(UINT action,wchar_t *project_name){
    if(!PostMessageW(native_backend_window,BWTL_WORKDIR_REQUEST,action,(LPARAM)project_name))free(project_name);
}

static void queue_workdir_command(UINT action,wchar_t *project_name){
    if(action<WORKDIR_VIEW||action>WORKDIR_CREATE||pending_workdir_action||native_busy){
        free(project_name);
        return;
    }
    pending_workdir_action=action;
    pending_project_name=project_name;
    if(ProMacroLoad(L"~ Command \x0060" L"BWTLWorkingDirectory\x0060;")){
        pending_workdir_action=0;
        pending_project_name=NULL;
        free(project_name);
        append(L"无法调度工作目录操作。");
    }
}

static void workdir_clear_old_context(void){
    /* A pending continuation or snapshot belongs to the old directory. */
    smart_waiting_for_user=0;
    pending_plan.ready=0;
    memset(&pending_part,0,sizeof(pending_part));
    part_snapshot_clear(&request_snapshot);
    foundation_reset();
}

static void project_create_workdir(const wchar_t *root,const wchar_t *name,wchar_t message[PRO_PATH_SIZE+100]){
    wchar_t workdir[MAX_PATH],details[512];
    const size_t capacity=PRO_PATH_SIZE+100;
    if(project_copy_create(root,name,workdir,details)!=ERROR_SUCCESS){
        wcsncpy_s(message,capacity,details,_TRUNCATE);
        return;
    }
    if(!storage_output_directory(workdir)){
        swprintf_s(message,capacity,L"新项目已复制。Creo 无法使用此工作目录：\n%ls",workdir);
        return;
    }
    ProError error=ProDirectoryChange(workdir);
    if(error){
        swprintf_s(message,capacity,L"新项目已复制。Creo 切换工作目录失败（%d）：\n%ls",error,workdir);
        return;
    }
    workdir_clear_old_context();
    ProPath current;
    if(ProDirectoryCurrentGet(current)){
        swprintf_s(message,capacity,L"新项目已创建。Creo 已接受切换，路径读回失败：\n%ls",workdir);
        return;
    }
    swprintf_s(message,capacity,L"新项目已创建，Creo 当前工作目录：\n%ls",current);
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
    wchar_t *project_name=pending_project_name;
    pending_project_name=NULL;
    if(!action||native_busy||smart_active||ai_process){free(project_name);return 0;}
    if(action==WORKDIR_CREATE){
        if(project_name){
            native_set_busy(2);
            ProPath root;
            swprintf_s(root,_countof(root),L"%lc:\\project",storage_data_drive());
            project_create_workdir(root,project_name,message);
            native_set_busy(0);
            append(message);
        }
        free(project_name);
        return 0;
    }
    free(project_name);
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
        if(!error&&!storage_output_directory(selected)){
            native_set_busy(0);
            append(storage_output_message);
            return 0;
        }
        if(!error){
            error=ProDirectoryChange(selected);
            if(!error){
                workdir_clear_old_context();
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
