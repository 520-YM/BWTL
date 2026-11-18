typedef struct FastenerSavedView {
    struct FastenerSavedView *next;
    ProMdl model;
    ProWVerstamp stamp;
    BY_HANDLE_FILE_INFORMATION source;
    wchar_t path[MAX_PATH],stage[MAX_PATH];
} FastenerSavedView;
static FastenerSavedView *fastener_saved_views;
static wchar_t fastener_last_stage[MAX_PATH];

static FastenerSavedView *fastener_saved_model(ProMdl model){
    ProMdlName name;
    ProMdlType type;
    ProMdl actual=NULL;
    ProWVerstamp stamp=NULL;
    if(ProMdlMdlnameGet(model,name)||ProMdlTypeGet(model,&type)||ProMdlnameInit(name,type,&actual)||
       actual!=model||ProMdlVerstampGet(model,&stamp))return NULL;
    FastenerSavedView *found=NULL;
    for(FastenerSavedView *entry=fastener_saved_views;entry;entry=entry->next)
        if(entry->model==model&&ProVerstampEqual(entry->stamp,stamp)){found=entry;break;}
    if(stamp)ProVerstampFree(&stamp);
    return found;
}

static int fastener_saved_load(const wchar_t *path,HANDLE source,ProMdl *model){
    BY_HANDLE_FILE_INFORMATION info;
    if(!GetFileInformationByHandle(source,&info))return 0;
    for(FastenerSavedView *entry=fastener_saved_views;entry;entry=entry->next){
        if(_wcsicmp(entry->path,path)||!company_source_matches(&info,&entry->source))continue;
        ProMdlName name;
        ProMdl current=NULL;
        ProMdlType type;
        /* Resolve by the recorded filename before using an older model handle. */
        const wchar_t *leaf=wcsrchr(path,L'\\')+1,*suffix=native_file_suffix(leaf);
        if(!suffix||wcsncpy_s(name,PRO_NAME_SIZE,leaf,suffix-leaf))return 0;
        type=native_file_kind(suffix)==ATTACH_ASSEMBLY?PRO_MDL_ASSEMBLY:PRO_MDL_PART;
        if(ProMdlnameInit(name,type,&current)||current!=entry->model||fastener_saved_model(current)!=entry)return 0;
        *model=current;
        return 1;
    }
    return 0;
}

static void fastener_saved_record(const wchar_t *path,HANDLE source,ProMdl model){
    FastenerSavedView *entry=fastener_saved_model(model);
    if(entry)return;
    entry=calloc(1,sizeof(*entry));
    if(!entry)return;
    if(ProMdlVerstampGet(model,&entry->stamp)||!GetFileInformationByHandle(source,&entry->source)){
        if(entry->stamp)ProVerstampFree(&entry->stamp);
        free(entry);return;
    }
    entry->model=model;
    wcscpy_s(entry->path,MAX_PATH,path);
    wcscpy_s(entry->stage,MAX_PATH,fastener_last_stage);
    entry->next=fastener_saved_views;
    fastener_saved_views=entry;
}

static void fastener_saved_clear(void){
    while(fastener_saved_views){
        FastenerSavedView *entry=fastener_saved_views;
        fastener_saved_views=entry->next;
        if(entry->stamp)ProVerstampFree(&entry->stamp);
        free(entry);
    }
}

static int fastener_equal_files(const wchar_t *first,const wchar_t *second){
    HANDLE a=CreateFileW(first,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    HANDLE b=CreateFileW(second,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    int same=0;
    BY_HANDLE_FILE_INFORMATION ia,ib;
    if(a!=INVALID_HANDLE_VALUE&&b!=INVALID_HANDLE_VALUE&&
       modeling_rule_file_matches(a,first,0,&ia)&&modeling_rule_file_matches(b,second,0,&ib)&&
       ia.nFileSizeHigh==ib.nFileSizeHigh&&ia.nFileSizeLow==ib.nFileSizeLow){
        char left[16384],right[16384];
        DWORD na,nb;
        same=1;
        do{
            if(!ReadFile(a,left,sizeof(left),&na,NULL)||!ReadFile(b,right,sizeof(right),&nb,NULL)||
               na!=nb||memcmp(left,right,na)){same=0;break;}
        }while(na);
    }
    if(a!=INVALID_HANDLE_VALUE)CloseHandle(a);
    if(b!=INVALID_HANDLE_VALUE)CloseHandle(b);
    return same;
}

static ProError fastener_save_project(ProMdl model,const wchar_t *input_directory,wchar_t saved[MAX_PATH],PartText *out){
    wchar_t directory[MAX_PATH];
    DWORD length=GetFullPathNameW(input_directory,MAX_PATH,directory,NULL);
    if(!length||length>=MAX_PATH)return PRO_TK_BAD_INPUTS;
    company_trim_path(directory);
    if(!storage_output_directory(directory))return PRO_TK_BAD_CONTEXT;
    static unsigned serial;
    wchar_t stage[MAX_PATH],pattern[MAX_PATH],check[MAX_PATH];
    ProError error=PRO_TK_NO_ERROR;
    FastenerSavedView *known=fastener_saved_model(model);
    if(known)wcscpy_s(stage,MAX_PATH,known->stage);
    else {
        if(swprintf_s(stage,MAX_PATH,L"%ls\\fastener-save-%u",ai_dir,++serial)<0||
           !storage_output_directory(ai_dir)||!CreateDirectoryW(stage,NULL))return PRO_TK_CANT_WRITE;
        error=ProDirectoryChange(stage);
        if(error)return error;
        error=ProMdlnameBackup(model,L"");
        ProError restored=ProDirectoryChange(directory);
        if(restored)return restored;
        if(error)return error;
    }
    wcscpy_s(fastener_last_stage,MAX_PATH,stage);
    if(ProDirectoryCurrentGet(check)||(company_trim_path(check),_wcsicmp(directory,check)))return PRO_TK_BAD_CONTEXT;
    swprintf_s(pattern,MAX_PATH,L"%ls\\*",stage);
    WIN32_FIND_DATAW entry;
    HANDLE find=FindFirstFileW(pattern,&entry);
    if(find==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    /* Validate every generated file before publishing a component package. */
    do{
        if(!wcscmp(entry.cFileName,L".")||!wcscmp(entry.cFileName,L".."))continue;
        if(entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)){error=PRO_TK_BAD_CONTEXT;break;}
        wchar_t from[MAX_PATH],to[MAX_PATH];
        if(swprintf_s(from,MAX_PATH,L"%ls\\%ls",stage,entry.cFileName)<0||
           swprintf_s(to,MAX_PATH,L"%ls\\%ls",directory,entry.cFileName)<0){error=PRO_TK_BAD_INPUTS;break;}
        if(GetFileAttributesW(to)!=INVALID_FILE_ATTRIBUTES&&!fastener_equal_files(from,to)){
            part_text_add(out,L"当前工作目录已有不同内容的文件 %ls。请改用其他工作目录，或先确定现有文件的处理方式。",to);
            error=PRO_TK_E_FOUND;break;
        }
    }while(FindNextFileW(find,&entry));
    FindClose(find);
    if(error)return error;
    ProMdlName name;
    ProMdlType type;
    if((error=ProMdlMdlnameGet(model,name))||(error=ProMdlTypeGet(model,&type)))return error;
    find=FindFirstFileW(pattern,&entry);
    if(find==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    saved[0]=0;
    do{
        if(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        wchar_t from[MAX_PATH],to[MAX_PATH];
        if(swprintf_s(from,MAX_PATH,L"%ls\\%ls",stage,entry.cFileName)<0||
           swprintf_s(to,MAX_PATH,L"%ls\\%ls",directory,entry.cFileName)<0){error=PRO_TK_BAD_INPUTS;break;}
        if(!CopyFileW(from,to,TRUE)&&!fastener_equal_files(from,to)){
            part_text_add(out,L"写入 %ls 未完成，已写入的文件保留在工作目录，可重新点击继续核对。",to);
            error=PRO_TK_CANT_WRITE;break;
        }
        if(!fastener_equal_files(from,to)){error=PRO_TK_CANT_WRITE;break;}
        size_t n=wcslen(name);
        if(!_wcsnicmp(entry.cFileName,name,n)&&
           !_wcsnicmp(entry.cFileName+n,type==PRO_MDL_ASSEMBLY?L".asm":L".prt",4))
            wcscpy_s(saved,MAX_PATH,to);
    }while(FindNextFileW(find,&entry));
    FindClose(find);
    if(!error&&!saved[0])error=PRO_TK_CANT_OPEN;
    return error;
}

#define BWTL_FASTENER_MENU_REQUEST (WM_APP+73)
static wchar_t *fastener_menu_pending;
static int fastener_chat(const wchar_t *text);

static int fastener_menu_command(uiCmdCmdId id,uiCmdValue *value,void *data){
    wchar_t *path=fastener_menu_pending;
    fastener_menu_pending=NULL;
    if(!path)return 0;
    wchar_t request[MAX_PATH+48];
    if(swprintf_s(request,_countof(request),L"打开并保存紧固件模型「%ls」",path)>=0)fastener_chat(request);
    free(path);
    return 0;
}

static void fastener_menu_receive(wchar_t *path){
    if(!path)return;
    if(native_busy||smart_active||ai_process||fastener_menu_pending){free(path);append(L"请等待当前任务完成后再打开紧固件。");return;}
    fastener_menu_pending=path;
    if(ProMacroLoad(L"~ Command \x0060" L"BWTLFastenerMenu\x0060;")){
        free(fastener_menu_pending);
        fastener_menu_pending=NULL;
        append(L"Creo 未接受紧固件打开请求，请重新点击。");
    }
}
