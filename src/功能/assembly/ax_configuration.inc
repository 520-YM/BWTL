
typedef struct {
    wchar_t label[PRO_NAME_SIZE],old_name[PRO_NAME_SIZE],new_name[PRO_NAME_SIZE],interface_token[PRO_NAME_SIZE+5];
    int id;
} AxConfigRow;

typedef struct { AxConfigRow rows[AX_CONFIG_LIMIT]; int count; } AxConfig;

static int ax_config_row(AxConfig *config,const wchar_t *label,const wchar_t *target,
    const wchar_t *old_name,const wchar_t *new_name,const wchar_t *interface_token){
    int ids[AX_PATH_DEPTH];
    if(config->count>=AX_CONFIG_LIMIT||!fd_name(label)||ax_path_parse(target,ids)!=1||
       !fd_name(old_name)||!fd_name(new_name)||!_wcsicmp(old_name,new_name)||!ax_csys_token(interface_token))return 0;
    for(int i=0;i<config->count;i++)
        if(config->rows[i].id==ids[0]&&!_wcsicmp(config->rows[i].label,label))return 0;
    AxConfigRow *row=&config->rows[config->count++];
    row->id=ids[0];
    wcscpy_s(row->label,_countof(row->label),label);
    wcscpy_s(row->old_name,_countof(row->old_name),old_name);
    wcscpy_s(row->new_name,_countof(row->new_name),new_name);
    wcscpy_s(row->interface_token,_countof(row->interface_token),interface_token);
    return 1;
}

static ProError ax_config_models(ProMdl assembly,const AxConfigRow *row,double mm,
    AxComponent *old,AxComponent *replacement,int *loaded){
    ProCsysdata before,after;
    ProSelection original=NULL,proposed=NULL;
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    wchar_t id[32];
    swprintf_s(id,_countof(id),L"ID:%d",row->id);
    ProError error=ax_component_named(assembly,id,row->old_name,old);
    if(!error)error=ax_ordinary_direct(&old->feature);
    if(!error)error=ax_batch_model(row->new_name,mm,&replacement->model,loaded);
    if(!error)error=ax_csys_selection(old->model,row->interface_token,0,&original,&before);
    if(!error)error=ax_csys_selection(replacement->model,row->interface_token,0,&proposed,&after);
    if(!error&&!ax_frames_aligned(&before,&after,identity,mm))error=PRO_TK_BAD_CONTEXT;
    if(original)ProSelectionFree(&original);
    if(proposed)ProSelectionFree(&proposed);
    return error;
}

static ProError ax_config_write(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxConfig config={0};
    ProMdlName name;
    ProError error=ProMdlMdlnameGet(assembly,name);
    if(error)return error;
    if(!fd_name(name))return PRO_TK_BAD_CONTEXT;
    for(int i=3;i<cmd->count;i+=5){
        AxComponent old={0},replacement={0};
        int loaded=0;
        if(!ax_config_row(&config,cmd->arg[i],cmd->arg[i+1],cmd->arg[i+2],cmd->arg[i+3],cmd->arg[i+4]))
            return PRO_TK_BAD_INPUTS;
        error=ax_config_models(assembly,&config.rows[config.count-1],mm,&old,&replacement,&loaded);
        if(loaded&&replacement.model){
            ProError erase=ProMdlErase(replacement.model);
            if(!error)error=erase;
        }
        if(error){
            part_text_add(out,L"配置 %ls 的 %ls 未通过同名坐标系位置和方向核对。\n",cmd->arg[i],cmd->arg[i+1]);
            return error;
        }
    }
    DeliveryOutput output={0};
    error=dv_stage_begin(cmd->arg[2],&output);
    if(error)return error;
    ProPath path;
    if(swprintf_s(path,_countof(path),L"%ls\\config.txt",output.stage)<0)
        return dv_stage_finish(&output,PRO_TK_BAD_INPUTS,out);
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file)return dv_stage_finish(&output,PRO_TK_CANT_WRITE,out);
    int ok=fwprintf(file,L"BWTL_CONFIG_1 %ls\n",name)>0;
    for(int i=0;ok&&i<config.count;i++){
        const AxConfigRow *row=&config.rows[i];
        ok=fwprintf(file,L"%ls ID:%d %ls %ls %ls\n",row->label,row->id,row->old_name,row->new_name,row->interface_token)>0;
    }
    if(fclose(file)!=0||!ok||!dv_file_nonempty(path))error=PRO_TK_CANT_WRITE;
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已生成产品配置表（%d 条替换规则）：%ls\\config.txt。\n",config.count,output.target);
    return error;
}

static ProError ax_config_read(ProMdl assembly,const wchar_t *leaf,const wchar_t *label,AxConfig *config){
    ProPath parent,folder,path;
    ProMdlName source;
    ProError error=ProDirectoryCurrentGet(parent);
    if(!error)error=ProMdlMdlnameGet(assembly,source);
    if(error)return error;
    if(!dv_safe_leaf(leaf)||!fd_name(source)||swprintf_s(folder,_countof(folder),L"%ls\\%ls",parent,leaf)<0||
       swprintf_s(path,_countof(path),L"%ls\\config.txt",folder)<0)return PRO_TK_BAD_INPUTS;
    DWORD attr=GetFileAttributesW(folder);
    if(attr==INVALID_FILE_ATTRIBUTES||!(attr&FILE_ATTRIBUTE_DIRECTORY)||(attr&FILE_ATTRIBUTE_REPARSE_POINT)||
       !dv_file_nonempty(path))return PRO_TK_E_NOT_FOUND;
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    if(!file)return PRO_TK_E_NOT_FOUND;
    wchar_t line[256],expected[256];
    AxConfig all={0};
    swprintf_s(expected,_countof(expected),L"BWTL_CONFIG_1 %ls\n",source);
    if(!fgetws(line,_countof(line),file)||_wcsicmp(line,expected))error=PRO_TK_BAD_INPUTS;
    while(!error&&fgetws(line,_countof(line),file)){
        wchar_t col[5][64],extra;
        if(!wcschr(line,L'\n')||swscanf_s(line,L"%63ls %63ls %63ls %63ls %63ls %lc",
                col[0],64u,col[1],64u,col[2],64u,col[3],64u,col[4],64u,&extra,1u)!=5||
           !ax_config_row(&all,col[0],col[1],col[2],col[3],col[4])){
            error=PRO_TK_BAD_INPUTS;
            break;
        }
        if(!_wcsicmp(label,col[0]))config->rows[config->count++]=all.rows[all.count-1];
    }
    if(!error&&(!feof(file)||ferror(file)))error=PRO_TK_BAD_INPUTS;
    fclose(file);
    return error?error:config->count?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError ax_config_apply(ProMdl assembly,const AxConfig *config,double mm,int backup,PartText *out){
    AxReplacement items[AX_CONFIG_LIMIT]={0};
    AxComponent replacement[AX_CONFIG_LIMIT]={0};
    int loaded[AX_CONFIG_LIMIT]={0},*ids=NULL,delete_attempted=0;
    ProMdl recovery=NULL;
    ProPath folder;
    ProMdlName name;
    ProAsmcomppath root;
    FdTxn before_delete={0};
    ProError error;
#define CONFIG_STEP(call) do{if((error=(call))){trace("assembly configuration " #call,error);goto done;}}while(0)
    CONFIG_STEP(ax_root_path(assembly,&root));
    CONFIG_STEP(ProArrayAlloc(config->count,sizeof(*ids),1,(ProArray *)&ids));
    for(int i=0;i<config->count;i++){
        AxReplacement *item=&items[i];
        CONFIG_STEP(ax_config_models(assembly,&config->rows[i],mm,&item->old,&replacement[i],&loaded[i]));
        ids[i]=item->old.feature.id;
        CONFIG_STEP(ax_dependencies(&item->old.feature,out));
        CONFIG_STEP(ax_check_default_set(&item->old.feature));
        CONFIG_STEP(ax_actual_pose(assembly,&item->old.feature,item->pose));
        CONFIG_STEP(ax_batch_replace_rows(item,&replacement[i],&root,mm));
    }
    if(backup){
        CONFIG_STEP(ax_delete_backup(assembly,&recovery,folder,name));
        part_text_add(out,L"恢复副本 %ls\\%ls.asm。\n",folder,name);
    }
    for(int i=0;i<config->count;i++){
        AxReplacement *item=&items[i];
        CONFIG_STEP(ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)replacement[i].model,item->pose,&item->staged.feature));
        if(item->count)CONFIG_STEP(ProAsmcompConstraintsSet(&root,&item->staged.feature,item->rows));
    }
    CONFIG_STEP(part_regenerate(assembly));
    for(int i=0;i<config->count;i++)
        CONFIG_STEP(ax_batch_replacement_pose(assembly,&items[i],replacement[i].model,&root,mm));
    CONFIG_STEP(ProSolidFeatVisit((ProSolid)assembly,ax_public_feature,NULL,&before_delete));
    delete_attempted=1;
    CONFIG_STEP(ax_multi_delete(assembly,ids));
    CONFIG_STEP(part_regenerate(assembly));
    CONFIG_STEP(ax_verify_multi_delete(assembly,&before_delete,ids,config->count));
    for(int i=0;i<config->count;i++)
        CONFIG_STEP(ax_batch_replacement_pose(assembly,&items[i],replacement[i].model,&root,mm));
    if(backup)for(int i=0;i<config->count;i++)
        part_text_add(out,L"ID:%d %ls → ID:%d %ls。\n",ids[i],config->rows[i].old_name,
            items[i].staged.feature.id,config->rows[i].new_name);
done:
    if(error){
        if(delete_attempted&&ax_verify_multi_delete(assembly,&before_delete,ids,0)){
            if(backup)ax_uncertain(out);
        }else for(int i=config->count-1;i>=0;i--)ax_discard_occurrence(assembly,&items[i].staged.feature,out);
    }
    for(int i=0;i<config->count;i++)if(items[i].rows)ProAsmcompconstraintArrayFree(items[i].rows);
    if(ids)ProArrayFree((ProArray *)&ids);
    if(recovery)ProMdlErase(recovery);
    if(error&&!ax_recovery_uncertain)
        for(int i=0;i<config->count;i++)if(loaded[i]&&replacement[i].model)ProMdlErase(replacement[i].model);
    return error;
#undef CONFIG_STEP
}

static ProError ax_order_final_revision(const wchar_t *folder,const wchar_t *name){
    ProPath pattern,path;
    WIN32_FIND_DATAW entry;
    int latest=0;
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.asm.*",folder,name)<0)return PRO_TK_BAD_INPUTS;
    HANDLE found=FindFirstFileW(pattern,&entry);
    if(found==INVALID_HANDLE_VALUE)return PRO_TK_CANT_WRITE;
    do{
        const wchar_t *dot=wcsrchr(entry.cFileName,L'.');
        int revision;
        if(dot&&part_integer(dot+1,&revision)&&revision>latest&&
            swprintf_s(path,_countof(path),L"%ls\\%ls",folder,entry.cFileName)>=0&&dv_file_nonempty(path))
            latest=revision;
    }while(FindNextFileW(found,&entry));
    FindClose(found);
    if(!latest)return PRO_TK_CANT_WRITE;
    found=FindFirstFileW(pattern,&entry);
    if(found==INVALID_HANDLE_VALUE)return PRO_TK_CANT_WRITE;
    ProError error=PRO_TK_NO_ERROR;
    do{
        const wchar_t *dot=wcsrchr(entry.cFileName,L'.');
        int revision;
        if(dot&&part_integer(dot+1,&revision)&&revision<latest){
            if(swprintf_s(path,_countof(path),L"%ls\\%ls",folder,entry.cFileName)<0||
                !dv_file_nonempty(path)||!DeleteFileW(path)){error=PRO_TK_CANT_WRITE;break;}
        }
    }while(FindNextFileW(found,&entry));
    FindClose(found);
    return error;
}

static ProError ax_order_saved_dependencies(ProMdl assembly,PartText *out){
    AxScan *scan=NULL;
    ProError error=ax_scan(assembly,1,&scan);
    for(int i=0;!error&&i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model)continue;
        ProBoolean modified;
        error=ProMdlModificationVerify(item->model,&modified);
        if(!error&&modified){
            part_text_add(out,L"组件 %ls 有未保存修改，订单输出已停止。\n",item->name);
            error=PRO_TK_BAD_CONTEXT;
        }
    }
    free(scan);
    return error;
}

static ProError ax_config_order(ProMdl assembly,const PartCommand *cmd,const AxConfig *config,double mm,PartText *out){
    DeliveryOutput output={0};
    ProMdl copy=NULL,existing=NULL;
    ProMdlName name;
    ProWVerstamp initial=NULL,final=NULL;
    ProPath original=L"",restored;
    int origin_read=0;
    ProMdl staged_models[AX_CONFIG_LIMIT]={0};
    int loaded[AX_CONFIG_LIMIT]={0};
    ProError error=PRO_TK_NO_ERROR;
    wcscpy_s(name,_countof(name),cmd->arg[4]);
    if(dv_recovery_uncertain)return PRO_TK_BAD_CONTEXT;
    if(!part_target_available(assembly,name,L"asm")||!part_target_available(assembly,name,L"prt")||
       !ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&existing))return PRO_TK_E_FOUND;
    error=ProMdlVerstampGet(assembly,&initial);
    if(!error){error=ProMdlOriginGet(assembly,original);origin_read=!error;}
    if(!error)error=ax_order_saved_dependencies(assembly,out);
    for(int i=0;!error&&i<config->count;i++){
        AxComponent old={0},replacement={0};
        error=ax_config_models(assembly,&config->rows[i],mm,&old,&replacement,&loaded[i]);
        staged_models[i]=replacement.model;
        ProBoolean modified;
        if(!error)error=ProMdlModificationVerify(replacement.model,&modified);
        if(!error&&modified){
            part_text_add(out,L"替换零件 %ls 有未保存修改，订单输出已停止。\n",config->rows[i].new_name);
            error=PRO_TK_BAD_CONTEXT;
        }
    }
    if(!error)error=dv_stage_begin(cmd->arg[5],&output);
    if(!error)error=dv_stage_enter(&output);
    if(!error)error=ProMdlnameCopy(assembly,name,&copy);
    if(!error)error=ax_config_apply(copy,config,mm,0,out);
    if(!error)error=ProMdlSave(copy);
    if(!error)error=ax_order_final_revision(output.stage,name);
    if(copy){
        ProError erase=ProMdlErase(copy);
        if(!error)error=erase;
    }
    if(initial){
        ProError check=ProMdlVerstampGet(assembly,&final);
        if(!check&&origin_read)check=ProMdlOriginGet(assembly,restored);
        if(!check&&(!ProVerstampEqual(initial,final)||(origin_read&&_wcsicmp(original,restored))))check=PRO_TK_GENERAL_ERROR;
        if(check){ax_uncertain(out);error=check;}
        ProVerstampFree(&initial);
    }
    if(final)ProVerstampFree(&final);
    for(int i=0;i<config->count;i++)if(loaded[i]&&staged_models[i])ProMdlErase(staged_models[i]);
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已输出订单装配 %ls\\%ls.asm，原模板版本和来源未改变。副本引用已有组件文件。\n",
        output.target,name);
    return error;
}

static ProError ax_config_execute(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    if(kind==AX_V_CONFIG)return ax_config_write(assembly,cmd,mm,out);
    AxConfig config={0};
    ProError error=ax_config_read(assembly,cmd->arg[2],cmd->arg[3],&config);
    if(error){
        part_text_add(out,L"无法读取该装配的配置表或找不到配置 %ls。\n",cmd->arg[3]);
        return error;
    }
    if(kind==AX_V_ORDER)return ax_config_order(assembly,cmd,&config,mm,out);
    error=ax_config_apply(assembly,&config,mm,1,out);
    if(!error)part_text_add(out,L"配置 %ls 已应用，%d 个替换位置与安装接口已核对。装配未保存。\n",
        cmd->arg[3],config.count);
    return error;
}
