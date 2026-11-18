/* Compare opened files, avoiding alternate slash spellings from Creo. */
static int assembly_rename_same_file(const wchar_t *first,const wchar_t *second){
    const DWORD shares=FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
    HANDLE a=CreateFileW(first,FILE_READ_ATTRIBUTES,shares,NULL,OPEN_EXISTING,0,NULL);
    if(a==INVALID_HANDLE_VALUE)return 0;
    HANDLE b=CreateFileW(second,FILE_READ_ATTRIBUTES,shares,NULL,OPEN_EXISTING,0,NULL);
    BY_HANDLE_FILE_INFORMATION left,right;
    int same=b!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(a,&left)&&GetFileInformationByHandle(b,&right)&&
        left.dwVolumeSerialNumber==right.dwVolumeSerialNumber&&left.nFileIndexHigh==right.nFileIndexHigh&&
        left.nFileIndexLow==right.nFileIndexLow;
    if(b!=INVALID_HANDLE_VALUE)CloseHandle(b);
    CloseHandle(a);
    return same;
}

static void assembly_rename_refresh(ProMdl model){
    int window;
    if(!ProMdlWindowGet(model,&window))trace("Refresh renamed assembly tree",ProTreetoolRefresh(model));
}
static int assembly_rename_equal_content(const wchar_t *a,const wchar_t *b){
    FILE *one=_wfopen(a,L"rb"),*two=_wfopen(b,L"rb");
    int match=one&&two;
    unsigned char left[8192],right[8192];
    while(match){
        size_t n=fread(left,1,sizeof(left),one),m=fread(right,1,sizeof(right),two);
        match=n==m&&!memcmp(left,right,n)&&!ferror(one)&&!ferror(two);
        if(n<sizeof(left))break;
    }
    if(one)fclose(one);
    if(two)fclose(two);
    return match;
}

static ProError assembly_rename_copy_history(const wchar_t *source,const wchar_t *destination,
                                             const wchar_t *name,int restore){
    ProPath pattern;
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.asm*",source,name)<0)return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW file;
    HANDLE find=FindFirstFileW(pattern,&file);
    if(find==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    const size_t prefix=wcslen(name)+4;
    int count=0;
    ProError error=PRO_TK_NO_ERROR;
    do{
        const size_t length=wcslen(file.cFileName);
        if(length<prefix)continue;
        if(length>prefix){
            const wchar_t *suffix=file.cFileName+prefix;
            wchar_t *end=NULL;
            if(*suffix!=L'.'||wcstol(suffix+1,&end,10)<1||*end)continue;
        }
        if(file.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)){
            error=PRO_TK_BAD_CONTEXT;
            break;
        }
        ProPath from,to;
        if(swprintf_s(from,_countof(from),L"%ls\\%ls",source,file.cFileName)<0||
           swprintf_s(to,_countof(to),L"%ls\\%ls",destination,file.cFileName)<0){
            error=PRO_TK_BAD_INPUTS;
            break;
        }
        if(!CopyFileW(from,to,TRUE)&&(!restore||GetFileAttributesW(to)==INVALID_FILE_ATTRIBUTES)){
            error=PRO_TK_CANT_WRITE;
            break;
        }
        if(!assembly_rename_equal_content(from,to)){
            error=PRO_TK_GENERAL_ERROR;
            break;
        }
        count++;
    }while(FindNextFileW(find,&file));
    DWORD scan=GetLastError();
    FindClose(find);
    return error?error:!count||scan!=ERROR_NO_MORE_FILES?PRO_TK_CANT_OPEN:PRO_TK_NO_ERROR;
}

static ProError assembly_rename_backup(const wchar_t *directory,const wchar_t *name,ProPath backup){
    wchar_t library[MAX_PATH];
    if(!storage_method_directory(library,L"装配改名备份",1)||
       swprintf_s(backup,PRO_PATH_SIZE,L"%ls\\%08lx-%016llx",library,GetCurrentProcessId(),GetTickCount64())<0||
       !CreateDirectoryW(backup,NULL))return PRO_TK_CANT_WRITE;
    return assembly_rename_copy_history(directory,backup,name,0);
}
static ProError assembly_rename_dependency_has(ProMdl owner,ProMdlType type,
                                               ProMdl target,const wchar_t *name,int *found){
    *found=0;
    if(type==PRO_MDL_DRAWING){
        ProSolid *models=NULL;
        int count=0;
        ProError error=ProDrawingSolidsCollect((ProDrawing)owner,&models);
        if(!error)error=ProArraySizeGet((ProArray)models,&count);
        for(int i=0;!error&&i<count;i++)
            if((ProMdl)models[i]==target){*found=1;break;}
        if(models)ProArrayFree((ProArray *)&models);
        return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
    }
    ProMdlnameShortdata *refs=NULL;
    ProMdlfileType *kinds=NULL;
    int size=0;
    ProError error=ProMdlDependenciesCleanup(owner);
    if(!error||error==PRO_TK_E_NOT_FOUND)
        error=ProMdlDependenciesDataList(owner,&refs,&kinds,&size);
    for(int i=0;i<size&&refs&&kinds;i++){
        if(kinds[i]==PRO_MDLFILE_ASSEMBLY&&!_wcsicmp(refs[i].name,name)){
            *found=1;
            break;
        }
    }
    if(refs)ProArrayFree((ProArray *)&refs);
    if(kinds)ProArrayFree((ProArray *)&kinds);
    return *found||error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}

static ProError assembly_rename_references(ProMdl target,const wchar_t *name,PartText *out){
    const ProMdlType types[]={PRO_MDL_ASSEMBLY,PRO_MDL_DRAWING};
    for(size_t k=0;k<_countof(types);k++){
        ProMdl *models=NULL;
        int count=0;
        ProError error=ProSessionMdlList(types[k],&models,&count);
        if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
        for(int i=0;!error&&i<count;i++){
            if(models[i]==target)continue;
            int found=0;
            error=assembly_rename_dependency_has(models[i],types[k],target,name,&found);
            if(found){
                ProMdlName dependent=L"";
                ProMdlMdlnameGet(models[i],dependent);
                part_text_add(out,L"已加载的%ls %ls 引用 %ls。先处理它的引用，再改名，避免该引用随会话改名改变。\n",
                    types[k]==PRO_MDL_ASSEMBLY?L"上级装配":L"工程图",dependent,name);
                error=PRO_TK_E_IN_USE;
            }
        }
        if(models)ProArrayFree((ProArray *)&models);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

/* A newly named ASM is saved only for a clean, already saved local source.
   Keep the old file so unopened assemblies and drawings can still resolve it. */
static ProError assembly_rename_apply(ProMdl model,const wchar_t *target_name,PartText *out){
    ProMdlType type;
    ProMdlName old=L"",target=L"",actual=L"";
    ProPath directory=L"",origin=L"",original_file=L"",new_file=L"",lock_path=L"",backup=L"";
    ProBoolean modified=PRO_B_FALSE;
    ProError error;
    HANDLE name_lock=INVALID_HANDLE_VALUE;
    int occupied=0,saved=0,persist=0,renamed=0,backup_ready=0,restore_attempted=0;

    if(!model||!model_assembly_name_valid(target_name))return PRO_TK_INVALID_NAME;
    if((error=ProMdlTypeGet(model,&type)))return error;
    if(type!=PRO_MDL_ASSEMBLY)return PRO_TK_INVALID_TYPE;
    if((error=ProMdlMdlnameGet(model,old))||(error=ProDirectoryCurrentGet(directory))||
       (error=ProMdlOriginGet(model,origin)))return error;
    if(!storage_prepare_output()||!storage_output_directory(directory)){
        part_text_add(out,L"%ls\n",storage_output_message);
        return PRO_TK_BAD_CONTEXT;
    }
    wcscpy_s(target,PRO_NAME_SIZE,target_name);
    if(!_wcsicmp(target,old)){
        part_text_add(out,L"活动装配已经叫 %ls.asm。\n",old);
        return PRO_TK_NO_ERROR;
    }
    error=model_assembly_available(target,directory);
    if(error==PRO_TK_E_FOUND)
        part_text_add(out,L"已存在同名装配 %ls.asm，位置在 Creo 会话或当前工作目录 %ls。\n",target,directory);
    if(error)return error;

    error=model_assembly_disk(directory,old,original_file,&occupied);
    if(error)return error;
    saved=original_file[0]!=0;
    int fresh=model_assembly_creation_index(model,old,directory,origin);
    if((occupied&&!saved)||(origin[0]&&fresh<0&&(!saved||!assembly_rename_same_file(origin,original_file)))){
        part_text_add(out,L"当前目录里的原装配版本与 Creo 的加载来源不同，请在原装配所在目录打开最新版本后改名。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    if(saved){
        if((error=ProMdlModificationVerify(model,&modified)))return error;
        persist=!modified;
    }
    error=assembly_rename_references(model,old,out);
    if(error){
        if(error!=PRO_TK_E_IN_USE)part_text_add(out,L"当前会话里的装配或工程图引用未能核对，未开始改名。\n");
        return error;
    }
    if(swprintf_s(lock_path,PRO_PATH_SIZE,L"%ls\\.bwtl-asm-rename-%ls.lock",directory,target)<0)
        return PRO_TK_BAD_INPUTS;
    name_lock=CreateFileW(lock_path,GENERIC_WRITE|DELETE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(name_lock==INVALID_HANDLE_VALUE){
        part_text_add(out,L"无法预留新装配名 %ls。请检查目录写入权限或是否有另一个会话正在使用该名称。\n",target);
        return PRO_TK_E_IN_USE;
    }
    error=model_assembly_available(target,directory);
    if(error)goto done;
    if(saved){
        error=assembly_rename_backup(directory,old,backup);
        if(error){
            part_text_add(out,L"原装配的磁盘版本未能完整备份，改名未开始。\n");
            goto done;
        }
        backup_ready=1;
    }
    error=ProMdlnameRename(model,target);
    renamed=!error;
    if(error)goto done;
    ProMdl located=NULL;
    if((error=ProMdlMdlnameGet(model,actual))||
       (error=ProMdlnameInit(target,PRO_MDL_ASSEMBLY,&located)))goto done;
    if(_wcsicmp(actual,target)||located!=model){error=PRO_TK_GENERAL_ERROR;goto done;}
    assembly_rename_refresh(model);

    if(persist){
        error=ProMdlSave(model);
        if(!error)error=model_assembly_disk(directory,target,new_file,&occupied);
        if(!error&&!new_file[0])error=PRO_TK_CANT_WRITE;
        if(error)goto done;
    }
    if(backup_ready){
        restore_attempted=1;
        error=assembly_rename_copy_history(backup,directory,old,1);
        if(error)goto done;
    }
    if(persist)
        part_text_add(out,L"已把装配 %ls 改名为 %ls，存盘并核对新文件 %ls。原图号文件已逐字节核对保留，当前会话的上级装配和工程图已检查。\n",
            old,actual,new_file);
    else if(saved)
        part_text_add(out,L"装配 %ls 已在 Creo 会话内改名为 %ls，名称已回读。原图号文件已保留，当前修改可用新名称保存。\n",old,actual);
    else
        part_text_add(out,L"装配 %ls 已在 Creo 会话内改名为 %ls，名称已回读，未自动保存。\n",old,actual);

done:
    if(error){
        int recovered=!renamed;
        if(renamed&&!restore_attempted){
            recovered=!ProMdlnameRename(model,old)&&!ProMdlMdlnameGet(model,actual)&&!_wcsicmp(actual,old);
            if(recovered)assembly_rename_refresh(model);
        }
        if(backup_ready&&!restore_attempted)
            recovered=assembly_rename_copy_history(backup,directory,old,1)==PRO_TK_NO_ERROR&&recovered;
        part_text_add(out,L"装配改名未完成（Creo %d）。%ls\n",error,
            recovered?L"已保留原装配名。":L"需要核对 Creo 模型树和两个图号的磁盘文件。");
        if(backup_ready)part_text_add(out,L"原图号备份：%ls。\n",backup);
        if(renamed){
            if(!new_file[0])model_assembly_disk(directory,target,new_file,&occupied);
            if(new_file[0])part_text_add(out,L"检测到新名字的磁盘文件，请核对 %ls。\n",new_file);
        }
    }
    if(!error&&fresh>=0)model_assembly_creation_record(model,target);
    CloseHandle(name_lock);
    return error;
}