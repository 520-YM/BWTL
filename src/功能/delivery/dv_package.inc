static ProError dv_no_external_dependencies(ProMdl model){
    ProExtRefInfo *refs=NULL;
    int count=0;
    ProError error=ProSolidExternParentsGet((ProSolid)model,PRO_ALL_EXT_REF_TYPES,&refs,&count);
    if(refs)ProExtRefInfoFree(&refs);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    if(count)return PRO_TK_BAD_CONTEXT;
    ProFamtable table;
    error=ProFamtableInit(model,&table);
    if(!error){
        error=ProFamtableCheck(&table);
        if(!error)return PRO_TK_BAD_CONTEXT;
    }
    return error==PRO_TK_E_NOT_FOUND||error==PRO_TK_EMPTY?PRO_TK_NO_ERROR:error;
}

static ProError dv_native_revision(const wchar_t *folder,const wchar_t *name,ProPath leaf){
    ProPath pattern;
    WIN32_FIND_DATAW file;
    int latest=0;
    if(swprintf_s(pattern,PRO_PATH_SIZE,L"%ls\\%ls.prt.*",folder,name)<0)return PRO_TK_BAD_INPUTS;
    HANDLE find=FindFirstFileW(pattern,&file);
    if(find==INVALID_HANDLE_VALUE)return PRO_TK_E_NOT_FOUND;
    do{
        const wchar_t *dot=wcsrchr(file.cFileName,L'.');
        int revision;
        if(!(file.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
           (file.nFileSizeHigh||file.nFileSizeLow)&&dot&&part_integer(dot+1,&revision)&&revision>latest){
            latest=revision;
            wcscpy_s(leaf,PRO_PATH_SIZE,file.cFileName);
        }
    }while(FindNextFileW(find,&file));
    FindClose(find);
    return latest?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError dv_native_copy(ProMdl model,DeliveryOutput *output,ProPath leaf){
    ProMdlName source,name;
    ProMdl copy=NULL;
    ProPath origin_before,origin_after;
    ProMassProperty original,duplicate;
    ProError error;
    if((error=ProMdlMdlnameGet(model,source))||
       (error=ProMdlOriginGet(model,origin_before))||(error=qa_geometry_mass(model,&original)))return error;
    if(swprintf_s(name,_countof(name),L"%.22ls_DELIVERY",source)<0||
       !part_target_available(model,name,L"prt"))return PRO_TK_E_FOUND;
    error=ProMdlnameCopy(model,name,&copy);
    if(!error)error=part_health(copy);
    if(!error)error=qa_geometry_mass(copy,&duplicate);
    if(!error&&(!part_close(original.volume,duplicate.volume)||!part_close(original.surface_area,duplicate.surface_area)))
        error=PRO_TK_GENERAL_ERROR;
    if(!error&&dv_native_revision(output->stage,name,leaf)){
        error=ProMdlSave(copy);
        if(!error)error=dv_native_revision(output->stage,name,leaf);
    }
    if(copy&&ProMdlErase(copy)&&!error)error=PRO_TK_GENERAL_ERROR;
    if(ProMdlOriginGet(model,origin_after)||_wcsicmp(origin_before,origin_after)){
        dv_recovery_uncertain=1;
        return PRO_TK_GENERAL_ERROR;
    }
    return error;
}

static int dv_sha256(const wchar_t *path,wchar_t digest[65],ULONGLONG *size){
    HCRYPTPROV provider=0;
    HCRYPTHASH hash=0;
    HANDLE file=INVALID_HANDLE_VALUE;
    BYTE block[16384],bytes[32];
    DWORD count,length=sizeof(bytes);
    int success=0;
    *size=0;
    if(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)||
       !CryptCreateHash(provider,CALG_SHA_256,0,0,&hash))goto done;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE)goto done;
    BY_HANDLE_FILE_INFORMATION info;
    if(!GetFileInformationByHandle(file,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
        goto done;
    for(;;){
        if(!ReadFile(file,block,sizeof(block),&count,NULL))goto done;
        if(!count)break;
        if(!CryptHashData(hash,block,count,0))goto done;
        *size+=count;
    }
    if(!*size||!CryptGetHashParam(hash,HP_HASHVAL,bytes,&length,0)||length!=sizeof(bytes))goto done;
    for(int i=0;i<32;i++)swprintf_s(digest+2*i,65-2*i,L"%02x",bytes[i]);
    success=1;
done:
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    return success;
}

static ProError dv_manifest(const wchar_t *directory,const wchar_t *model,ProPath leaves[3]){
    ProPath path;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\manifest.csv",directory)<0)return PRO_TK_BAD_INPUTS;
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file)return PRO_TK_CANT_WRITE;
    ProError error=fwprintf(file,L"源模型,文件,字节,SHA256\n")<0?PRO_TK_CANT_WRITE:PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<3;i++){
        wchar_t digest[65];
        ULONGLONG bytes;
        if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",directory,leaves[i])<0||
           !dv_sha256(path,digest,&bytes)||!dv_csv_text(file,model)||fputwc(L',',file)==WEOF||
           !dv_csv_text(file,leaves[i])||fwprintf(file,L",%llu,%ls\n",bytes,digest)<0)error=PRO_TK_CANT_WRITE;
    }
    if(fclose(file)!=0&&!error)error=PRO_TK_CANT_WRITE;
    return error;
}

static ProError dv_package(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput output={0};
    ProPath leaves[3]={{0},{0},L"machining.csv"};
    ProMdlName name;
    ProError error;
    int features,bores;
    if((error=qa_single_body(model,NULL))||(error=dv_no_external_dependencies(model))||
       (error=ProMdlMdlnameGet(model,name))||(error=dv_stage_begin(cmd->arg[2],&output)))return error;
    error=dv_stage_enter(&output);
    if(!error)error=dv_export_file(model,output.stage,PRO_INTF_EXPORT_STEP,leaves[0]);
    if(!error)error=dv_report_file(model,output.stage,&features,&bores);
    if(!error)error=dv_native_copy(model,&output,leaves[1]);
    if(!error)error=dv_manifest(output.stage,name,leaves);
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"交付包已生成：%ls\n文件包含 %ls、%ls、machining.csv 和 SHA-256 清单 manifest.csv。\n",
        output.target,leaves[0],leaves[1]);
    return error;
}
