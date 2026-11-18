
static int std_intake_matrix(const StdRecord *record,const wchar_t *candidate,wchar_t real[MAX_PATH]){
    wchar_t location[MAX_PATH],line[2048],fingerprint[240];
    if(!storage_method_directory(location,L"标准件规格试改",0)||!or_real_path(candidate,0,real)||!or_below(real,location))return 0;
    FILE *file=ax_learning_open(real);
    if(!file)return 0;
    OrRow cells={0};
    double total=0;
    int good=fgetws(line,_countof(line),file)&&or_columns(line[0]==0xfeff?line+1:line,&cells,5)&&
        !wcscmp(cells.value[0],L"BWTL-STANDARD-MATRIX-1")&&!_wcsicmp(cells.value[1],record->key)&&
        !_wcsicmp(cells.value[2],record->geometry.revision)&&
        MultiByteToWideChar(CP_UTF8,0,record->geometry.stamp,-1,fingerprint,_countof(fingerprint))&&
        !wcscmp(cells.value[3],fingerprint)&&or_number(cells.value[4],&total)&&floor(total)==total&&
        total>=2&&total<=STD_VARIANT_LIMIT;
    for(int i=0;good&&i<(int)total;i++){
        double status,n;
        StdDriver parameters[STD_VARIANT_LIMIT];
        good=fgetws(line,_countof(line),file)&&or_columns(line,&cells,7)&&or_safe_id(cells.value[0])&&
            std_drivers(cells.value[1],parameters)&&or_number(cells.value[2],&status)&&status==0&&
            or_number(cells.value[3],&n)&&n==record->geometry.count;
    }
    if(good){
        double passed,known,error;
        good=fgetws(line,_countof(line),file)&&or_columns(line,&cells,4)&&!wcscmp(cells.value[0],L"PASS")&&
            or_number(cells.value[1],&passed)&&or_number(cells.value[2],&known)&&or_number(cells.value[3],&error)&&
            passed==total&&known==total&&error==0&&!fgetws(line,_countof(line),file)&&!ferror(file);
    }
    fclose(file);
    return good;
}

static int std_intake_scope(const wchar_t *name,wchar_t path[MAX_PATH],PartText *out){
    OrTable table={0};
    if(!std_scope_path(name,path,0)||!or_table_load(&table,path,7,out)){or_free(&table);return 0;}
    int good=table.size>0;
    for(int i=0;good&&i<table.size;i++){
        double low,high;
        wchar_t real[MAX_PATH];
        const OrRow *row=&table.rows[i];
        good=or_number(row->value[2],&low)&&or_number(row->value[3],&high)&&high>=low&&
            or_real_path(row->value[6],0,real)&&towupper(real[0])==towupper(runtime_data[0]);
    }
    or_free(&table);
    return good;
}

static ProError std_intake_drawing(const StdRecord *record,const wchar_t *drawing,wchar_t path[MAX_PATH]){
    if(!or_real_path(drawing,0,path)||towupper(path[0])!=towupper(runtime_data[0]))return PRO_TK_E_NOT_FOUND;
    const wchar_t *leaf=wcsrchr(path,L'\\');
    leaf=leaf?leaf+1:path;
    const wchar_t *dot=wcschr(leaf,L'.');
    if(!dot||dot==leaf||dot-leaf>=PRO_NAME_SIZE||_wcsnicmp(dot,L".drw.",5)||!dot[5])return PRO_TK_BAD_INPUTS;
    for(const wchar_t *p=dot+5;*p;p++)if(*p<L'0'||*p>L'9')return PRO_TK_BAD_INPUTS;
    ProMdlName name;
    wcsncpy_s(name,_countof(name),leaf,dot-leaf);
    ProMdl object=NULL;
    int loaded=ProMdlnameInit(name,PRO_MDL_DRAWING,&object)!=PRO_TK_NO_ERROR;
    ProError error=loaded?ProMdlFiletypeLoad(path,PRO_MDLFILE_DRAWING,PRO_B_FALSE,&object):PRO_TK_NO_ERROR;
    ProPath disk;
    wchar_t absolute[MAX_PATH];
    if(!error)error=ProMdlOriginGet(object,disk);
    if(!error&&(!or_real_path(disk,0,absolute)||_wcsicmp(absolute,path)))error=PRO_TK_BAD_CONTEXT;
    ProSolid *linked=NULL;
    int n=0,verified=0;
    if(!error)error=ProDrawingSolidsCollect((ProDrawing)object,&linked);
    if(!error)error=ProArraySizeGet((ProArray)linked,&n);
    for(int i=0;!error&&i<n;i++){
        ProMdlName model;
        if(ProMdlMdlnameGet((ProMdl)linked[i],model)||_wcsicmp(model,record->geometry.model))continue;
        char stamp[240];
        if(!ax_study_verstamp((ProMdl)linked[i],stamp)&&!strcmp(stamp,record->geometry.stamp))verified=1;
    }
    if(linked)ProArrayFree((ProArray *)&linked);
    if(loaded&&object)error=std_variant_erase(&object,error);
    return error?error:verified?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError std_intake(const wchar_t *key,const wchar_t *rev,const wchar_t *scope,const wchar_t *matrix,
    const wchar_t *drawing,PartText *out){
    StdRecord entry={0};
    ProError error=std_lookup(key,rev,&entry);
    if(error){part_text_add(out,L"缺少该图号和修订的标准接口登记。\n");return error;}
    wchar_t scope_path[MAX_PATH],matrix_path[MAX_PATH],drawing_path[MAX_PATH];
    if(!std_intake_scope(scope,scope_path,out)){
        part_text_add(out,L"学习方法适用范围或验收证据仍缺失。\n");
        return PRO_TK_INCOMPLETE;
    }
    if(!std_intake_matrix(&entry,matrix,matrix_path)){
        part_text_add(out,L"缺少同编号、同修订、同模型版本且全部通过的规格试改回执。\n");
        return PRO_TK_INCOMPLETE;
    }
    error=std_intake_drawing(&entry,drawing,drawing_path);
    if(error){
        part_text_add(out,L"工程图须位于数据盘，并实际关联已登记的零件版本。\n");
        return error;
    }
    wchar_t dir[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    if(!std_dir(dir,0)||swprintf_s(path,_countof(path),L"%ls\\%ls.%ls.approved.tsv",dir,entry.key,entry.geometry.revision)<0)
        return PRO_TK_CANT_WRITE;
    if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return PRO_TK_E_FOUND;
    FILE *file=ax_study_atomic(path,temp);
    if(!file)return PRO_TK_CANT_WRITE;
    fwprintf(file,L"BWTL-STANDARD-APPROVAL-1\t%ls\t%ls\t%ls\t%hs\n范围\t%ls\n试改回执\t%ls\n工程图\t%ls\n",
        entry.key,entry.geometry.revision,entry.geometry.model,entry.geometry.stamp,scope_path,matrix_path,drawing_path);
    if(!std_finish_new(file,temp,path))return PRO_TK_CANT_WRITE;
    part_text_add(out,L"标准件 %ls / %ls 已通过入库检查。已登记原生参数、修订、验收、工程图和适用范围。\n%ls\n",
        entry.key,entry.geometry.revision,path);
    return PRO_TK_NO_ERROR;
}
