#define NATIVE_ATTACHMENT_MAX 6
#define NATIVE_FILE_MAX_BYTES (100ULL * 1024 * 1024)

typedef enum {ATTACH_NONE, ATTACH_PDF, ATTACH_STEP, ATTACH_PART, ATTACH_ASSEMBLY, ATTACH_TEXT} AttachmentKind;
typedef struct {
    wchar_t source[MAX_PATH], path[MAX_PATH], name[MAX_PATH];
    AttachmentKind kind;
    FILETIME modified;
    LARGE_INTEGER size;
} NativeFile;

static NativeFile native_files[NATIVE_ATTACHMENT_MAX];
static int native_file_count;

static const wchar_t *native_file_suffix(const wchar_t *name){
    const wchar_t *last=wcsrchr(name,L'.');
    if(last&&last[1]&&wcsspn(last+1,L"0123456789")==wcslen(last+1)){
        const wchar_t *previous=last;
        while(previous>name&&*--previous!=L'.'){}
        if(*previous==L'.'&&(!_wcsnicmp(previous,L".prt.",5)||!_wcsnicmp(previous,L".asm.",5)))return previous;
    }
    return last;
}
static AttachmentKind native_file_kind(const wchar_t *suffix){
    if(!suffix)return ATTACH_NONE;
    if(!_wcsicmp(suffix,L".pdf"))return ATTACH_PDF;
    if(!_wcsicmp(suffix,L".step")||!_wcsicmp(suffix,L".stp"))return ATTACH_STEP;
    if(!_wcsicmp(suffix,L".prt")||!_wcsnicmp(suffix,L".prt.",5))return ATTACH_PART;
    if(!_wcsicmp(suffix,L".asm")||!_wcsnicmp(suffix,L".asm.",5))return ATTACH_ASSEMBLY;
    if(!_wcsicmp(suffix,L".txt")||!_wcsicmp(suffix,L".csv")||!_wcsicmp(suffix,L".tsv")||
       !_wcsicmp(suffix,L".md")||!_wcsicmp(suffix,L".json")||!_wcsicmp(suffix,L".log"))return ATTACH_TEXT;
    return ATTACH_NONE;
}
static int native_file_is_image(const wchar_t *suffix){
    static const wchar_t *const image_ext[]={L".png",L".jpg",L".jpeg",L".bmp",L".webp",L".gif",L".tif",L".tiff"};
    if(suffix)for(int i=0;i<_countof(image_ext);i++)if(!_wcsicmp(suffix,image_ext[i]))return 1;
    return 0;
}
static int native_attachment_count(void){
    AcquireSRWLockShared(&native_image_lock);
    int count=native_image_count+native_file_count;
    ReleaseSRWLockShared(&native_image_lock);
    return count;
}
static int native_attachment_import(const wchar_t *path,wchar_t *error,size_t cap){
    NativeFile item={0};
    DWORD length=GetFullPathNameW(path,MAX_PATH,item.source,NULL);
    if(!length||length>=MAX_PATH){
        wcscpy_s(error,cap,L"文件路径太长或不可读取。");
        return 0;
    }
    const wchar_t *name=wcsrchr(item.source,L'\\');
    name=name?name+1:item.source;
    const wchar_t *suffix=native_file_suffix(name);
    if(native_attachment_count()>=NATIVE_ATTACHMENT_MAX){
        wcscpy_s(error,cap,L"一条消息最多附 6 个文件。");
        return 0;
    }
    if(native_file_is_image(suffix))return native_image_import(item.source,error,cap);
    item.kind=native_file_kind(suffix);
    if(!item.kind){
        wcscpy_s(error,cap,L"支持 PDF、STEP/STP、PRT、ASM、图片、TXT、CSV、TSV、MD、JSON 和 LOG。");
        return 0;
    }
    if(!native_attachment_root[0]){
        wcscpy_s(error,cap,L"附件目录尚未就绪。");
        return 0;
    }
    HANDLE source=CreateFileW(item.source,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(source==INVALID_HANDLE_VALUE){
        wcscpy_s(error,cap,L"文件无法读取，请核对权限或是否被占用。");
        return 0;
    }
    int valid=GetFileSizeEx(source,&item.size)&&GetFileTime(source,NULL,NULL,&item.modified);
    if(!valid||item.size.QuadPart<=0||(ULONGLONG)item.size.QuadPart>NATIVE_FILE_MAX_BYTES){
        CloseHandle(source);
        wcscpy_s(error,cap,L"文件为空，或超过单个附件 100 MB 的限制。");
        return 0;
    }
    wcscpy_s(item.name,_countof(item.name),name);
    valid=swprintf_s(item.path,_countof(item.path),L"%ls\\file-%lu-%llu-%ld%ls",native_attachment_root,
        GetCurrentProcessId(),GetTickCount64(),InterlockedIncrement(&native_image_serial),suffix)>=0&&
        CopyFileW(item.source,item.path,TRUE);
    CloseHandle(source);
    if(!valid){
        wcscpy_s(error,cap,L"无法将附件存到 BWTL 会话目录，请检查 E 盘空间。");
        return 0;
    }
    AcquireSRWLockExclusive(&native_image_lock);
    native_files[native_file_count++]=item;
    ReleaseSRWLockExclusive(&native_image_lock);
    error[0]=0;
    return 1;
}
static int native_files_snapshot(NativeFile out[NATIVE_ATTACHMENT_MAX]){
    AcquireSRWLockShared(&native_image_lock);
    int count=native_file_count;
    memcpy(out,native_files,count*sizeof(*out));
    ReleaseSRWLockShared(&native_image_lock);
    return count;
}
static void native_attachments_clear(void){
    native_images_clear();
    AcquireSRWLockExclusive(&native_image_lock);
    native_file_count=0;
    ReleaseSRWLockExclusive(&native_image_lock);
}