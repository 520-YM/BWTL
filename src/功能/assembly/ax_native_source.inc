/* BWTL company standard models live under the active instance's data root. */
static ProPath ax_standard_data_directory;

static ProError ax_target_collision(const wchar_t *name,ProMdlType type){
    if(!dv_safe_leaf(name))return PRO_TK_BAD_INPUTS;
    ProPath folder,pattern;
    ProMdl existing=NULL;
    ProError error=ProMdlnameInit((wchar_t *)name,type,&existing);
    if(!error)return PRO_TK_E_FOUND;
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    if((error=ProDirectoryCurrentGet(folder)))return error;
    const wchar_t *extension=type==PRO_MDL_ASSEMBLY?L"asm":L"prt";
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.%ls",folder,name,extension)<0)return PRO_TK_BAD_INPUTS;
    if(GetFileAttributesW(pattern)!=INVALID_FILE_ATTRIBUTES)return PRO_TK_E_FOUND;
    if(GetLastError()!=ERROR_FILE_NOT_FOUND)return PRO_TK_CANT_OPEN;
    if(wcscat_s(pattern,_countof(pattern),L".*"))return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW entry;
    HANDLE search=FindFirstFileW(pattern,&entry);
    if(search!=INVALID_HANDLE_VALUE){FindClose(search);return PRO_TK_E_FOUND;}
    return GetLastError()==ERROR_FILE_NOT_FOUND?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
}

static ProError ax_native_source(const wchar_t *folder,const wchar_t *name,ProMdlType type,ProPath path){
    const wchar_t *extension=type==PRO_MDL_ASSEMBLY?L"asm":L"prt";
    ProPath pattern,leaf=L"";
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.%ls.*",folder,name,extension)<0)return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW entry;
    HANDLE search=FindFirstFileW(pattern,&entry);
    int latest=0;
    if(search!=INVALID_HANDLE_VALUE){
        do{
            const wchar_t *dot=wcsrchr(entry.cFileName,L'.');
            int revision;
            if(!(entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
               (entry.nFileSizeHigh||entry.nFileSizeLow)&&dot&&part_integer(dot+1,&revision)&&revision>latest){
                latest=revision;
                wcscpy_s(leaf,_countof(leaf),entry.cFileName);
            }
        }while(FindNextFileW(search,&entry));
        DWORD ended=GetLastError();
        FindClose(search);
        if(ended!=ERROR_NO_MORE_FILES)return PRO_TK_CANT_OPEN;
    }else if(GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)return PRO_TK_CANT_OPEN;
    if(latest)
        return swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",folder,leaf)>=0?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls.%ls",folder,name,extension)<0)return PRO_TK_BAD_INPUTS;
    return dv_file_nonempty(path)?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError ax_find_standard(const wchar_t *library,const wchar_t *name,ProMdlType type,ProPath path){
    ProError error=ax_native_source(library,name,type,path);
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    ProPath pattern,folder,candidate;
    if(swprintf_s(pattern,PRO_PATH_SIZE,L"%ls\\*",library)<0)return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW entry;
    HANDLE search=FindFirstFileW(pattern,&entry);
    if(search==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?PRO_TK_E_NOT_FOUND:PRO_TK_CANT_OPEN;
    int matches=0;
    error=PRO_TK_NO_ERROR;
    do{
        if(!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(entry.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
           !wcscmp(entry.cFileName,L".")||!wcscmp(entry.cFileName,L".."))continue;
        if(swprintf_s(folder,PRO_PATH_SIZE,L"%ls\\%ls",library,entry.cFileName)<0){error=PRO_TK_BAD_INPUTS;break;}
        ProError found=ax_native_source(folder,name,type,candidate);
        if(found==PRO_TK_E_NOT_FOUND)continue;
        if(found){error=found;break;}
        if(++matches>1){error=PRO_TK_E_FOUND;break;}
        wcscpy_s(path,PRO_PATH_SIZE,candidate);
    }while(FindNextFileW(search,&entry));
    DWORD ended=GetLastError();
    FindClose(search);
    if(!error&&ended!=ERROR_NO_MORE_FILES)error=PRO_TK_CANT_OPEN;
    return error?error:matches==1?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}
static ProError ax_load_native(const wchar_t *name,ProMdlType type,ProMdl *model,int *loaded){
    *loaded=0;
    if(!dv_safe_leaf(name))return PRO_TK_BAD_INPUTS;
#ifdef BWTL_COMPANY_SOURCE_BINDING
    ProError bound=company_source_resolve(name,type,model,loaded);
    if(bound!=PRO_TK_NOT_EXIST)return bound;
#endif
    ProError error=ProMdlnameInit((wchar_t *)name,type,model);
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    ProPath directory,path;
    if((error=ProDirectoryCurrentGet(directory)))return error;
    error=ax_native_source(directory,name,type,path);
    if(error==PRO_TK_E_NOT_FOUND&&ax_standard_data_directory[0]){
        wcscpy_s(directory,_countof(directory),ax_standard_data_directory);
        wchar_t *end=wcsrchr(directory,L'\\');
        if(end&&!_wcsicmp(end+1,L"data")){
            *end=0;
            if(wcscat_s(directory,_countof(directory),L"\\公司资料\\公司标准\\公司标准件"))return PRO_TK_BAD_INPUTS;
            DWORD attributes=GetFileAttributesW(directory);
            if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&
               !(attributes&FILE_ATTRIBUTE_REPARSE_POINT))
                error=ax_find_standard(directory,name,type,path);
        }
    }
    if(error)return error;
    error=ProMdlFiletypeLoad(path,type==PRO_MDL_ASSEMBLY?PRO_MDLFILE_ASSEMBLY:PRO_MDLFILE_PART,PRO_B_FALSE,model);
    if(!error)*loaded=1;
    return error;
}
