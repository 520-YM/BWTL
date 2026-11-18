/* Exact Creo instance for the published company M3 x 6 BSO model. */
static const wchar_t fastener_bso_name[]=L"BSO_3_5M3X6";
static const wchar_t fastener_bso_project_name[]=L"BSO_3_5M3X6_BWTL";
static ProError fastener_bso_verify_copy(ProMdl source,ProMdl copy,PartText *out);

static ProError fastener_bso_read(ProMdl *model,wchar_t path[MAX_PATH],PartText *out){
    const UINT category=COMPANY_OPEN_FASTENER_FIRST+4; /* Press-fit hardware. */
    wchar_t folder[MAX_PATH];
    if(!company_fastener_resolve(category,folder)){
        part_text_add(out,L"公司库的「压铆件」目录无法读取。请检查所选数据盘。");
        return PRO_TK_CANT_OPEN;
    }
    ProError error=ax_native_source(folder,L"bso_3_5m3xx",PRO_MDL_PART,path);
    if(error){part_text_add(out,L"压铆件目录可读，但未找到 BSO 的族表母件。");return error;}
    wchar_t verified[MAX_PATH];
    HANDLE file=fastener_chat_open_source(path,verified);
    if(file==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    NativeFile source={0};
    HANDLE lock=company_library_source(verified,&source,out);
    if(lock==INVALID_HANDLE_VALUE){CloseHandle(file);return PRO_TK_CANT_OPEN;}
    ProMdl generic=NULL,owner=NULL;
    int loaded=0;
    ProFamtable table;
    ProFaminstance instance;
    ProName requested;
    wcscpy_s(requested,_countof(requested),fastener_bso_name);
    error=attachment_open_native(&source,&generic,&loaded,out);
    if(!error)error=ProFamtableInit(generic,&table);
    if(!error)error=ProFaminstanceInit(requested,&table,&instance);
    if(!error)error=ProFaminstanceRetrieve(&instance,model);
    if(!error)error=ProFaminstanceGenericGet(*model,PRO_B_TRUE,&owner);
    if(!error&&owner!=generic)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=part_health(*model);
    if(!error){
        wcscpy_s(path,MAX_PATH,verified);
        part_text_add(out,L"已从公司库读到 BSO M3×6，Creo 实例 %ls。",fastener_bso_name);
    }else if(!out->text[0])part_text_add(out,L"找到族表母件，Creo 未能读取确切的 %ls 实例。",fastener_bso_name);
    CloseHandle(lock);
    CloseHandle(file);
    return error;
}


static ProError fastener_bso_copy(ProMdl source,ProMdl *copy,wchar_t saved[MAX_PATH],PartText *out){
    ProPath directory,existing;
    if(ProDirectoryCurrentGet(directory))return PRO_TK_BAD_CONTEXT;
    company_trim_path(directory);
    if(!storage_output_directory(directory)){
        part_text_add(out,L"当前 Creo 工作目录不可写，请切换到所选数据盘的项目目录。");
        return PRO_TK_BAD_CONTEXT;
    }
    ProError error=ax_native_source(directory,fastener_bso_project_name,PRO_MDL_PART,existing);
    if(!error){
        NativeFile file={0};
        HANDLE lock=company_library_source(existing,&file,out);
        if(lock==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
        int loaded=0;
        error=attachment_open_native(&file,copy,&loaded,out);
        /* The newly created copy can still have its private staging origin. */
        if(error==PRO_TK_E_IN_USE&&*copy){
            ProPath origin;
            const FastenerSavedView *known=fastener_saved_model(*copy);
            if(known&&!ProMdlOriginGet(*copy,origin)&&company_is_under(origin,known->stage)&&
               fastener_equal_files(origin,existing)){
                out->text[0]=0;
                error=0;
            }
        }
        CloseHandle(lock);
        if(!error)error=fastener_bso_verify_copy(source,*copy,out);
        if(!error)wcscpy_s(saved,MAX_PATH,existing);
        if(error&&loaded&&*copy){ProMdlErase(*copy);*copy=NULL;}
        return error;
    }
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    static LONG serial;
    wchar_t stage[MAX_PATH];
    if(!storage_output_directory(ai_dir)||
       swprintf_s(stage,_countof(stage),L"%ls\\bso-%lu",ai_dir,InterlockedIncrement(&serial))<0||
       !CreateDirectoryW(stage,NULL))return PRO_TK_CANT_WRITE;
    error=ProDirectoryChange(stage);
    if(error)return error;
    error=ProMdlnameCopy(source,(wchar_t *)fastener_bso_project_name,copy);
    ProError restored=ProDirectoryChange(directory);
    if(restored)return restored;
    if(!error)error=fastener_bso_verify_copy(source,*copy,out);
    if(!error)error=ax_native_source(stage,fastener_bso_project_name,PRO_MDL_PART,existing);
    if(!error&&swprintf_s(saved,MAX_PATH,L"%ls\\%ls",directory,wcsrchr(existing,L'\\')+1)<0)error=PRO_TK_BAD_INPUTS;
    if(!error&&(!CopyFileW(existing,saved,TRUE)&&!fastener_equal_files(existing,saved)))error=PRO_TK_CANT_WRITE;
    if(!error&&!fastener_equal_files(existing,saved))error=PRO_TK_CANT_WRITE;
    if(!error){
        HANDLE staged=CreateFileW(existing,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        if(staged!=INVALID_HANDLE_VALUE){
            wcscpy_s(fastener_last_stage,MAX_PATH,stage);
            fastener_saved_record(existing,staged,*copy);
            CloseHandle(staged);
        }
    }else if(*copy){ProMdlErase(*copy);*copy=NULL;}
    return error;
}
