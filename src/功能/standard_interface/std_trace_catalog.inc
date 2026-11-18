
static ProError std_alias_add(const wchar_t *key,const wchar_t *rev,const wchar_t *name,PartText *out){
    StdRecord original={0};
    if(!_wcsicmp(key,name)||!or_safe_id(name))return PRO_TK_BAD_INPUTS;
    ProError error=std_lookup(key,rev,&original);
    if(error)return error;
    wchar_t canonical[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    if(!std_path(name,L"UNUSED",canonical,0))return PRO_TK_BAD_INPUTS;
    wchar_t *leaf=wcsrchr(canonical,L'\\');
    if(!leaf||swprintf_s(leaf+1,MAX_PATH-(leaf+1-canonical),L"%ls.*.iface.tsv",name)<0)return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW item;
    HANDLE same=FindFirstFileW(canonical,&item);
    if(same!=INVALID_HANDLE_VALUE){FindClose(same);return PRO_TK_E_FOUND;}
    if(!std_alias_path(name,1,path))return PRO_TK_CANT_WRITE;
    if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES){
        part_text_add(out,L"别名 %ls 已在标准接口库登记。可查已有归属。\n",name);
        return PRO_TK_E_FOUND;
    }
    FILE *file=ax_study_atomic(path,temp);
    if(!file)return PRO_TK_CANT_WRITE;
    fwprintf(file,L"BWTL-STD-ALIAS-1\t%ls\t%ls\t%ls\n",name,original.key,original.geometry.revision);
    if(!std_finish_new(file,temp,path))return PRO_TK_CANT_WRITE;
    part_text_add(out,L"已关联 %ls → %ls / %ls。\n",name,original.key,original.geometry.revision);
    return PRO_TK_NO_ERROR;
}

static ProError std_alias_query(const wchar_t *name,PartText *out){
    wchar_t key[41],version[41];
    if(!std_read_alias(name,key,version))return PRO_TK_E_NOT_FOUND;
    StdRecord record={0};
    ProError error=std_lookup(key,version,&record);
    if(!error)part_text_add(out,L"%ls 对应内部编号 %ls，修订 %ls，Creo 模型 %ls。\n",
        name,key,version,record.geometry.model);
    return error;
}

static int std_source_path(const StdRecord *entry,wchar_t path[MAX_PATH]){
    wchar_t folder[MAX_PATH];
    return std_dir(folder,0)&&swprintf_s(path,MAX_PATH,L"%ls\\%ls.%ls.source.tsv",folder,
        entry->key,entry->geometry.revision)>=0;
}

static int std_source_kind(const wchar_t *type){
    return !wcscmp(type,L"供应商图纸")||!wcscmp(type,L"原生建模")||!wcscmp(type,L"扫描")||
        !wcscmp(type,L"历史样件")||!wcscmp(type,L"公司图纸");
}

static ProError std_source_add(const wchar_t *key,const wchar_t *rev,const wchar_t *kind,
    const wchar_t *source_file,const wchar_t *note,PartText *out){
    if(!std_source_kind(kind)||!*note||wcslen(note)>=OR_CELL||wcspbrk(note,L"\t\r\n"))return PRO_TK_BAD_INPUTS;
    StdRecord record={0};
    ProError error=std_lookup(key,rev,&record);
    if(error)return error;
    wchar_t origin[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    if(!std_source_path(&record,path)||!or_real_path(source_file,0,origin)||wcslen(origin)>=OR_CELL||
       wcspbrk(origin,L"\t\r\n"))return PRO_TK_E_NOT_FOUND;
    wchar_t *end=wcsrchr(origin,L'\\');
    if(!end)return PRO_TK_BAD_INPUTS;
    *end=0;
    int correct_drive=storage_output_directory(origin);
    *end=L'\\';
    if(!correct_drive)return PRO_TK_BAD_CONTEXT;
    SYSTEMTIME t;
    GetLocalTime(&t);
    FILE *file=ax_study_atomic(path,temp);
    if(!file)return PRO_TK_CANT_WRITE;
    fwprintf(file,L"BWTL-STD-SOURCE-1\t%ls\t%04u-%02u-%02u\t%ls\t%ls\n",kind,t.wYear,t.wMonth,t.wDay,origin,note);
    if(!std_finish_new(file,temp,path))return PRO_TK_E_FOUND;
    part_text_add(out,L"%ls / %ls 的素材来源已登记：%ls。\n%ls\n",record.key,record.geometry.revision,kind,origin);
    return PRO_TK_NO_ERROR;
}

static ProError std_source_query(const wchar_t *key,const wchar_t *rev,PartText *out){
    StdRecord record={0};
    ProError error=std_lookup(key,rev,&record);
    if(error)return error;
    wchar_t path[MAX_PATH],resolved[MAX_PATH];
    OrTable contents={0};
    int ok=std_source_path(&record,path)&&or_table_load(&contents,path,5,out)&&contents.size==1&&
        !wcscmp(contents.rows[0].value[0],L"BWTL-STD-SOURCE-1");
    if(ok){
        OrRow *row=contents.rows;
        int present=or_real_path(row->value[3],0,resolved);
        part_text_add(out,L"%ls / %ls · %ls · %ls\n来源 %ls（当前%ls）\n说明 %ls\n",record.key,
            record.geometry.revision,row->value[1],row->value[2],row->value[3],present?L"可读取":L"无法访问",row->value[4]);
    }
    or_free(&contents);
    return ok?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}
