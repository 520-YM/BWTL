static ProError imitation_standard_directory(ProPath directory)
{
    ProPath root,standards;
    ProError error=imitation_library_paths(root,standards,directory);
    if(!error&&(!imitation_library_is_directory(root)||!imitation_library_is_directory(standards)||
                !imitation_library_is_directory(directory)))error=PRO_TK_E_NOT_FOUND;
    return error;
}

static ProError imitation_standard_verify(const wchar_t *input,ProPath path)
{
    ProPath directory,receipt;
    ProError error=imitation_standard_directory(directory);
    if(error)return error;
    wchar_t *leaf=NULL;
    const DWORD length=GetFullPathNameW(input,PRO_PATH_SIZE,path,&leaf);
    if(!length||length>=PRO_PATH_SIZE||!leaf)return PRO_TK_BAD_INPUTS;
    const size_t dir_length=wcslen(directory);
    if(leaf-path!=dir_length+1||path[dir_length]!=L'\\'||_wcsnicmp(path,directory,dir_length))
        return PRO_TK_BAD_CONTEXT;
    BY_HANDLE_FILE_INFORMATION model;
    if(!imitation_file_info(path,&model))return PRO_TK_CANT_OPEN;
    if(swprintf_s(receipt,_countof(receipt),L"%ls.验收.txt",path)<0)return PRO_TK_BAD_INPUTS;
    HANDLE file=CreateFileW(receipt,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE)return PRO_TK_E_NOT_FOUND;
    BY_HANDLE_FILE_INFORMATION acceptance;
    char text[1024];
    DWORD count=0;
    const int read=GetFileInformationByHandle(file,&acceptance)&&
        !(acceptance.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
        CompareFileTime(&model.ftLastWriteTime,&acceptance.ftLastWriteTime)<=0&&
        ReadFile(file,text,sizeof(text)-1,&count,NULL);
    CloseHandle(file);
    if(!read)return PRO_TK_CANT_OPEN;
    text[count]=0;
    const char prefix[]="BWTL 仿照验收\n源模型 ";
    const char verified[]="\n尺寸试改 已通过并恢复原值\n";
    if(strncmp(text,prefix,sizeof(prefix)-1))return PRO_TK_INCOMPLETE;
    const char *status=strchr(text+sizeof(prefix)-1,'\n');
    if(!status||strncmp(status,verified,sizeof(verified)-1))return PRO_TK_INCOMPLETE;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_open_standard(const wchar_t *input,ProMdl *model,PartText *out)
{
    ProPath path;
    ProError error=imitation_standard_verify(input,path);
    if(error){
        part_text_add(out,L"请从公司标准件目录选择已通过尺寸试改的零件。验收记录需与该零件版本对应。\n");
        return error;
    }
    int opened=0;
    error=imitation_load_source(path,model,&opened);
    if(!error){
        ProPath actual;
        error=ProMdlOriginGet(*model,actual);
        if(!error&&_wcsicmp(actual,path))error=PRO_TK_BAD_CONTEXT;
    }
    if(!error&&!opened){
        ProBoolean modified;
        error=ProMdlModificationVerify(*model,&modified);
        if(!error&&modified){
            part_text_add(out,L"该标准件在 Creo 中已有未保存修改。请核对当前窗口后再从已验收版本开始。\n");
            error=PRO_TK_BAD_CONTEXT;
        }
    }
    ProMdl current=NULL;
    if(!error&&(ProMdlCurrentGet(&current)||current!=*model))error=dv_display_new(*model);
    if(error&&opened){ProMdlErase(*model);*model=NULL;}
    if(!error){
        ProMdlName name=L"";
        ProMdlMdlnameGet(*model,name);
        part_text_add(out,L"公司标准件 %ls 已打开。发送「按当前标准件重建新规格，长 …、宽 …、高 …」，或指定孔径、板厚。结果会在新零件中生成。\n",name);
    }
    return error;
}

static ProError imitation_open_standard_dialog(PartText *out)
{
    ProPath directory,path=L"";
    ProError error=imitation_standard_directory(directory);
    if(error){
        part_text_add(out,L"当前 BWTL 配置还没有公司标准件目录。请先归档一个完成尺寸试改的结果。\n");
        return error;
    }
    error=imitation_choose_at(path,_countof(path),L"选择已验收公司标准件",0,directory);
    ProMdl model=NULL;
    return error?error:imitation_open_standard(path,&model,out);
}
