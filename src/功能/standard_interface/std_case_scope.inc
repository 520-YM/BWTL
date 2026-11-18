
static int std_scope_path(const wchar_t *name,wchar_t path[MAX_PATH],int create){
    wchar_t directory[MAX_PATH];
    return or_safe_id(name)&&std_dir(directory,create)&&swprintf_s(path,MAX_PATH,L"%ls\\%ls.scope.tsv",directory,name)>=0;
}

static int std_method_file(const wchar_t *kind,const wchar_t *model,wchar_t path[MAX_PATH]){
    if(!wcscmp(kind,L"装配"))return ax_study_casefile(model,path,0);
    if(wcscmp(kind,L"零件"))return 0;
    const wchar_t *suffix=L".bwtlmethod.txt";
    size_t n=wcslen(model),end=wcslen(suffix);
    if(n<=end||n>150||_wcsicmp(model+n-end,suffix))return 0;
    for(size_t i=0;i<n-end;i++)if(!ax_learning_word(model[i]))return 0;
    wchar_t directory[MAX_PATH];
    return storage_method_directory(directory,L"建模方法库",0)&&swprintf_s(path,MAX_PATH,L"%ls\\%ls",directory,model)>=0;
}

static ProError std_scope_add(const wchar_t *name,const wchar_t *kind,const wchar_t *model,const wchar_t *file,PartText *out){
    wchar_t method[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    if(!std_scope_path(name,path,1)||!std_method_file(kind,model,method))return PRO_TK_BAD_INPUTS;
    FILE *read=ax_learning_open(method);
    if(!read){part_text_add(out,L"找不到当前实例实际学习的案例文件。\n");return PRO_TK_E_NOT_FOUND;}
    wchar_t header[280];
    const wchar_t *prefix=!wcscmp(kind,L"装配")?L"BWTL-ASSEMBLY-RULES-3 ":L"BWTL-METHOD-1 ";
    int actual=fgetws(header,_countof(header),read)!=NULL;
    if(actual)actual=!wcsncmp(header[0]==0xfeff?header+1:header,prefix,wcslen(prefix));
    fclose(read);
    if(!actual)return PRO_TK_E_NOT_FOUND;
    OrTable table={0};
    ProError error=or_table_load(&table,file,7,out)?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    for(int i=0;!error&&i<table.size;i++){
        OrRow *r=&table.rows[i];
        double min,max;
        wchar_t evidence[MAX_PATH];
        if(!or_safe_id(r->value[0])||!or_safe_id(r->value[1])||!or_number(r->value[2],&min)||
            !or_number(r->value[3],&max)||min>max||min<0||!or_real_path(r->value[6],0,evidence)||
            wcslen(evidence)>=OR_CELL||towupper(evidence[0])!=towupper(runtime_data[0])){
            part_text_add(out,L"第 %u 行规格、范围或 E 盘验收证据无法核实。\n",r->line);
            error=PRO_TK_BAD_INPUTS;
        }else wcscpy_s(r->value[6],OR_CELL,evidence);
    }
    if(!error){
        FILE *write=ax_study_atomic(path,temp);
        if(!write)error=PRO_TK_CANT_WRITE;
        else{
            fwprintf(write,L"# BWTL-METHOD-SCOPE-1 %ls · %ls\n",kind,model);
            for(int i=0;i<table.size;i++)for(int j=0;j<7;j++)
                fwprintf(write,L"%ls%lc",table.rows[i].value[j],j==6?L'\n':L'\t');
            if(!std_finish_new(write,temp,path))error=PRO_TK_CANT_WRITE;
        }
    }
    if(!error)part_text_add(out,L"方法 %ls 已登记 %d 条实测范围、所需参照及失败条件。\n%ls\n",name,table.size,path);
    or_free(&table);
    return error;
}

static ProError std_scope_query(const wchar_t *name,const wchar_t *metric,double size,PartText *out){
    wchar_t path[MAX_PATH];
    if(!std_scope_path(name,path,0)||!or_safe_id(metric)||!isfinite(size)||size<0)return PRO_TK_BAD_INPUTS;
    OrTable table={0};
    if(!or_table_load(&table,path,7,out)){or_free(&table);return PRO_TK_E_NOT_FOUND;}
    int matches=0,seen=0;
    for(int i=0;i<table.size;i++){
        const OrRow *r=&table.rows[i];
        double min,max;
        if(_wcsicmp(r->value[1],metric)||!or_number(r->value[2],&min)||!or_number(r->value[3],&max))continue;
        seen++;
        if(size<min||size>max)continue;
        wchar_t path[MAX_PATH];
        if(!or_real_path(r->value[6],0,path)){
            part_text_add(out,L"%ls 的验收回执已无法读取，需重新验收。\n",r->value[0]);
            continue;
        }
        part_text_add(out,L"%ls，%.6g–%.6g mm。所需参照 %ls。已知失败条件 %ls。\n验收 %ls\n",
            r->value[0],min,max,r->value[4],r->value[5],path);
        matches++;
    }
    if(!matches)part_text_add(out,seen?L"超出已验收范围，或验收证据目前无法读取。\n":L"该方法尚无此项尺寸记录。\n");
    or_free(&table);
    return matches?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}
