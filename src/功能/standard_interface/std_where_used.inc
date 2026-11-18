
static int std_assembly_name(const wchar_t *file,ProMdlName name){
    const wchar_t *leaf=wcsrchr(file,L'\\');
    leaf=leaf?leaf+1:file;
    const wchar_t *extension=wcschr(leaf,L'.');
    if(!extension||extension==leaf||extension-leaf>=PRO_NAME_SIZE||_wcsnicmp(extension,L".asm.",5))return 0;
    const wchar_t *revision=extension+5;
    if(!*revision)return 0;
    for(const wchar_t *s=revision;*s;s++)if(*s<L'0'||*s>L'9')return 0;
    wcsncpy_s(name,PRO_NAME_SIZE,leaf,extension-leaf);
    return ax_study_key(name);
}

static ProError std_where_one(const OrRow *project,const StdRecord *record,FILE *receipt,int *hits,PartText *out){
    wchar_t file[MAX_PATH],located[MAX_PATH];
    ProMdlName name;
    if(!or_real_path(project->value[2],0,file)||towupper(file[0])!=towupper(runtime_data[0])||
        !std_assembly_name(file,name)||wcspbrk(file,L"\r\n\t"))return PRO_TK_BAD_INPUTS;
    ProMdl assembly=NULL;
    int loaded=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&assembly)!=PRO_TK_NO_ERROR;
    ProError error=loaded?ProMdlFiletypeLoad(file,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&assembly):PRO_TK_NO_ERROR;
    ProPath origin;
    if(!error)error=ProMdlOriginGet(assembly,origin);
    if(!error&&(!or_real_path(origin,0,located)||_wcsicmp(located,file))){
        part_text_add(out,L"%ls 已打开的版本或路径与清单不一致。\n",name);
        error=PRO_TK_BAD_CONTEXT;
    }
    AxScan *scan=NULL;
    if(!error)error=ax_scan(assembly,0,&scan);
    if(!error&&(scan->limited||scan->suppressed)){
        part_text_add(out,L"%ls 的实例树有抑制或截断，需打开完整表示再检查。\n",name);
        error=PRO_TK_INCOMPLETE;
    }
    for(int i=0;!error&&i<scan->count;i++){
        const AxOccurrence *item=&scan->occurrences[i];
        if(item->type!=PRO_MDL_PART||_wcsicmp(item->name,record->geometry.model))continue;
        if(!item->model){error=PRO_TK_INCOMPLETE;break;}
        wchar_t id[AX_STUDY_PATH],parent[AX_STUDY_PATH]=L"ROOT";
        char actual[240];
        if(!ax_path_label(item->ids,item->depth,id,_countof(id))||
           (item->depth>1&&!ax_path_label(item->ids,item->depth-1,parent,_countof(parent)))){error=PRO_TK_BAD_CONTEXT;break;}
        error=ax_study_verstamp(item->model,actual);
        if(error)break;
        const wchar_t *state=strcmp(actual,record->geometry.stamp)?L"同名但版本需核对":L"版本一致";
        fwprintf(receipt,L"%ls\t%ls\t%ls\t%ls\t%ls\t%ls\t%ls\n",project->value[0],project->value[1],file,id,parent,item->name,state);
        part_text_add(out,L"%ls · %ls · %ls（父 %ls，%ls）\n",project->value[0],project->value[1],id,parent,state);
        (*hits)++;
    }
    free(scan);
    if(loaded&&assembly){
        ProError erased=ProMdlErase(assembly);
        if(!error)error=erased;
    }
    return error;
}

static ProError std_where_used(const wchar_t *key,const wchar_t *rev,const wchar_t *projects,PartText *out){
    StdRecord record={0};
    ProError error=std_lookup(key,rev,&record);
    if(error)return error;
    OrTable rows={0};
    if(!or_table_load(&rows,projects,3,out)){or_free(&rows);return PRO_TK_BAD_INPUTS;}
    wchar_t folder[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    FILE *write=NULL;
    if(std_dir(folder,0)&&swprintf_s(path,_countof(path),L"%ls\\%ls.%ls.where-used.tsv",folder,record.key,record.geometry.revision)>=0)
        write=ax_study_atomic(path,temp);
    if(!write){or_free(&rows);return PRO_TK_CANT_WRITE;}
    fputws(L"机型\t订单\tCreo总装\t引用实例\t父装配\t组件\t版本验证\n",write);
    int count=0;
    for(int i=0;i<rows.size;i++){
        error=std_where_one(&rows.rows[i],&record,write,&count,out);
        if(error){part_text_add(out,L"清单第 %u 行无法完整检查。\n",rows.rows[i].line);break;}
    }
    if(error){fclose(write);DeleteFileW(temp);}
    else if(!ax_study_commit(write,temp,path))error=PRO_TK_CANT_WRITE;
    if(!error)part_text_add(out,L"扫描 %d 个总装，找到 %d 个引用候选。迁移清单 %ls。\n",rows.size,count,path);
    or_free(&rows);
    return error;
}
