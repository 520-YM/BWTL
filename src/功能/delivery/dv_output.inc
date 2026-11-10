typedef struct {
    ProPath parent,stage,target;
    int owned,in_stage;
} DeliveryOutput;

static int dv_file_nonempty(const wchar_t *path){
    WIN32_FILE_ATTRIBUTE_DATA data;
    return GetFileAttributesExW(path,GetFileExInfoStandard,&data)&&
        !(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
        (data.nFileSizeHigh||data.nFileSizeLow);
}

static ProError dv_stage_begin(const wchar_t *leaf,DeliveryOutput *output){
    static LONG serial;
    ProError error=ProDirectoryCurrentGet(output->parent);
    if(error)return error;
    if(!dv_safe_leaf(leaf)||swprintf_s(output->target,PRO_PATH_SIZE,L"%ls\\%ls",output->parent,leaf)<0)
        return PRO_TK_BAD_INPUTS;
    if(GetFileAttributesW(output->target)!=INVALID_FILE_ATTRIBUTES)return PRO_TK_E_FOUND;
    if(GetLastError()!=ERROR_FILE_NOT_FOUND)return PRO_TK_CANT_WRITE;
    if(swprintf_s(output->stage,PRO_PATH_SIZE,L"%ls\\bwtl_stage_%lx_%llx_%lx",
        output->parent,GetCurrentProcessId(),GetTickCount64(),InterlockedIncrement(&serial))<0)return PRO_TK_BAD_INPUTS;
    if(!CreateDirectoryW(output->stage,NULL))return PRO_TK_CANT_WRITE;
    output->owned=1;
    return PRO_TK_NO_ERROR;
}

static ProError dv_stage_enter(DeliveryOutput *output){
    ProError error=ProDirectoryChange(output->stage);
    if(!error)output->in_stage=1;
    return error;
}

/* Only a newly created, owned, flat staging directory is cleaned. */
static int dv_stage_clean(DeliveryOutput *output){
    ProPath pattern,path;
    WIN32_FIND_DATAW entry;
    DWORD attributes=GetFileAttributesW(output->stage);
    if(!output->owned||attributes==INVALID_FILE_ATTRIBUTES||
       !(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return 0;
    if(swprintf_s(pattern,PRO_PATH_SIZE,L"%ls\\*",output->stage)<0)return 0;
    HANDLE find=FindFirstFileW(pattern,&entry);
    int clean=1;
    if(find!=INVALID_HANDLE_VALUE){
        do{
            if(!wcscmp(entry.cFileName,L".")||!wcscmp(entry.cFileName,L".."))continue;
            if((entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
               swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",output->stage,entry.cFileName)<0||
               !DeleteFileW(path))clean=0;
        }while(FindNextFileW(find,&entry));
        FindClose(find);
    }
    return clean&&RemoveDirectoryW(output->stage);
}

static ProError dv_stage_finish(DeliveryOutput *output,ProError error,PartText *out){
    if(!output->owned)return error;
    if(output->in_stage){
        ProError restore=ProDirectoryChange(output->parent);
        if(restore){
            dv_recovery_uncertain=1;
            part_text_add(out,L"工作目录恢复未能核实，临时输出位于 %ls。\n",output->stage);
            return restore;
        }
        output->in_stage=0;
    }
    if(!error){
        if(MoveFileExW(output->stage,output->target,MOVEFILE_WRITE_THROUGH)){
            output->owned=0;
            return PRO_TK_NO_ERROR;
        }
        error=PRO_TK_CANT_WRITE;
    }
    if(!dv_stage_clean(output))part_text_add(out,L"临时文件保留在 %ls，交付目录未发布。\n",output->stage);
    output->owned=0;
    return error;
}

static int dv_csv_text(FILE *file,const wchar_t *value){
    if(fputwc(L'"',file)==WEOF)return 0;
    if(*value&&wcschr(L"=+-@\t\r",*value)&&fputwc(L'\'',file)==WEOF)return 0;
    for(;*value;value++){
        if(*value==L'"'&&fputwc(L'"',file)==WEOF)return 0;
        if(fputwc(*value,file)==WEOF)return 0;
    }
    return fputwc(L'"',file)!=WEOF;
}
