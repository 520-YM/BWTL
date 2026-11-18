#define COMPANY_PATH_ROOTS 8
#define COMPANY_SCAN_LIMIT 16000
#define COMPANY_LIST_LIMIT 72

static wchar_t company_roots[COMPANY_PATH_ROOTS][MAX_PATH];
static int company_root_count;

static int company_match(const wchar_t *text,const wchar_t *word){
    size_t count=wcslen(word);
    for(;*text;text++)if(!_wcsnicmp(text,word,count))return 1;
    return !count;
}

static int company_request(void){
    static const wchar_t *const terms[]={
        L"公司标准",L"标准文件",L"公司资料",L"技术文件",L"公司库",L"标准库",L"资料库",L"标准件",L"紧固件",L"电子件",
        L"压铆件",L"螺母柱",L"压铆螺母",
        L"库里",L"库中",L"设计规范",L"标准样件",L"库模型"
    };
    for(int i=0;i<_countof(terms);i++)if(wcsstr(smart_latest_request,terms[i]))return 1;
    return company_match(smart_latest_request,L"BSO");
}

static int company_path_in_request(const wchar_t *path){
    size_t length=wcslen(path);
    if(length<4)return 0;
    for(const wchar_t *p=smart_latest_request;*p;p++){
        if(p!=smart_latest_request&&((p[-1]<=127&&iswalnum(p[-1]))||wcschr(L"/\\:",p[-1])))continue;
        size_t n=0;
        while(n<length&&p[n]&&
            (towupper(p[n])==towupper(path[n])||
            ((p[n]==L'/'||p[n]==L'\\')&&(path[n]==L'/'||path[n]==L'\\'))))n++;
        if(n!=length)continue;
        const wchar_t *after=p+n;
        if(*after==L'\\'||*after==L'/')after++;
        while(*after==L' '||*after==L'\t')after++;
        if(!*after||wcschr(L"\r\n\"'“”‘’「」【】，。；、！？）",*after))return 1;
    }
    return 0;
}

static void company_trim_path(wchar_t *path){
    size_t length=wcslen(path);
    while(length>3&&(path[length-1]==L'\\'||path[length-1]==L'/'))path[--length]=0;
}

static int company_is_under(const wchar_t *path,const wchar_t *root){
    size_t length=wcslen(root);
    return length&&!_wcsnicmp(path,root,length)&&(!path[length]||path[length]==L'\\');
}

static HANDLE company_open_path(const wchar_t *input,int directory,wchar_t path[MAX_PATH]){
    const wchar_t drive=storage_output_drive()?storage_output_drive():towupper(runtime_data[0]);
    size_t length=wcslen(input);
    if(length<4||length>=MAX_PATH||towupper(input[0])!=drive||input[1]!=L':'||
       (input[2]!=L'\\'&&input[2]!=L'/')||wcspbrk(input+2,L":*?|\r\n"))return INVALID_HANDLE_VALUE;
    DWORD normalized=GetFullPathNameW(input,MAX_PATH,path,NULL);
    if(normalized<4||normalized>=MAX_PATH)return INVALID_HANDLE_VALUE;
    company_trim_path(path);
    HANDLE handle=CreateFileW(path,directory?FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES:GENERIC_READ,FILE_SHARE_READ,
        NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|(directory?FILE_FLAG_BACKUP_SEMANTICS:0),NULL);
    if(handle==INVALID_HANDLE_VALUE)return handle;
    BY_HANDLE_FILE_INFORMATION info;
    wchar_t final_path[MAX_PATH+4];
    DWORD final_length=GetFinalPathNameByHandleW(handle,final_path,_countof(final_path),VOLUME_NAME_DOS);
    if(!final_length||final_length>=_countof(final_path)||wcsncmp(final_path,L"\\\\?\\",4)||
       _wcsicmp(final_path+4,path)||GetFileType(handle)!=FILE_TYPE_DISK||
       !GetFileInformationByHandle(handle,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
       (!!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=!!directory)){
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }
    return handle;
}

static int company_default_root(wchar_t root[MAX_PATH]){
    wchar_t parent[MAX_PATH];
    wcscpy_s(parent,_countof(parent),runtime_data);
    company_trim_path(parent);
    wchar_t *leaf=wcsrchr(parent,L'\\');
    if(leaf&&!_wcsicmp(leaf+1,L"data"))*leaf=0;
    return swprintf_s(root,MAX_PATH,L"%ls\\公司资料",parent)>=0;
}

static int company_fastener_primary_root(wchar_t root[MAX_PATH]);

static int company_registered_path(const wchar_t *path){
    wchar_t root[MAX_PATH];
    return (company_default_root(root)&&company_is_under(path,root))||
        (company_fastener_primary_root(root)&&company_is_under(path,root));
}

static HANDLE company_open_root(const wchar_t *input,wchar_t path[MAX_PATH]){
    wchar_t root[MAX_PATH];
    if(!company_default_root(root))return INVALID_HANDLE_VALUE;
    if(!*input||!wcscmp(input,L"公司库")||!wcscmp(input,L"公司标准库")||
       !wcscmp(input,L"标准库")||!wcscmp(input,L"公司资料")||!wcscmp(input,L"默认公司库")){
        if(!company_request())return INVALID_HANDLE_VALUE;
        wchar_t primary[MAX_PATH],verified[MAX_PATH];
        if(company_fastener_primary_root(primary)){
            HANDLE shared=company_open_path(primary,1,verified);
            if(shared!=INVALID_HANDLE_VALUE){
                CloseHandle(shared);
                wcscpy_s(root,_countof(root),verified);
            }
        }
        input=root;
    }
    if(!company_path_in_request(input)&&(!company_request()||!company_registered_path(input))){
        int granted=0;
        for(int i=0;i<company_root_count;i++)if(company_is_under(input,company_roots[i]))granted=1;
        if(!granted)return INVALID_HANDLE_VALUE;
    }
    HANDLE handle=company_open_path(input,1,path);
    if(handle==INVALID_HANDLE_VALUE)return handle;
    /* Check the canonical spelling as well; a relative dot segment never grants a sibling. */
    if(!company_path_in_request(input)&&(!company_request()||!company_registered_path(path))){
        int granted=0;
        for(int i=0;i<company_root_count;i++)if(company_is_under(path,company_roots[i]))granted=1;
        if(!granted){CloseHandle(handle);return INVALID_HANDLE_VALUE;}
    }
    for(int i=0;i<company_root_count;i++)if(company_is_under(path,company_roots[i]))return handle;
    if(company_root_count<COMPANY_PATH_ROOTS)
        wcscpy_s(company_roots[company_root_count++],MAX_PATH,path);
    return handle;
}

static HANDLE company_open_document(const wchar_t *input,const wchar_t *root,wchar_t path[MAX_PATH]){
    if(*root){
        wchar_t scope[MAX_PATH];
        HANDLE directory=company_open_root(root,scope);
        if(directory==INVALID_HANDLE_VALUE)return directory;
        CloseHandle(directory);
    }
    HANDLE file=company_open_path(input,0,path);
    if(file==INVALID_HANDLE_VALUE)return file;
    if(company_path_in_request(input))return file;
    if(company_request()&&company_registered_path(path))return file;
    for(int i=0;i<company_root_count;i++)if(company_is_under(path,company_roots[i]))return file;
    CloseHandle(file);
    return INVALID_HANDLE_VALUE;
}

typedef struct {
    unsigned visited,shown,skipped;
    int bounded,recursive;
    const wchar_t *query;
    PartText *out;
} CompanyScan;

static int company_known_file(const wchar_t *name){
    const wchar_t *suffix=native_file_suffix(name);
    if(native_file_kind(suffix)||native_file_is_image(suffix))return 1;
    return suffix&&(!_wcsicmp(suffix,L".docx")||!_wcsicmp(suffix,L".xlsx")||
                    !_wcsicmp(suffix,L".doc")||!_wcsicmp(suffix,L".xls"));
}

static void company_scan_dir(const wchar_t *folder,CompanyScan *scan,int depth){
    wchar_t pattern[MAX_PATH],path[MAX_PATH];
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\*",folder)<0){scan->skipped++;return;}
    WIN32_FIND_DATAW entry;
    HANDLE search=FindFirstFileW(pattern,&entry);
    if(search==INVALID_HANDLE_VALUE){
        if(GetLastError()!=ERROR_FILE_NOT_FOUND)scan->skipped++;
        return;
    }
    do{
        if(scan->visited++>=COMPANY_SCAN_LIMIT||scan->shown>=COMPANY_LIST_LIMIT||scan->out->truncated){
            scan->bounded=1;break;
        }
        const wchar_t *name=entry.cFileName;
        if(!wcscmp(name,L".")||!wcscmp(name,L".."))continue;
        if(entry.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_SYSTEM)){scan->skipped++;continue;}
        if(swprintf_s(path,_countof(path),L"%ls\\%ls",folder,name)<0){scan->skipped++;continue;}
        int is_dir=(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
        if((is_dir||company_known_file(name))&&company_match(name,scan->query)){
            part_text_add(scan->out,L"%ls %ls\n",is_dir?L"[目录]":L"[文件]",path);
            scan->shown++;
        }
        if(is_dir&&scan->recursive&&depth<8){
            wchar_t resolved[MAX_PATH];
            HANDLE child=company_open_path(path,1,resolved);
            if(child==INVALID_HANDLE_VALUE)scan->skipped++;
            else{company_scan_dir(resolved,scan,depth+1);CloseHandle(child);}
        }else if(is_dir&&scan->recursive&&depth==8)scan->bounded=1;
        if(scan->visited>=COMPANY_SCAN_LIMIT||scan->shown>=COMPANY_LIST_LIMIT||scan->out->truncated){
            scan->bounded=1;break;
        }
    }while(FindNextFileW(search,&entry));
    if(!scan->bounded&&GetLastError()!=ERROR_NO_MORE_FILES)scan->skipped++;
    FindClose(search);
}

static ProError company_list(const wchar_t *root,const wchar_t *query,int recursive,PartText *out){
    wchar_t path[MAX_PATH];
    if(wcslen(query)>120||(recursive&&!*query)){
        part_text_add(out,L"递归搜索需要明确的文件名、型号或目录名关键词（最多 120 字）。\n");
        return PRO_TK_BAD_INPUTS;
    }
    HANDLE directory=company_open_root(root,path);
    if(directory==INVALID_HANDLE_VALUE){
        part_text_add(out,L"打不开已授权的资料目录。请核对路径，或在消息中用引号写出要浏览的完整数据盘目录。\n");
        return PRO_TK_CANT_OPEN;
    }
    part_text_add(out,L"资料目录：%ls\n",path);
    CompanyScan scan={0};
    scan.out=out;scan.query=query;scan.recursive=recursive;
    company_scan_dir(path,&scan,0);
    CloseHandle(directory);
    part_text_add(out,L"显示 %u 项。%ls%ls\n",scan.shown,
        scan.bounded?L"达到读取范围，请缩小目录或关键词。":L"",
        scan.skipped?L"链接、路径过长或无访问权限的条目已略过。":L"");
    return PRO_TK_NO_ERROR;
}

static ProError company_document_stage(HANDLE file,const wchar_t *source,PartText *out){
    const wchar_t *suffix=wcsrchr(source,L'.');
    static LONG serial;
    LARGE_INTEGER size;
    if(!suffix||!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart>40LL*1024*1024){
        part_text_add(out,L"资料为空或超过单份 40 MB 限制。\n");
        return PRO_TK_BAD_INPUTS;
    }
    const AttachmentKind kind=native_file_kind(suffix);
    if(kind!=ATTACH_TEXT&&kind!=ATTACH_PDF&&_wcsicmp(suffix,L".docx")&&_wcsicmp(suffix,L".xlsx")){
        part_text_add(out,L"资料正文支持 PDF、DOCX、XLSX 和常见文本。Creo 原生模型请用模型读取。旧版 DOC/XLS 可另存为 DOCX/XLSX 或 PDF。\n");
        return PRO_TK_UNSUPPORTED;
    }
    wchar_t leaf[80],destination[MAX_PATH];
    if(!storage_output_directory(ai_dir)||
       swprintf_s(leaf,_countof(leaf),L"company-doc-%lu-%ld%ls",resident_sequence,InterlockedIncrement(&serial),suffix)<0||
       !resident_path(destination,leaf))return PRO_TK_CANT_WRITE;
    HANDLE copy=CreateFileW(destination,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(copy==INVALID_HANDLE_VALUE)return PRO_TK_CANT_WRITE;
    char bytes[16384];
    LONGLONG remaining=size.QuadPart;
    int ok=1;
    while(remaining){
        DWORD got=0,wrote=0;
        const DWORD request=remaining>sizeof(bytes)?sizeof(bytes):(DWORD)remaining;
        if(!ReadFile(file,bytes,request,&got,NULL)||!got||!WriteFile(copy,bytes,got,&wrote,NULL)||wrote!=got){
            ok=0;break;
        }
        remaining-=got;
    }
    CloseHandle(copy);
    if(!ok){DeleteFileW(destination);return PRO_TK_CANT_WRITE;}
    part_text_add(out,L"BWTL_LIBRARY_DOCUMENT\n%ls\n%ls",leaf,source);
    return PRO_TK_NO_ERROR;
}
