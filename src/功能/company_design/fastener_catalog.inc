static int company_fastener_model_name(const wchar_t *name,size_t *model_chars,unsigned long *revision){
    const wchar_t *extension=wcsrchr(name,L'.');
    if(!extension||extension==name)return 0;
    *revision=0;
    if(extension[1]>=L'0'&&extension[1]<=L'9'){
        wchar_t *end;
        errno=0;
        *revision=wcstoul(extension+1,&end,10);
        if(*end||errno==ERANGE)return 0;
        while(--extension>name&&*extension!=L'.'){}
    }
    if(extension==name||(_wcsnicmp(extension,L".prt",4)&&_wcsnicmp(extension,L".asm",4)))return 0;
    if(extension[4]&&extension[4]!=L'.')return 0;
    *model_chars=(size_t)(extension+4-name);
    return 1;
}

static int company_fastener_catalog_add(CompanyFastenerCatalog *catalog,const wchar_t *path,
        size_t name_offset,size_t model_chars,unsigned long revision,UINT category){
    if(catalog->count==catalog->capacity){
        size_t capacity=catalog->capacity?catalog->capacity*2:64;
        if(capacity>=(UINT_MAX-COMPANY_FASTENER_MODEL_FIRST)/2)return 0;
        CompanyFastenerItem *items=realloc(catalog->items,capacity*sizeof(*items));
        if(!items)return 0;
        catalog->items=items;
        catalog->capacity=capacity;
    }
    CompanyFastenerItem *item=&catalog->items[catalog->count++];
    wcscpy_s(item->path,_countof(item->path),path);
    item->caption[0]=0;
    item->name_offset=name_offset;
    item->model_chars=model_chars;
    item->revision=revision;
    item->category=category;
    return 1;
}

static int company_fastener_catalog_scan(CompanyFastenerCatalog *catalog,const wchar_t *directory,
        UINT category,unsigned depth){
    wchar_t path[MAX_PATH],search[MAX_PATH];
    HANDLE dir=company_open_path(directory,1,path);
    if(dir==INVALID_HANDLE_VALUE)return 0;
    if(swprintf_s(search,_countof(search),L"%ls\\*",path)<0){CloseHandle(dir);return 0;}
    WIN32_FIND_DATAW entry;
    HANDLE found=FindFirstFileW(search,&entry);
    if(found==INVALID_HANDLE_VALUE){
        DWORD err=GetLastError();
        CloseHandle(dir);
        return err==ERROR_FILE_NOT_FOUND;
    }
    size_t offset=wcslen(path)+1;
    int complete=1;
    do{
        if(entry.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)continue;
        if(!wcscmp(entry.cFileName,L".")||!wcscmp(entry.cFileName,L".."))continue;
        size_t model_chars;
        unsigned long revision;
        int subfolder=(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
        if(!subfolder&&!company_fastener_model_name(entry.cFileName,&model_chars,&revision))continue;
        wchar_t child[MAX_PATH];
        if(swprintf_s(child,_countof(child),L"%ls\\%ls",path,entry.cFileName)<0){
            complete=0;
            continue;
        }
        if(subfolder){
            if(depth==32||!company_fastener_catalog_scan(catalog,child,category,depth+1))complete=0;
        }else if(!company_fastener_catalog_add(catalog,child,offset,model_chars,revision,category)){
            complete=0;
            break;
        }
    }while(FindNextFileW(found,&entry));
    if(GetLastError()!=ERROR_NO_MORE_FILES)complete=0;
    FindClose(found);
    CloseHandle(dir);
    return complete;
}

static int company_fastener_catalog_key(const CompanyFastenerItem *a,const CompanyFastenerItem *b){
    if(a->category!=b->category)return a->category<b->category?-1:1;
    int cmp=_wcsnicmp(a->path+a->name_offset,b->path+b->name_offset,
        min(a->model_chars,b->model_chars));
    if(cmp)return cmp;
    if(a->model_chars!=b->model_chars)return a->model_chars<b->model_chars?-1:1;
    cmp=_wcsnicmp(a->path,b->path,min(a->name_offset,b->name_offset));
    if(cmp||a->name_offset==b->name_offset)return cmp;
    return a->name_offset<b->name_offset?-1:1;
}

static int company_fastener_catalog_order(const void *first,const void *second){
    const CompanyFastenerItem *a=first,*b=second;
    int cmp=company_fastener_catalog_key(a,b);
    if(cmp)return cmp;
    if(a->revision!=b->revision)return a->revision>b->revision?-1:1;
    return _wcsicmp(b->path,a->path);
}

static void company_fastener_catalog_captions(CompanyFastenerCatalog *catalog,UINT category,
        const wchar_t *directory,const wchar_t *filename){
    wchar_t input[MAX_PATH],path[MAX_PATH];
    if(swprintf_s(input,_countof(input),L"%ls\\%ls",directory,filename)<0)return;
    HANDLE file=company_open_path(input,0,path);
    if(file==INVALID_HANDLE_VALUE)return;
    LARGE_INTEGER size;
    if(!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart>1024*1024){CloseHandle(file);return;}
    char *bytes=malloc((size_t)size.QuadPart);
    DWORD count=0;
    BOOL read_ok=bytes&&ReadFile(file,bytes,(DWORD)size.QuadPart,&count,NULL);
    CloseHandle(file);
    if(!read_ok||count!=(DWORD)size.QuadPart){free(bytes);return;}
    UINT encoding=936; /* The existing Creo library .mnu files are encoded in GBK. */
    const char *start=bytes;
    if(count>=3&&!memcmp(bytes,"\xef\xbb\xbf",3)){encoding=CP_UTF8;start+=3;count-=3;}
    int length=MultiByteToWideChar(encoding,MB_ERR_INVALID_CHARS,start,(int)count,NULL,0);
    wchar_t *content=length>0?malloc(((size_t)length+1)*sizeof(*content)):NULL;
    if(!content||MultiByteToWideChar(encoding,MB_ERR_INVALID_CHARS,start,(int)count,content,length)!=length){
        free(bytes);
        free(content);
        return;
    }
    free(bytes);
    content[length]=0;
    wchar_t *model=NULL;
    size_t model_chars=0;
    for(wchar_t *line=content;line&&*line;){
        wchar_t *next=wcschr(line,L'\n');
        if(next)*next++=0;
        while(*line==L' '||*line==L'\t')line++;
        size_t len=wcslen(line);
        while(len&&iswspace(line[len-1]))line[--len]=0;
        if(model){
            if(len&&*line!=L'#'){
                for(size_t i=0;i<catalog->count;i++){
                    CompanyFastenerItem *item=&catalog->items[i];
                    if(item->category==category&&item->model_chars==model_chars&&
                       !_wcsnicmp(item->path+item->name_offset,model,model_chars))
                        wcsncpy_s(item->caption,_countof(item->caption),line,_TRUNCATE);
                }
            }
            model=NULL;
        }
        unsigned long revision;
        if(company_fastener_model_name(line,&model_chars,&revision)&&model_chars==len)model=line;
        line=next;
    }
    free(content);
}

static void company_fastener_catalog_build(CompanyFastenerCatalog *catalog){
    wchar_t directories[_countof(native_company_fasteners)][MAX_PATH]={0};
    for(UINT i=0;i<_countof(native_company_fasteners);i++){
        if(!company_fastener_resolve(COMPANY_OPEN_FASTENER_FIRST+i,directories[i])){
            catalog->unavailable|=1u<<i;
            directories[i][0]=0;
        }else if(!company_fastener_catalog_scan(catalog,directories[i],i,0))
            catalog->incomplete|=1u<<i;
    }
    if(catalog->count>1){
        qsort(catalog->items,catalog->count,sizeof(*catalog->items),company_fastener_catalog_order);
        size_t unique=1;
        for(size_t i=1;i<catalog->count;i++){
            CompanyFastenerItem *item=&catalog->items[i];
            if(!company_fastener_catalog_key(&catalog->items[unique-1],item))continue;
            catalog->items[unique++]=*item;
        }
        catalog->count=unique;
    }
    for(UINT i=0;i<_countof(native_company_fasteners);i++){
        if(directories[i][0])
            company_fastener_catalog_captions(catalog,i,directories[i],native_company_fasteners[i].catalog_file);
    }
}