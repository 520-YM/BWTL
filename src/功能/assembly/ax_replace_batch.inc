typedef struct {
    AxComponent old,staged;
    ProAsmcompconstraint *rows;
    ProMatrix pose;
    int count;
} AxReplacement;

static ProError ax_multi_delete(ProMdl assembly,int *ids){
    ProFeatureDeleteOptions *options=NULL;
    ProError error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        options[0]=PRO_FEAT_DELETE_NO_OPTS;
        error=ProFeatureWithoptionsDelete((ProSolid)assembly,ids,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
    }
    if(options)ProArrayFree((ProArray *)&options);
    return error;
}

static ProError ax_verify_multi_delete(ProMdl assembly,const FdTxn *before,const int *removed,int count){
    FdTxn after={0};
    ProError error=part_health(assembly);
    if(!error)error=ProSolidFeatVisit((ProSolid)assembly,ax_public_feature,NULL,&after);
    if(error)return error;
    if(after.before_count!=before->before_count-count)return PRO_TK_GENERAL_ERROR;
    for(int i=0;i<after.before_count;i++){
        if(!fd_was_feature(before,after.before_ids[i]))return PRO_TK_GENERAL_ERROR;
        for(int j=0;j<count;j++)if(after.before_ids[i]==removed[j])return PRO_TK_GENERAL_ERROR;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_batch_replace_rows(AxReplacement *item,AxComponent *new_model,ProAsmcomppath *root,double mm){
    ProError error=ax_read_constraints(&item->old.feature,root,&item->rows,&item->count);
    if(error)return error;
    if(!item->count)return ax_packaged(&item->old.feature);
    if(item->count>AX_PLACEMENT_LIMIT)return PRO_TK_BAD_CONTEXT;
    if(item->count==1){
        ProAsmcompConstrType type;
        if((error=ProAsmcompconstraintTypeGet(item->rows[0],&type)))return error;
        if(type==PRO_ASM_FIX)return PRO_TK_NO_ERROR;
    }
    if((error=ax_simple_rows(item->rows,item->count)))return error;
    for(int i=0;i<item->count;i++){
        ProSelection old=NULL,mapped=NULL;
        ProDatumside side;
        error=ProAsmcompconstraintCompreferenceGet(item->rows[i],&old,&side);
        if(!error)error=ax_map_interface(old,new_model,0,&mapped,mm);
        if(!error)error=ProAsmcompconstraintCompreferenceSet(item->rows[i],mapped,side);
        if(mapped)ProSelectionFree(&mapped);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_batch_replacement_pose(ProMdl assembly,AxReplacement *item,ProMdl model,ProAsmcomppath *root,double mm){
    ProMdl actual=NULL;
    ProError error=ProAsmcompMdlGet(&item->staged.feature,&actual);
    if(error||actual!=model)return error?error:PRO_TK_GENERAL_ERROR;
    return ax_replacement_check(assembly,&item->staged,root,item->rows,item->count,item->pose,mm);
}

static ProError ax_replace_batch(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxReplacement items[12]={0};
    AxComponent new_model={0};
    ProAsmcomppath root;
    ProMdl copy=NULL;
    ProPath backup;
    ProMdlName backup_name;
    FdTxn before_delete={0};
    int *ids=NULL,loaded=0,delete_attempted=0,count=cmd->count-8;
    ProError error;
#define RB_STEP(call) do{if((error=(call))){trace("assembly batch replace " #call,error);goto done;}}while(0)
    RB_STEP(ax_root_path(assembly,&root));
    RB_STEP(ax_batch_model(cmd->arg[3],mm,&new_model.model,&loaded));
    RB_STEP(ProArrayAlloc(count,sizeof(*ids),1,(ProArray *)&ids));
    for(int i=0;i<count;i++){
        AxReplacement *item=&items[i];
        RB_STEP(ax_component_named(assembly,cmd->arg[i+8],cmd->arg[2],&item->old));
        ids[i]=item->old.feature.id;
        for(int j=0;j<i;j++)if(ids[i]==ids[j]){error=PRO_TK_BAD_INPUTS;goto done;}
        RB_STEP(ax_ordinary_direct(&item->old.feature));
        RB_STEP(ax_dependencies(&item->old.feature,out));
        RB_STEP(ax_check_default_set(&item->old.feature));
        RB_STEP(ax_actual_pose(assembly,&item->old.feature,item->pose));
        RB_STEP(ax_batch_replace_rows(item,&new_model,&root,mm));
    }
    AxHardwareSite before,after;
    RB_STEP(ax_hw_site(&items[0].old,cmd->arg[4],cmd->arg[5],mm,&before));
    RB_STEP(ax_hw_site(&new_model,cmd->arg[6],cmd->arg[7],mm,&after));
    if(before.bore||after.bore||fabs(before.cylinder.radius-after.cylinder.radius)*mm>0.0005||
       !ad_near(before.contact,after.contact,0.001/mm)||
       ad_dot(before.plane.direction,after.plane.direction)<1-1e-7){
        part_text_add(out,L"新旧件的杆径、轴线或承压面安装接口不一致，批量替换未执行。\n");
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    RB_STEP(ax_delete_backup(assembly,&copy,backup,backup_name));
    part_text_add(out,L"恢复副本 %ls\\%ls.asm。\n",backup,backup_name);
    for(int i=0;i<count;i++){
        AxReplacement *item=&items[i];
        RB_STEP(ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)new_model.model,item->pose,&item->staged.feature));
        if(item->count)RB_STEP(ProAsmcompConstraintsSet(&root,&item->staged.feature,item->rows));
    }
    RB_STEP(part_regenerate(assembly));
    for(int i=0;i<count;i++)RB_STEP(ax_batch_replacement_pose(assembly,&items[i],new_model.model,&root,mm));
    RB_STEP(ProSolidFeatVisit((ProSolid)assembly,ax_public_feature,NULL,&before_delete));
    delete_attempted=1;
    RB_STEP(ax_multi_delete(assembly,ids));
    RB_STEP(part_regenerate(assembly));
    RB_STEP(ax_verify_multi_delete(assembly,&before_delete,ids,count));
    for(int i=0;i<count;i++)RB_STEP(ax_batch_replacement_pose(assembly,&items[i],new_model.model,&root,mm));
    part_text_add(out,L"已批量替换 %d 个实例，位置和原定位已核对，顶层装配未保存。\n",count);
    for(int i=0;i<count;i++)part_text_add(out,L"ID:%d → ID:%d\n",ids[i],items[i].staged.feature.id);
done:
    if(error){
        if(delete_attempted&&ax_verify_multi_delete(assembly,&before_delete,ids,0))ax_uncertain(out);
        else for(int i=count-1;i>=0;i--)ax_discard_occurrence(assembly,&items[i].staged.feature,out);
    }
    for(int i=0;i<count;i++)if(items[i].rows)ProAsmcompconstraintArrayFree(items[i].rows);
    if(ids)ProArrayFree((ProArray *)&ids);
    if(copy)ProMdlErase(copy);
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(new_model.model);
    return error;
#undef RB_STEP
}
