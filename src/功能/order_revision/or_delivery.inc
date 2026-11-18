
static int or_real_path(const wchar_t *path,int folder,wchar_t resolved[MAX_PATH]){
    HANDLE handle=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,OPEN_EXISTING,folder?FILE_FLAG_BACKUP_SEMANTICS:FILE_ATTRIBUTE_NORMAL,NULL);
    if(handle==INVALID_HANDLE_VALUE)return 0;
    wchar_t final[MAX_PATH];
    DWORD count=GetFinalPathNameByHandleW(handle,final,_countof(final),VOLUME_NAME_DOS);
    BY_HANDLE_FILE_INFORMATION info;
    int good=count>=7&&count<_countof(final)&&!wcsncmp(final,L"\\\\?\\",4)&&final[5]==L':'&&final[6]==L'\\'&&
        GetFileInformationByHandle(handle,&info)&&folder==!!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
    CloseHandle(handle);
    if(good)wcscpy_s(resolved,MAX_PATH,final+4);
    return good;
}

static int or_below(const wchar_t *path,const wchar_t *folder){
    size_t n=wcslen(folder);
    return n>0&&!_wcsnicmp(path,folder,n)&&(folder[n-1]==L'\\'||path[n]==L'\\');
}

static ProError or_location(ProMdl model,wchar_t cwd[MAX_PATH],wchar_t file[MAX_PATH],ProMdlName name,PartText *out){
    ProPath raw,current;
    ProMdlType type;
    ProError error=model?ProMdlTypeGet(model,&type):PRO_TK_BAD_CONTEXT;
    if(error)return error;
    if(type!=PRO_MDL_PART&&type!=PRO_MDL_ASSEMBLY)return PRO_TK_BAD_CONTEXT;
    if((error=ProMdlMdlnameGet(model,name))||(error=ProDirectoryCurrentGet(current))||
       (error=ProMdlOriginGet(model,raw)))return error;
    if(!storage_output_directory(current)||!or_real_path(current,1,cwd)||!or_real_path(raw,0,file)||!or_below(file,cwd)){
        part_text_add(out,L"请将目标模型保存到该订单的工作目录并从那里打开。Creo 当前目录 %ls，模型来源 %ls。\n",current,raw);
        return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static ProError or_order_marker(const wchar_t *folder,const wchar_t *order,int create){
    wchar_t path[MAX_PATH];
    if(swprintf_s(path,MAX_PATH,L"%ls\\.bwtl-order.txt",folder)<0)return PRO_TK_BAD_INPUTS;
    FILE *read=ax_learning_open(path);
    if(read){
        wchar_t expected[84]=L"";
        int good=fgetws(expected,_countof(expected),read)!=NULL;
        fclose(read);
        expected[wcscspn(expected,L"\r\n")]=0;
        wchar_t *start=expected[0]==0xfeff?expected+1:expected;
        return good&&!_wcsicmp(start,order)?PRO_TK_NO_ERROR:PRO_TK_E_IN_USE;
    }
    if(!create)return PRO_TK_E_NOT_FOUND;
    FILE *write=_wfopen(path,L"wtx, ccs=UTF-8");
    if(!write)return PRO_TK_CANT_WRITE;
    int good=fwprintf(write,L"%ls\n",order)>=0;
    if(fclose(write))good=0;
    return good?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
}

static ProError or_workspace(ProMdl model,const wchar_t *order,int create,PartText *out){
    if(!or_safe_id(order))return PRO_TK_BAD_INPUTS;
    wchar_t current[MAX_PATH],existing[MAX_PATH],state[MAX_PATH];
    ProMdlName name;
    ProError error=or_location(model,current,existing,name,out);
    if(error)return error;
    if(!or_path(order,L"订单绑定.tsv",state,create))return PRO_TK_E_NOT_FOUND;
    if(!create||GetFileAttributesW(state)!=INVALID_FILE_ATTRIBUTES){
        OrTable saved={0};
        int ok=or_table_load(&saved,state,2,out)&&saved.size==1&&
            !_wcsicmp(current,saved.rows[0].value[0])&&!_wcsicmp(name,saved.rows[0].value[1]);
        or_free(&saved);
        if(!ok){
            part_text_add(out,L"目标与订单 %ls 绑定的工作目录或顶层模型不一致。\n",order);
            return PRO_TK_BAD_CONTEXT;
        }
    }
    error=or_order_marker(current,order,create);
    if(error){
        part_text_add(out,L"该工作目录的订单标识无法核对。目录 %ls，要求订单 %ls。\n",current,order);
        return error;
    }
    if(create&&GetFileAttributesW(state)==INVALID_FILE_ATTRIBUTES){
        OrOutput sheet;
        if(!or_output_begin(&sheet,order,L"订单绑定.tsv"))return PRO_TK_CANT_WRITE;
        fwprintf(sheet.file,L"%ls\t%ls\n",current,name);
        if(!or_output_end(&sheet,1))return PRO_TK_CANT_WRITE;
    }
    part_text_add(out,L"订单 %ls，模型 %ls，工作目录 %ls，绑定检查通过。\n",order,name,current);
    return PRO_TK_NO_ERROR;
}

static int or_task_order(const OrTable *input,int id,int *visited,int *sequence,int *count,PartText *out){
    if(visited[id]==2)return 1;
    const OrRow *row=&input->rows[id];
    if(visited[id]==1){
        part_text_add(out,L"任务 %ls 的依赖形成循环。\n",row->value[0]);
        return 0;
    }
    visited[id]=1;
    if(wcscmp(row->value[4],L"-")){
        wchar_t path[OR_CELL],*context=NULL;
        wcscpy_s(path,_countof(path),row->value[4]);
        for(wchar_t *part=wcstok_s(path,L",",&context);part;part=wcstok_s(NULL,L",",&context)){
            or_trim(part);
            int prior=or_find(input,0,part);
            if(prior<0){
                part_text_add(out,L"任务 %ls 指向未登记的前置项 %ls。\n",row->value[0],part);
                return 0;
            }
            if(!or_task_order(input,prior,visited,sequence,count,out))return 0;
        }
    }
    visited[id]=2;
    sequence[(*count)++]=id;
    return 1;
}

static ProError or_tasks(const wchar_t *order,const wchar_t *path,PartText *out){
    OrTable input={0};
    int ok=or_table_load(&input,path,6,out)&&or_unique_column(&input,0,out);
    int state[OR_ROWS]={0},sequence[OR_ROWS]={0},count=0;
    for(int i=0;ok&&i<input.size;i++){
        if(wcscmp(input.rows[i].value[5],L"0")&&wcscmp(input.rows[i].value[5],L"1"))ok=0;
        else if(!wcscmp(input.rows[i].value[5],L"1"))ok=or_task_order(&input,i,state,sequence,&count,out);
    }
    if(!ok||!count){or_free(&input);return PRO_TK_BAD_INPUTS;}
    OrOutput sheet;
    if(!or_output_begin(&sheet,order,L"顺序任务.tsv")){or_free(&input);return PRO_TK_CANT_WRITE;}
    fwprintf(sheet.file,L"# 序号\t任务ID\t模块\t类别\t说明\t先完成\n");
    for(int i=0;i<count;i++){
        const OrRow *row=&input.rows[sequence[i]];
        fwprintf(sheet.file,L"%d\t%ls\t%ls\t%ls\t%ls\t%ls\n",i+1,row->value[0],row->value[1],row->value[2],row->value[3],row->value[4]);
        part_text_add(out,L"%d. %ls · %ls · %ls。\n",i+1,row->value[1],row->value[2],row->value[3]);
    }
    int written=or_output_end(&sheet,1);
    if(written)part_text_add(out,L"%ls\n",sheet.final);
    or_free(&input);
    return written?PRO_TK_NO_ERROR:PRO_TK_CANT_WRITE;
}

static ProError or_closeout(ProMdl model,const wchar_t *order,const wchar_t *checks,PartText *out){
    ProError error=or_workspace(model,order,0,out);
    if(error)return error;
    wchar_t guard[MAX_PATH],cwd[MAX_PATH],where[MAX_PATH];
    ProMdlName name;
    if(or_path(order,L"保留项.tsv",guard,0)&&GetFileAttributesW(guard)!=INVALID_FILE_ATTRIBUTES){
        error=or_compare(model,order,1,out);
        if(error)return error;
    }
    ProBoolean dirty=PRO_B_TRUE;
    if((error=ProMdlModificationVerify(model,&dirty))||dirty){
        part_text_add(out,L"请先确认本次实际改动并保存目标模型，再生成交付闭环单。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    if((error=or_location(model,cwd,where,name,out)))return error;
    OrTable expected={0};
    if(!or_table_load(&expected,checks,6,out)||!or_unique_column(&expected,0,out)){
        or_free(&expected);return PRO_TK_BAD_INPUTS;
    }
    OrOutput report;
    if(!or_output_begin(&report,order,L"设计改动闭环.tsv")){or_free(&expected);return PRO_TK_CANT_WRITE;}
    char stamp[240];
    error=ax_study_verstamp(model,stamp);
    if(!error){
        fwprintf(report.file,L"# %ls %ls 版本 %hs\n# 任务\t需求\t测量\t目标\t实测\t公差\t交付文件\t状态\n",order,name,stamp);
        int failed=0;
        for(int i=0;i<expected.size;i++){
            OrRow *row=&expected.rows[i];
            double want=0,have=0,tolerance=0;
            wchar_t file[MAX_PATH]=L"",asset[MAX_PATH]=L"";
            int geometry=or_number(row->value[3],&want)&&or_number(row->value[4],&tolerance)&&tolerance>=0&&
                !or_read_number(model,row->value[2],&have);
            int delivery=swprintf_s(asset,MAX_PATH,L"%ls\\%ls",cwd,row->value[5])>=0&&
                or_real_path(asset,0,file)&&or_below(file,cwd);
            int passed=geometry&&fabs(want-have)<=tolerance&&delivery;
            failed+=!passed;
            fwprintf(report.file,L"%ls\t%ls\t%ls\t%.12g\t%.12g\t%.12g\t%ls\t%ls\n",
                row->value[0],row->value[1],row->value[2],want,have,tolerance,delivery?file:L"-",passed?L"通过":geometry?L"未闭环":L"参照未核实");
            part_text_add(out,L"%ls %ls，%ls。\n",row->value[0],row->value[1],
                passed?L"模型和交付文件均通过":!geometry?L"测量参照不完整":!delivery?L"缺交付文件或文件不属于本订单":L"尺寸超差");
        }
        if(!or_output_end(&report,1))error=PRO_TK_CANT_WRITE;
        else{
            part_text_add(out,L"闭环 %d/%d 项。\n%ls\n",expected.size-failed,expected.size,report.final);
            if(failed)error=PRO_TK_INCOMPLETE;
        }
    }else or_output_end(&report,0);
    or_free(&expected);
    return error;
}
