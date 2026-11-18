/* A successful library read binds a saved source revision for the current user turn. */
#define COMPANY_SOURCE_LIMIT 24
typedef enum {
    COMPANY_SOURCE_UNVERIFIED,
    COMPANY_SOURCE_READY,
    COMPANY_SOURCE_AMBIGUOUS
} CompanySourceState;

typedef struct {
    ProMdlName name;
    ProMdlType type;
    NativeFile file;
    BY_HANDLE_FILE_INFORMATION stamp;
    CompanySourceState state;
    ProMdl placed_model;
    ProWVerstamp placed_stamp;
} CompanySource;

static CompanySource company_sources[COMPANY_SOURCE_LIMIT];
static size_t company_source_count;
static int company_source_overflow;
static CompanySource *company_placement_source;
static ProMdl company_placement_model;

static void company_source_forget_placement(CompanySource *source){
    if(source->placed_stamp)ProVerstampFree(&source->placed_stamp);
    source->placed_stamp=NULL;
    source->placed_model=NULL;
}

static void company_source_reset(void){
    for(size_t i=0;i<company_source_count;i++)company_source_forget_placement(&company_sources[i]);
    company_placement_source=NULL;
    company_placement_model=NULL;
    company_source_count=0;
    company_source_overflow=0;
}

static CompanySource *company_source_find(const wchar_t *name,ProMdlType type){
    for(size_t i=0;i<company_source_count;i++){
        CompanySource *source=&company_sources[i];
        if(source->type==type&&!_wcsicmp(source->name,name))return source;
    }
    return NULL;
}

static HANDLE company_source_open(const wchar_t *path,BY_HANDLE_FILE_INFORMATION *info){
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE)return file;
    wchar_t resolved[MAX_PATH+4];
    DWORD length=GetFinalPathNameByHandleW(file,resolved,_countof(resolved),VOLUME_NAME_DOS);
    if(length<7||length>=_countof(resolved)||wcsncmp(resolved,L"\\\\?\\",4)||
       _wcsicmp(resolved+4,path)||GetFileType(file)!=FILE_TYPE_DISK||
       !GetFileInformationByHandle(file,info)||
       (info->dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       (!info->nFileSizeHigh&&!info->nFileSizeLow)){
        CloseHandle(file);
        return INVALID_HANDLE_VALUE;
    }
    return file;
}

static int company_source_matches(const BY_HANDLE_FILE_INFORMATION *first,const BY_HANDLE_FILE_INFORMATION *second){
    return first->dwVolumeSerialNumber==second->dwVolumeSerialNumber&&
           first->nFileIndexHigh==second->nFileIndexHigh&&first->nFileIndexLow==second->nFileIndexLow&&
           first->nFileSizeHigh==second->nFileSizeHigh&&first->nFileSizeLow==second->nFileSizeLow&&
           !CompareFileTime(&first->ftLastWriteTime,&second->ftLastWriteTime);
}

static int company_source_pin(const wchar_t *validated_path){
    wchar_t path[MAX_PATH];
    DWORD length=GetFullPathNameW(validated_path,_countof(path),path,NULL);
    if(length<4||length>=_countof(path)||path[1]!=L':'||path[2]!=L'\\')return 0;
    const wchar_t *leaf=wcsrchr(path,L'\\')+1;
    const wchar_t *suffix=native_file_suffix(leaf);
    AttachmentKind kind=native_file_kind(suffix);
    if(kind!=ATTACH_PART&&kind!=ATTACH_ASSEMBLY)return 0;

    ProMdlName name;
    ProMdlType type=kind==ATTACH_PART?PRO_MDL_PART:PRO_MDL_ASSEMBLY;
    if(suffix==leaf||wcsncpy_s(name,_countof(name),leaf,suffix-leaf))return 0;
    CompanySource *source=company_source_find(name,type);
    if(source&&source->state==COMPANY_SOURCE_AMBIGUOUS)return 0;
    if(source&&_wcsicmp(path,source->file.source)){
        source->state=COMPANY_SOURCE_AMBIGUOUS;
        company_source_forget_placement(source);
        return 0;
    }
    if(!source){
        if(company_source_count>=_countof(company_sources)){
            company_source_overflow=1;
            return 0;
        }
        source=&company_sources[company_source_count++];
        memset(source,0,sizeof(*source));
        wcscpy_s(source->name,_countof(source->name),name);
        source->type=type;
        wcscpy_s(source->file.source,_countof(source->file.source),path);
        wcscpy_s(source->file.name,_countof(source->file.name),leaf);
        source->file.kind=kind;
    }
    BY_HANDLE_FILE_INFORMATION info;
    HANDLE file=company_source_open(path,&info);
    if(file==INVALID_HANDLE_VALUE){
        source->state=COMPANY_SOURCE_UNVERIFIED;
        company_source_forget_placement(source);
        return 0;
    }
    CloseHandle(file);
    if(source->state==COMPANY_SOURCE_READY&&!company_source_matches(&source->stamp,&info)){
        source->state=COMPANY_SOURCE_UNVERIFIED;
        company_source_forget_placement(source);
        return 0;
    }
    source->stamp=info;
    source->file.modified=info.ftLastWriteTime;
    source->file.size.HighPart=(LONG)info.nFileSizeHigh;
    source->file.size.LowPart=info.nFileSizeLow;
    source->state=COMPANY_SOURCE_READY;
    return 1;
}

/* A selected but unverified source must still prevent a same-name fallback. */
static void company_source_block(const wchar_t *validated_path){
    for(size_t i=0;i<company_source_count;i++){
        CompanySource *source=&company_sources[i];
        if(!_wcsicmp(source->file.source,validated_path)&&source->state!=COMPANY_SOURCE_AMBIGUOUS){
            source->state=COMPANY_SOURCE_UNVERIFIED;
            company_source_forget_placement(source);
            return;
        }
    }
}

/* Assembly regeneration can mark a placed component dirty. Reuse only an exact
   model already placed by this turn, with no changes since that placement. */
static ProError company_source_reuse(CompanySource *source,HANDLE reference,ProMdl *model){
    if(!source->placed_stamp)return PRO_TK_NOT_EXIST;
    ProMdl existing=NULL;
    ProError error=ProMdlnameInit(source->name,source->type,&existing);
    if(error==PRO_TK_E_NOT_FOUND||(!error&&existing!=source->placed_model))return PRO_TK_NOT_EXIST;
    if(error)return error==PRO_TK_NOT_EXIST?PRO_TK_E_IN_USE:error;
    ProPath origin;
    if(ProMdlOriginGet(existing,origin)||!attachment_same_file(origin,reference))return PRO_TK_E_IN_USE;
    ProWVerstamp stamp=NULL;
    error=ProMdlVerstampGet(existing,&stamp);
    int same=!error&&stamp&&ProVerstampEqual(source->placed_stamp,stamp);
    if(stamp)ProVerstampFree(&stamp);
    if(!same)return PRO_TK_E_IN_USE;
    *model=existing;
    return PRO_TK_NO_ERROR;
}

static void company_source_note_placement(ProMdl model){
    CompanySource *source=company_placement_source;
    ProMdl selected=company_placement_model;
    company_placement_source=NULL;
    company_placement_model=NULL;
    if(!source||!model||model!=selected||source->state!=COMPANY_SOURCE_READY)return;
    company_source_forget_placement(source);
    BY_HANDLE_FILE_INFORMATION info;
    HANDLE reference=company_source_open(source->file.source,&info);
    if(reference==INVALID_HANDLE_VALUE)return;
    ProPath origin;
    if(company_source_matches(&source->stamp,&info)&&
       !ProMdlOriginGet(model,origin)&&attachment_same_file(origin,reference)&&
       !ProMdlVerstampGet(model,&source->placed_stamp)&&source->placed_stamp)
        source->placed_model=model;
    else company_source_forget_placement(source);
    CloseHandle(reference);
}

static ProError company_source_resolve(const wchar_t *name,ProMdlType type,ProMdl *model,int *loaded){
    company_placement_source=NULL;
    company_placement_model=NULL;
    CompanySource *source=company_source_find(name,type);
    if(!source)return company_source_overflow?PRO_TK_OUT_OF_RANGE:PRO_TK_NOT_EXIST;
    *model=NULL;
    *loaded=0;
    if(source->state==COMPANY_SOURCE_AMBIGUOUS)return PRO_TK_E_FOUND;
    if(source->state!=COMPANY_SOURCE_READY)return PRO_TK_CANT_OPEN;

    BY_HANDLE_FILE_INFORMATION info;
    HANDLE reference=company_source_open(source->file.source,&info);
    if(reference==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    if(!company_source_matches(&source->stamp,&info)){
        CloseHandle(reference);
        return PRO_TK_E_IN_USE;
    }
    wchar_t report[512]=L"";
    PartText out={report,_countof(report),0};
    ProError error=company_source_reuse(source,reference,model);
    if(error==PRO_TK_NOT_EXIST)error=attachment_open_native(&source->file,model,loaded,&out);
    CloseHandle(reference);
    if(error){
        /* The loader can report a mismatched origin after a new model was loaded. */
        if(*loaded&&*model&&attachment_release_count<NATIVE_ATTACHMENT_MAX){
            attachment_release[attachment_release_count++]=*model;
            if(ProMacroLoad(L"~ Command \x0060" L"BWTLAttachmentCleanup\x0060;"))
                trace("Could not queue rejected company source cleanup",-1);
        }
        *loaded=0;
        *model=NULL;
        return error==PRO_TK_NOT_EXIST?PRO_TK_CANT_OPEN:error;
    }

    /* Transfer a freshly read source to its caller if teardown has not run yet. */
    for(int i=0;i<attachment_release_count;i++){
        if(attachment_release[i]==*model){
            attachment_release[i]=attachment_release[--attachment_release_count];
            *loaded=1;
            break;
        }
    }
    company_placement_source=source;
    company_placement_model=*model;
    return PRO_TK_NO_ERROR;
}
