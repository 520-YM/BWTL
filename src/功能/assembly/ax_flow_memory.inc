
static int ax_flow_memory(const wchar_t *source,const wchar_t *label,const wchar_t *tail,
    wchar_t output[MAX_PATH],int create){
    wchar_t library[MAX_PATH];
    return or_safe_id(source)&&or_safe_id(label)&&ax_learning_directory(library,create)&&
        swprintf_s(output,MAX_PATH,L"%ls\\%ls-%ls.%ls.tsv",library,source,label,tail)>=0;
}

static ProError ax_flow_direction_save(ProMdl active,const wchar_t *source,const wchar_t *word,
    const wchar_t *token,const wchar_t *axis,PartText *out){
    if(!active||wcslen(axis)!=2||(axis[0]!=L'+'&&axis[0]!=L'-')||axis[1]<L'X'||axis[1]>L'Z'||
        wcsncmp(token,L"CSYS:",5)||wcslen(token)>80||wcspbrk(token,L"\t\r\n"))return PRO_TK_BAD_INPUTS;
    wchar_t casefile[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    if(!ax_study_casefile(source,casefile,0)||GetFileAttributesW(casefile)==INVALID_FILE_ATTRIBUTES||
       !ax_flow_memory(source,word,L"direction",path,1))return PRO_TK_E_NOT_FOUND;
    FdRef item={0};
    ProError error=fd_ref_resolve(active,token,PRO_CSYS,&item);
    fd_ref_free(&item);
    if(error)return error;
    FILE *write=ax_study_atomic(path,temp);
    if(!write)return PRO_TK_CANT_WRITE;
    fwprintf(write,L"%ls\t%ls\n",token,axis);
    if(!ax_study_commit(write,temp,path))return PRO_TK_CANT_WRITE;
    part_text_add(out,L"%ls 的“%ls”已绑定到 %ls 的 %ls。\n",source,word,token,axis);
    return PRO_TK_NO_ERROR;
}

static ProError ax_flow_direction_get(ProMdl active,const wchar_t *source,const wchar_t *word,
    const wchar_t *target,PartText *out){
    wchar_t path[MAX_PATH];
    OrTable table={0};
    if(!active||!ax_flow_memory(source,word,L"direction",path,0))return PRO_TK_E_NOT_FOUND;
    if(!or_table_load(&table,path,2,out)||table.size!=1){or_free(&table);return PRO_TK_E_NOT_FOUND;}
    wchar_t token[OR_CELL],axis[OR_CELL];
    wcscpy_s(token,_countof(token),table.rows[0].value[0]);
    wcscpy_s(axis,_countof(axis),table.rows[0].value[1]);
    or_free(&table);
    if(wcslen(axis)!=2||(axis[0]!=L'+'&&axis[0]!=L'-')||axis[1]<L'X'||axis[1]>L'Z')return PRO_TK_BAD_INPUTS;
    AxComponent occurrence={0};
    int component=target&&*target&&wcscmp(target,L"ROOT");
    ProError error=PRO_TK_NO_ERROR;
    if(component)error=ax_component_resolve(active,target,&occurrence);
    else occurrence.model=active;
    FdRef item={0};
    ProCsys id;
    ProGeomitemdata *geom=NULL;
    if(!error)error=fd_ref_resolve(occurrence.model,token,PRO_CSYS,&item);
    if(!error)error=ProGeomitemToCsys(&item.item,&id);
    if(!error)error=ProCsysDataGet(id,&geom);
    ProVector v={0};
    if(!error){
        ProCsysdata *value=geom->data.p_csys_data;
        const double *from=axis[1]==L'X'?value->x_vector:axis[1]==L'Y'?value->y_vector:value->z_vector;
        for(int i=0;i<3;i++)v[i]=(axis[0]==L'-'?-1:1)*from[i];
        if(component){
            ProMatrix pose;
            ProVector result;
            error=ProAsmcomppathTrfGet(&occurrence.path,PRO_B_TRUE,pose);
            if(!error)error=ProVectorTrfEval(v,pose,result);
            if(!error)memcpy(v,result,sizeof(v));
        }
    }
    if(!error)part_text_add(out,L"%ls 在目标 %ls 上取 %ls 的 %ls，换算到当前模型方向为 (%.7g, %.7g, %.7g)。\n",
        word,component?target:L"ROOT",token,axis,v[0],v[1],v[2]);
    if(geom)ProGeomitemdataFree(&geom);
    fd_ref_free(&item);
    return error;
}

static int ax_flow_revision_snapshot(const wchar_t *file,wchar_t destination[MAX_PATH]){
    return swprintf_s(destination,MAX_PATH,L"%ls.rules",file)>=0;
}

static int ax_flow_rules_summary(const wchar_t *path,int *total,int *positioned,int *limited){
    FILE *in=ax_learning_open(path);
    if(!in)return 0;
    wchar_t line[850];
    int found=0;
    while(fgetws(line,_countof(line),in)){
        if(swscanf_s(line,L"END %d %d %d",total,positioned,limited)==3){found=1;break;}
    }
    fclose(in);
    return found;
}

static int ax_flow_rule_delta(const wchar_t *previous,const wchar_t *current,int *first_line){
    FILE *old=ax_learning_open(previous),*now=ax_learning_open(current);
    int same=-1;
    *first_line=0;
    if(old&&now){
        wchar_t a[850],b[850];
        while(1){
            wchar_t *left=fgetws(a,_countof(a),old),*right=fgetws(b,_countof(b),now);
            (*first_line)++;
            if(!left&&!right){if(!ferror(old)&&!ferror(now))same=1;break;}
            if(!left||!right||wcscmp(a,b)){same=0;break;}
        }
    }
    if(old)fclose(old);
    if(now)fclose(now);
    return same;
}

static ProError ax_flow_revision_note(ProMdl active,const wchar_t *source,const wchar_t *reason,PartText *out){
    if(!active||!*reason||wcslen(reason)>=OR_CELL||wcspbrk(reason,L"\r\n\t"))return PRO_TK_BAD_INPUTS;
    wchar_t learning[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    ProMdlName current;
    ProError error=ProMdlMdlnameGet(active,current);
    if(error)return error;
    if(_wcsicmp(source,current)||!ax_study_casefile(source,learning,0)||
       GetFileAttributesW(learning)==INVALID_FILE_ATTRIBUTES)return PRO_TK_BAD_CONTEXT;
    FILETIME proof;
    char stamp[240];
    if(!ax_study_case_stamp(source,&proof))return PRO_TK_E_NOT_FOUND;
    if((error=ax_study_verstamp(active,stamp)))return error;
    SYSTEMTIME now;
    GetLocalTime(&now);
    static LONG serial;
    wchar_t key[64],snapshot[MAX_PATH];
    swprintf_s(key,_countof(key),L"%04u%02u%02u-%02u%02u%02u%03u-%08lx-%08lx",
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,
        GetCurrentProcessId(),(unsigned long)InterlockedIncrement(&serial));
    if(!ax_flow_memory(source,key,L"revision",path,1)||!ax_flow_revision_snapshot(path,snapshot)||
        !CopyFileW(learning,snapshot,TRUE))return PRO_TK_CANT_WRITE;
    FILETIME after;
    if(!ax_study_case_stamp(source,&after)||CompareFileTime(&proof,&after)){
        DeleteFileW(snapshot);
        return PRO_TK_BAD_CONTEXT;
    }
    FILE *write=ax_study_atomic(path,temp);
    if(!write){DeleteFileW(snapshot);return PRO_TK_CANT_WRITE;}
    fwprintf(write,L"%ls\t%hs\t%lu-%lu\t%ls\n",key,stamp,proof.dwHighDateTime,proof.dwLowDateTime,reason);
    if(!ax_study_commit(write,temp,path)){DeleteFileW(snapshot);return PRO_TK_CANT_WRITE;}
    part_text_add(out,L"装配学习修订说明已保存。%ls，模型版本 %hs。\n%ls\n",key,stamp,path);
    return PRO_TK_NO_ERROR;
}

static int ax_flow_note_order(const void *left,const void *right){return _wcsicmp(left,right);}

static ProError ax_flow_history(const wchar_t *source,PartText *out){
    wchar_t folder[MAX_PATH],pattern[MAX_PATH];
    if(!or_safe_id(source)||!ax_learning_directory(folder,0)||
       swprintf_s(pattern,MAX_PATH,L"%ls\\%ls-*.revision.tsv",folder,source)<0)return PRO_TK_E_NOT_FOUND;
    WIN32_FIND_DATAW data;
    HANDLE search=FindFirstFileW(pattern,&data);
    if(search==INVALID_HANDLE_VALUE)return PRO_TK_E_NOT_FOUND;
    typedef wchar_t NoteFile[MAX_PATH];
    NoteFile *names=calloc(OR_ROWS,sizeof(*names));
    if(!names){FindClose(search);return PRO_TK_OUT_OF_MEMORY;}
    int count=0,truncated=0;
    do{
        if(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
        if(count==OR_ROWS){truncated=1;break;}
        wcscpy_s(names[count++],MAX_PATH,data.cFileName);
    }while(FindNextFileW(search,&data));
    FindClose(search);
    qsort(names,count,sizeof(*names),ax_flow_note_order);
    int shown=0;
    wchar_t previous[MAX_PATH]=L"";
    for(int i=0;i<count;i++){
        wchar_t path[MAX_PATH];
        OrTable table={0};
        if(swprintf_s(path,MAX_PATH,L"%ls\\%ls",folder,names[i])>=0&&
           or_table_load(&table,path,4,out)&&table.size==1){
            OrRow *row=&table.rows[0];
            part_text_add(out,L"%ls，Creo %ls，学习文件标记 %ls，原因 %ls。\n",
                row->value[0],row->value[1],row->value[2],row->value[3]);
            wchar_t snapshot[MAX_PATH];
            int total,positioned,limited;
            if(ax_flow_revision_snapshot(path,snapshot)&&ax_flow_rules_summary(snapshot,&total,&positioned,&limited)){
                part_text_add(out,L"  学习验收：%d/%d 个实例保有完整定位条件%s。\n",
                    positioned,total,limited?L"，案例扫描有截断":L"");
                if(*previous){
                    int line=0;
                    int same=ax_flow_rule_delta(previous,snapshot,&line);
                    if(same==1)part_text_add(out,L"  与上一份规则内容一致。\n");
                    if(same==0)part_text_add(out,L"  与上一份规则在第 %d 行开始不同，查看快照 %ls。\n",line,snapshot);
                }
                wcscpy_s(previous,_countof(previous),snapshot);
            }
            shown++;
        }
        or_free(&table);
    }
    free(names);
    if(truncated)part_text_add(out,L"历史超过读取上限。其余可在装配方法库打开。\n");
    part_text_add(out,L"显示 %d 条修订说明。\n",shown);
    return shown?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}
