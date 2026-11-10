static ProError ax_load_subassembly(const wchar_t *name,ProMdl *model,int *loaded){
    *loaded=0;
    ProError error=ProMdlnameInit((wchar_t *)name,PRO_MDL_ASSEMBLY,model);
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    ProPath directory,pattern,leaf=L"",path;
    if((error=ProDirectoryCurrentGet(directory)))return error;
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.asm.*",directory,name)<0)return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW file;
    HANDLE search=FindFirstFileW(pattern,&file);
    if(search==INVALID_HANDLE_VALUE)return PRO_TK_E_NOT_FOUND;
    int latest=0;
    do{
        const wchar_t *dot=wcsrchr(file.cFileName,L'.');
        int revision;
        if(!(file.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
           (file.nFileSizeHigh||file.nFileSizeLow)&&dot&&part_integer(dot+1,&revision)&&revision>latest){
            latest=revision;
            wcscpy_s(leaf,_countof(leaf),file.cFileName);
        }
    }while(FindNextFileW(search,&file));
    FindClose(search);
    if(!latest)return PRO_TK_E_NOT_FOUND;
    if(swprintf_s(path,_countof(path),L"%ls\\%ls",directory,leaf)<0)return PRO_TK_BAD_INPUTS;
    error=ProMdlFiletypeLoad(path,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,model);
    if(!error)*loaded=1;
    return error;
}

static ProError ax_new_subassembly(const wchar_t *name,ProMdl *model){
    ProPath path;
    ProMdl source=NULL,existing=NULL;
    TemplateDatums original={0},copy={0};
    if(!part_target_available(NULL,name,L"asm")||!ProMdlnameInit((wchar_t *)name,PRO_MDL_ASSEMBLY,&existing))return PRO_TK_E_FOUND;
    ProError error=ProConfigoptionGet(L"template_designasm",path);
    if(error)return error;
    if(!path[0])return PRO_TK_E_NOT_FOUND;
    if((error=ProMdlFiletypeLoad(path,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&source))||
       (error=ProSolidFeatVisit((ProSolid)source,template_feature,NULL,&original)))return error;
    if(original.components)return PRO_TK_BAD_CONTEXT;
    if((error=ProMdlnameCopy(source,(wchar_t *)name,model))||
       (error=ProSolidFeatVisit((ProSolid)*model,template_feature,NULL,&copy)))return error;
    return copy.total==original.total&&copy.planes==original.planes&&copy.csys==original.csys&&!copy.components?
        PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static void ax_discard_occurrence(ProMdl assembly,ProAsmcomp *occurrence,PartText *out){
    if(occurrence->owner==assembly&&occurrence->id>0&&
       (ax_leaf_without_children(occurrence)||model_remove_feature((ProSolid)assembly,occurrence->id)||part_regenerate(assembly)))
        ax_uncertain(out);
}

static ProError ax_subassembly(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    ProMdl model=NULL,actual_model=NULL;
    ProAsmcomp occurrence={0};
    AxScan *scan=NULL;
    ProMatrix wanted={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}},actual;
    int loaded=0;
    double child_mm;
    ProError error=kind==AX_NEW_ASM?ax_new_subassembly(cmd->arg[2],&model):ax_load_subassembly(cmd->arg[2],&model,&loaded);
    if(error)goto done;
    if(assembly==model){error=PRO_TK_BAD_CONTEXT;goto done;}
    if((error=part_health(model))||(error=ax_master(model))||
       (error=qa_mm_per_unit(model,&child_mm))||(error=ax_scan(model,1,&scan)))goto done;
    if(!part_close(child_mm,mm)||scan->suppressed){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<scan->count;i++)if(scan->occurrences[i].model==assembly){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<3;i++)wanted[3][i]=as_arg(cmd,3+i)/mm;
    if((error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)model,wanted,&occurrence))||
       (error=part_regenerate(assembly))||(error=ax_packaged(&occurrence))||
       (error=ProAsmcompMdlGet(&occurrence,&actual_model))||(error=ax_actual_pose(assembly,&occurrence,actual)))goto done;
    if(actual_model!=model||!ax_same_pose(wanted,actual,mm)){error=PRO_TK_GENERAL_ERROR;goto done;}
    part_text_add(out,L"%ls子装配 %ls，实例 ID:%d，含 %d 个下级实例。初始位置已核对，顶层装配未保存。\n",
        kind==AX_NEW_ASM?L"已新建并装入":L"已装入",cmd->arg[2],occurrence.id,scan->count);
done:
    free(scan);
    if(error){
        ax_discard_occurrence(assembly,&occurrence,out);
        if(model&&(kind==AX_NEW_ASM||loaded)&&!ax_recovery_uncertain)ProMdlErase(model);
    }
    return error;
}

static ProError ax_component_named(ProMdl assembly,const wchar_t *id,const wchar_t *name,AxComponent *component){
    ProMdlName actual;
    ProError error=ax_component_resolve(assembly,id,component);
    if(!error)error=ProMdlMdlnameGet(component->model,actual);
    return error?error:_wcsicmp(actual,name)?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
}

static ProError ax_dependencies(ProAsmcomp *component,PartText *out){
    int *children=NULL,local=0,external=0;
    ProExtRefInfo *refs=NULL;
    ProError error=ProFeatureChildrenGet(component,&children,&local);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error)error=ProFeatureExternChildrenGet(component,PRO_ALL_EXT_REF_TYPES,&refs,&external);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error){
        part_text_add(out,L"下游依赖：装配内 %d 项，Creo 报告的外部引用 %d 项。\n",local,external);
        for(int i=0;i<local;i++)part_text_add(out,L"依赖特征 ID:%d\n",children[i]);
        if(local||external)error=PRO_TK_BAD_CONTEXT;
    }
    if(children)ProArrayFree((ProArray *)&children);
    if(refs)ProExtRefInfoFree(&refs);
    return error;
}

static ProError ax_public_feature(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProBoolean visible;
    ProError error=ProFeatureVisibilityGet(feature,&visible);
    return error?error:visible?fd_feature_ids(feature,PRO_TK_NO_ERROR,data):PRO_TK_NO_ERROR;
}

static ProError ax_delete_check(ProMdl assembly,int id,const FdTxn *before,int removed){
    FdTxn after={0};
    ProError error=part_health(assembly);
    if(!error)error=ProSolidFeatVisit((ProSolid)assembly,ax_public_feature,NULL,&after);
    if(error)return error;
    if(after.before_count!=before->before_count-removed){
        trace("assembly public features before",before->before_count);
        trace("assembly public features after",after.before_count);
        return PRO_TK_GENERAL_ERROR;
    }
    for(int i=0;i<after.before_count;i++)
        if((removed&&after.before_ids[i]==id)||!fd_was_feature(before,after.before_ids[i]))return PRO_TK_GENERAL_ERROR;
    return PRO_TK_NO_ERROR;
}

static ProError ax_delete_backup(ProMdl assembly,ProMdl *copy,ProPath folder,ProMdlName copy_name){
    ProPath profile,pattern,directory,after;
    WIN32_FIND_DATAW file;
    DWORD n=GetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",profile,_countof(profile));
    if(!n||n>=_countof(profile))return PRO_TK_BAD_CONTEXT;
    if(swprintf_s(copy_name,PRO_NAME_SIZE,L"bw_ax_%lx_%llx",GetCurrentProcessId(),GetTickCount64())<0||
       swprintf_s(folder,PRO_PATH_SIZE,L"%ls\\%ls",profile,copy_name)<0)return PRO_TK_BAD_INPUTS;
    if(!CreateDirectoryW(folder,NULL))return PRO_TK_CANT_WRITE;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error)error=ProDirectoryChange(folder);
    if(error)return error;
    error=ProMdlnameCopy(assembly,copy_name,copy);
    if(!error)error=ProMdlnameBackup(*copy,folder);
    if(ProDirectoryChange(directory)||ProDirectoryCurrentGet(after)||_wcsicmp(directory,after))
        return PRO_TK_BAD_CONTEXT;
    if(error)return error;
    if(swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.asm.*",folder,copy_name)<0)return PRO_TK_BAD_INPUTS;
    HANDLE search=FindFirstFileW(pattern,&file);
    if(search==INVALID_HANDLE_VALUE)return PRO_TK_CANT_OPEN;
    FindClose(search);
    return file.nFileSizeHigh||file.nFileSizeLow?PRO_TK_NO_ERROR:PRO_TK_CANT_OPEN;
}

static ProError ax_delete_component(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxComponent component={0};
    FdTxn before={0};
    ProMdl copy=NULL;
    ProAsmcomp trial;
    ProPath backup=L"";
    ProMdlName copy_name=L"";
    ProError error=ax_component_named(assembly,cmd->arg[2],cmd->arg[3],&component);
    if(error)return error;
    error=ax_dependencies(&component.feature,out);
    if(ax_delete_preview(cmd))return error==PRO_TK_BAD_CONTEXT?PRO_TK_NO_ERROR:error;
    if(error||(error=ax_ordinary_direct(&component.feature)))return error;
    if((error=ProSolidFeatVisit((ProSolid)assembly,ax_public_feature,NULL,&before))||
       (error=ax_delete_backup(assembly,&copy,backup,copy_name)))goto done;
    part_text_add(out,L"恢复副本 %ls\\%ls.asm。\n",backup,copy_name);
    int id=component.feature.id;
    if((error=ax_component_feature(copy,id,&trial))||(error=ax_leaf_without_children(&trial))||
       (error=model_remove_feature((ProSolid)copy,id))||(error=part_regenerate(copy))||(error=ax_delete_check(copy,id,&before,1)))goto done;
    error=model_remove_feature((ProSolid)assembly,id);
    if(!error)error=part_regenerate(assembly);
    if(!error)error=ax_delete_check(assembly,id,&before,1);
    if(error){
        if(ax_delete_check(assembly,id,&before,0))ax_uncertain(out);
    }else part_text_add(out,L"已删除实例 %ls，核对其他特征 ID 保持，装配未保存。\n",cmd->arg[2]);
done:
    if(copy)ProMdlErase(copy);
    return error;
}

static void ax_rotate_vector(const double *v,const double *axis,double c,double s,double *out){
    ProVector cross;
    ad_cross(axis,v,cross);
    double along=ad_dot(axis,v);
    for(int i=0;i<3;i++)out[i]=v[i]*c+cross[i]*s+axis[i]*along*(1-c);
}

static ProError ax_component_pose(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent target={0};
    ProAsmcomppath root;
    ProMatrix before,initial,wanted,after;
    ProAsmcompconstraint *rows=NULL;
    int count=0,attempted=0;
    ProError error;
    if((error=ax_component_resolve(assembly,cmd->arg[2],&target))||
       (error=ax_ordinary_direct(&target.feature))||(error=ax_leaf_without_children(&target.feature))||
       (error=ax_actual_pose(assembly,&target.feature,before))||(error=ProAsmcompPositionGet(&target.feature,initial))||
       (error=ax_root_path(assembly,&root)))return error;
    memcpy(wanted,before,sizeof(wanted));
    if(kind==AX_UNFIX_COMPONENT){
        ProAsmcompConstrType type;
        if((error=ax_check_default_set(&target.feature))||
           (error=ax_read_constraints(&target.feature,&root,&rows,&count)))goto done;
        if(count!=1){error=PRO_TK_BAD_CONTEXT;goto done;}
        if((error=ProAsmcompconstraintTypeGet(rows[0],&type)))goto done;
        if(type!=PRO_ASM_FIX){error=PRO_TK_BAD_CONTEXT;goto done;}
        attempted=1;
        error=ProAsmcompAllConstrRemove(&target.feature);
    }else{
        if((error=ax_packaged(&target.feature)))goto done;
        int index=cmd->arg[3][0]-L'X';
        ProVector axis={0},pivot={0},relative,rotation;
        double radians=kind==AX_FLIP_COMPONENT?AS_PI:as_arg(cmd,4)*AS_PI/180.0;
        if(kind==AX_FLIP_COMPONENT){
            memcpy(axis,before[index],sizeof(axis));
            memcpy(pivot,before[3],sizeof(pivot));
        }else{
            axis[index]=1;
            for(int i=0;i<3;i++)pivot[i]=as_arg(cmd,5+i)/mm;
        }
        if(!ad_unit(axis)){error=PRO_TK_BAD_CONTEXT;goto done;}
        double c=cos(radians),s=sin(radians);
        for(int i=0;i<3;i++)ax_rotate_vector(before[i],axis,c,s,wanted[i]);
        ad_sub(before[3],pivot,relative);
        ax_rotate_vector(relative,axis,c,s,rotation);
        for(int i=0;i<3;i++)wanted[3][i]=pivot[i]+rotation[i];
        if(ax_same_pose(before,wanted,mm)){error=PRO_TK_NO_CHANGE;goto done;}
        attempted=1;
    }
    if(!error)error=ax_set_position(assembly,&target.feature,wanted);
    if(!error)error=ax_packaged(&target.feature);
    if(!error)error=ax_actual_pose(assembly,&target.feature,after);
    if(!error&&!ax_same_pose(wanted,after,mm))error=PRO_TK_GENERAL_ERROR;
    if(!error)part_text_add(out,L"%ls 已%ls，已核对组件姿态和位置，未保存。\n",cmd->arg[2],
        kind==AX_UNFIX_COMPONENT?L"解除固定并保持原位":kind==AX_FLIP_COMPONENT?L"绕自身轴翻转 180°":L"旋转");
done:
    if(error&&attempted&&!ax_placement_restored(assembly,&target,&root,rows,count,before,mm)){
        ProError restore=ax_set_position(assembly,&target.feature,initial);
        if(count&&!restore)restore=ProAsmcompConstraintsSet(&root,&target.feature,rows);
        if(count&&!restore)restore=part_regenerate(assembly);
        if(restore||!ax_placement_restored(assembly,&target,&root,rows,count,before,mm))ax_uncertain(out);
        else part_text_add(out,L"已核对原位置和约束恢复。\n");
    }
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}
