
typedef struct {
    ProMdl model;
    ProMdlType type;
    ProMdlName name;
} AxPackModel;

static int ax_pack_add(AxPackModel *models,int *count,ProMdl model,const wchar_t *name,ProMdlType type){
    if(!fd_name(name))return 0;
    for(int i=0;i<*count;i++)if(type==models[i].type&&!_wcsicmp(name,models[i].name)){
        if(model)models[i].model=model;
        return 1;
    }
    if(*count==AX_MAX_OCCURRENCES+1)return 0;
    AxPackModel *entry=&models[(*count)++];
    entry->model=model;
    entry->type=type;
    wcscpy_s(entry->name,_countof(entry->name),name);
    return 1;
}

static int ax_pack_plain_folder(const wchar_t *folder){
    DWORD attr=GetFileAttributesW(folder);
    return attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY)&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT);
}

static ProError ax_pack_saved_path(const AxPackModel *entry,ProPath source){
    ProBoolean modified;
    ProPath directory=L"",origin=L"",base,pattern;
    const wchar_t *ext=entry->type==PRO_MDL_ASSEMBLY?L"asm":L"prt";
    if(!entry->model)return PRO_TK_E_NOT_FOUND;
    ProError error=ProMdlModificationVerify(entry->model,&modified);
    if(error||modified)return error?error:PRO_TK_BAD_CONTEXT;
    if(swprintf_s(base,_countof(base),L"%ls.%ls",entry->name,ext)<0)return PRO_TK_BAD_INPUTS;
    error=ProMdlOriginGet(entry->model,origin);
    if(error)return error;
    if(origin[0]){
        wchar_t *slash=wcsrchr(origin,L'\\');
        if(!slash)slash=wcsrchr(origin,L'/');
        size_t length=wcslen(base);
        int revision;
        if(!slash||_wcsnicmp(slash+1,base,length)||(slash[1+length]&&slash[1+length]!=L'.'))return PRO_TK_BAD_CONTEXT;
        if(slash[1+length]==L'.'&&part_integer(slash+2+length,&revision)&&revision>0){
            /* An explicit origin revision must agree with the saved file we package. */
            wcscpy_s(source,PRO_PATH_SIZE,origin);
            if(!dv_file_nonempty(source))return PRO_TK_CANT_OPEN;
        }
        *slash=0;
        wcscpy_s(directory,PRO_PATH_SIZE,origin);
    }else{
        error=ProMdlDirectoryPathGet(entry->model,directory);
        if(error)return error;
    }
    if(!ax_pack_plain_folder(directory)||swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.*",directory,base)<0)
        return PRO_TK_CANT_OPEN;
    WIN32_FIND_DATAW file;
    HANDLE find=FindFirstFileW(pattern,&file);
    int latest=0;
    ProPath candidate=L"";
    if(find!=INVALID_HANDLE_VALUE){
        do{
            const wchar_t *dot=wcsrchr(file.cFileName,L'.');
            int revision;
            if(dot&&part_integer(dot+1,&revision)&&revision>latest&&
                !(file.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
                (file.nFileSizeHigh||file.nFileSizeLow)){
                latest=revision;
                wcscpy_s(candidate,_countof(candidate),file.cFileName);
            }
        }while(FindNextFileW(find,&file));
        FindClose(find);
    }
    if(!latest)return PRO_TK_CANT_OPEN;
    if(source[0]){
        const wchar_t *leaf=wcsrchr(source,L'\\');
        if(!leaf)leaf=wcsrchr(source,L'/');
        if(!leaf||_wcsicmp(leaf+1,candidate))return PRO_TK_BAD_CONTEXT;
        return PRO_TK_NO_ERROR;
    }
    return swprintf_s(source,PRO_PATH_SIZE,L"%ls\\%ls",directory,candidate)<0?PRO_TK_BAD_INPUTS:PRO_TK_NO_ERROR;
}

static ProError ax_pack_copy(const wchar_t *source,const wchar_t *leaf,const wchar_t *folder,wchar_t digest[65]){
    ProPath target;
    wchar_t actual[65];
    ULONGLONG expected_bytes,actual_bytes;
    if(swprintf_s(target,_countof(target),L"%ls\\%ls",folder,leaf)<0)return PRO_TK_BAD_INPUTS;
    if(!dv_sha256(source,digest,&expected_bytes))return PRO_TK_CANT_OPEN;
    if(!CopyFileW(source,target,TRUE))return PRO_TK_CANT_WRITE;
    if(!dv_sha256(target,actual,&actual_bytes)||expected_bytes!=actual_bytes||wcscmp(digest,actual))return PRO_TK_GENERAL_ERROR;
    return PRO_TK_NO_ERROR;
}

static ProError ax_pack_dependencies(const AxBomData *data,const wchar_t *folder,int *packed,int *missing,PartText *out){
    AxPackModel *models=calloc(AX_MAX_OCCURRENCES+1,sizeof(*models));
    if(!models)return PRO_TK_OUT_OF_MEMORY;
    FILE *list=NULL;
    ProError error=ax_bom_open(folder,L"native-files.csv",&list);
    int count=0,ok=1;
    if(error){free(models);return error;}
    ok=fwprintf(list,L"模型,类型,交付文件,状态,SHA256\n")>0;
    for(int i=0;ok&&i<data->count;i++){
        const AxBomModel *row=&data->models[i];
        ok=ax_pack_add(models,&count,row->model,row->name,row->type);
    }
    for(int i=0;ok&&i<data->scan->count;i++){
        const AxOccurrence *item=&data->scan->occurrences[i];
        if(item->model)continue;
        if(item->type==PRO_MDL_PART||item->type==PRO_MDL_ASSEMBLY)
            ok=ax_pack_add(models,&count,NULL,item->name,item->type);
        else{
            const wchar_t *cells[]={item->name,L"未知",L"",L"实例类型不可核实",L""};
            ok=ax_bom_csv(list,cells,_countof(cells));
            (*missing)++;
        }
    }
    for(int i=0;ok&&i<count;i++){
        AxPackModel *entry=&models[i];
        if(!entry->model)ProMdlnameInit(entry->name,entry->type,&entry->model);
        if(!entry->model)continue;
        ProMdlnameShortdata *names=NULL;
        ProMdlfileType *types=NULL;
        int length=0;
        ProError dep=ProMdlDependenciesDataList(entry->model,&names,&types,&length);
        if(dep&&dep!=PRO_TK_E_NOT_FOUND){
            const wchar_t *cells[]={entry->name,L"依赖",L"",L"依赖列表未完整读取",L""};
            ok=ax_bom_csv(list,cells,_countof(cells));
            (*missing)++;
        }
        for(int j=0;ok&&j<length;j++){
            if(types[j]==PRO_MDLFILE_PART||types[j]==PRO_MDLFILE_ASSEMBLY)
                ok=ax_pack_add(models,&count,NULL,names[j].name,types[j]==PRO_MDLFILE_ASSEMBLY?PRO_MDL_ASSEMBLY:PRO_MDL_PART);
            else{
                const wchar_t *cells[]={names[j].name,names[j].type,L"",L"非原生零件或装配依赖，需单独核对",L""};
                ok=ax_bom_csv(list,cells,_countof(cells));
                (*missing)++;
            }
        }
        if(names)ProArrayFree((ProArray *)&names);
        if(types)ProArrayFree((ProArray *)&types);
    }
    for(int i=0;ok&&i<count;i++){
        AxPackModel *entry=&models[i];
        ProPath source=L"";
        wchar_t digest[65]=L"";
        ProError state=ax_pack_saved_path(entry,source);
        const wchar_t *leaf=L"";
        const wchar_t *message=L"已收集";
        if(state){
            message=state==PRO_TK_BAD_CONTEXT?L"模型未保存、来源冲突或版本不明":
                !entry->model?L"未载入，无法核实依赖文件":L"缺少可读的已保存文件";
            (*missing)++;
            if(*missing<=30)part_text_add(out,L"%ls：%ls。\n",entry->name,message);
        }else{
            leaf=wcsrchr(source,L'\\');
            if(!leaf)leaf=wcsrchr(source,L'/');
            if(!leaf){ok=0;break;}
            leaf++;
            error=ax_pack_copy(source,leaf,folder,digest);
            if(error){ok=0;break;}
            (*packed)++;
        }
        const wchar_t *cells[]={entry->name,entry->type==PRO_MDL_ASSEMBLY?L"装配":L"零件",leaf,message,digest};
        ok=ax_bom_csv(list,cells,_countof(cells));
    }
    if(fclose(list)!=0||!ok)error=error?error:PRO_TK_CANT_WRITE;
    free(models);
    return error;
}

static ProError ax_pack_attachment(const DeliveryOutput *output,const wchar_t *input,const wchar_t *name,
    const wchar_t *ext,wchar_t digest[65]){
    ProPath source;
    if(!ax_pack_input_leaf(input,ext)||swprintf_s(source,_countof(source),L"%ls\\%ls",output->parent,input)<0)
        return PRO_TK_BAD_INPUTS;
    if(!dv_file_nonempty(source))return PRO_TK_CANT_OPEN;
    if(!_wcsicmp(ext,L".pdf")){
        FILE *pdf=_wfopen(source,L"rb");
        char header[5];
        if(!pdf)return PRO_TK_CANT_OPEN;
        int valid=fread(header,1,sizeof(header),pdf)==sizeof(header)&&!memcmp(header,"%PDF-",sizeof(header));
        fclose(pdf);
        if(!valid)return PRO_TK_BAD_INPUTS;
    }
    return ax_pack_copy(source,name,output->stage,digest);
}

static ProError ax_pack_bundle(const PartCommand *cmd,const AxBomData *data,const DeliveryOutput *output,PartText *out){
    wchar_t drawing_hash[65],report_hash[65];
    int packed=0,missing=0,written=0;
    ProError error=ax_pack_attachment(output,cmd->arg[3],L"assembly-drawing.pdf",L".pdf",drawing_hash);
    if(!error)error=ax_pack_attachment(output,cmd->arg[4],L"inspection-report.txt",L".txt",report_hash);
    if(!error)error=ax_pack_dependencies(data,output->stage,&packed,&missing,out);
    if(!error&&missing){
        part_text_add(out,L"有 %d 项依赖未核实。可先用原生打包功能查看清单。\n",missing);
        error=PRO_TK_INCOMPLETE;
    }
    if(!error)error=ax_bom_csv_output(AX_B_TREE,data,NULL,cmd,output->stage,&written);
    if(!error)error=ax_bom_csv_output(AX_B_BUY,data,NULL,cmd,output->stage,&written);
    FILE *file=NULL;
    if(!error)error=ax_bom_open(output->stage,L"delivery.csv",&file);
    if(!error){
        const wchar_t *drawing[]={L"装配图（用户指定）",cmd->arg[3],L"assembly-drawing.pdf",drawing_hash};
        const wchar_t *report[]={L"检查报告（用户指定）",cmd->arg[4],L"inspection-report.txt",report_hash};
        const wchar_t *model[]={L"配置对应的当前装配",data->models[0].name,L"native-files.csv",L""};
        int ok=fwprintf(file,L"内容,来源,交付文件,SHA256\n")>0&&ax_bom_csv(file,model,_countof(model))&&
            ax_bom_csv(file,drawing,_countof(drawing))&&ax_bom_csv(file,report,_countof(report));
        if(fclose(file)!=0||!ok)error=PRO_TK_CANT_WRITE;
    }
    return error;
}

static ProError ax_bom_delivery(ProMdl assembly,const PartCommand *cmd,int kind,PartText *out){
    if(dv_recovery_uncertain)return PRO_TK_BAD_CONTEXT;
    AxBomData data={0},prior={0};
    DeliveryOutput output={0};
    ProMdl old=NULL;
    int loaded=0,written=0,packed=0,missing=0;
    ProError error=ax_bom_collect(assembly,&data,kind==AX_B_TREE||kind==AX_B_CHECK||kind==AX_B_NATIVE);
    if(!error&&kind==AX_B_DIFF){
        error=ax_load_subassembly(cmd->arg[3],&old,&loaded);
        if(!error)error=old==assembly?PRO_TK_BAD_INPUTS:ax_master(old);
        if(!error)error=ax_bom_collect(old,&prior,0);
    }
    if(!error&&kind==AX_B_CHECK&&cmd->count==2){
        int ok=ax_bom_check_result(NULL,&data,out);
        ax_bom_clear(&data);
        return ok?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    }
    if(!error)error=dv_stage_begin(cmd->arg[2],&output);
    if(!error){
        if(kind==AX_B_NATIVE)error=ax_pack_dependencies(&data,output.stage,&packed,&missing,out);
        else if(kind==AX_B_PACK)error=ax_pack_bundle(cmd,&data,&output,out);
        else error=ax_bom_csv_output(kind,&data,&prior,cmd,output.stage,&written);
    }
    error=dv_stage_finish(&output,error,out);
    if(!error){
        if(kind==AX_B_NATIVE)
            part_text_add(out,L"已收集 %d 个原生文件，%d 项未核实。%ls\\native-files.csv\n",packed,missing,output.target);
        else if(kind==AX_B_PACK)part_text_add(out,L"装配交付包已生成：%ls\\delivery.csv。\n",output.target);
        else if(kind>=AX_B_FAST&&kind<=AX_B_ROUTE)
            part_text_add(out,L"已输出 %d 条记录：%ls\\%ls。\n",written,output.target,ax_bom_deliver_files[kind-AX_B_TREE]);
        else part_text_add(out,L"已输出 BOM：%ls\\%ls。\n",output.target,ax_bom_deliver_files[kind-AX_B_TREE]);
    }
    ax_bom_clear(&prior);
    ax_bom_clear(&data);
    if(loaded&&old)ProMdlErase(old);
    return error;
}
