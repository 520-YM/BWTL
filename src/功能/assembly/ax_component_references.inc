static ProError ax_map_interface(ProSelection old,AxComponent *replacement,int assembly_side,ProSelection *mapped,double mm){
    ProModelitem item;
    ProFeature owner;
    ProName name=L"";
    wchar_t token[PART_TOKEN_MAX];
    ProError error=ProSelectionModelitemGet(old,&item);
    if(error)return error;
    if(item.owner==replacement->model)
        return ProSelectionAlloc(assembly_side?&replacement->path:NULL,&item,mapped);
    if((error=ProGeomitemFeatureGet(&item,&owner))||(error=ProModelitemNameGet(&owner,name)))return error;
    if(!fd_name(name))return PRO_TK_E_NOT_FOUND;
    if(item.type==PRO_CSYS){
        ProCsysdata before,after;
        ProSelection prior=NULL,local=NULL;
        swprintf_s(token,_countof(token),L"CSYS:%ls",name);
        error=ax_csys_selection(item.owner,token,0,&prior,&before);
        if(!error)error=ax_csys_selection(replacement->model,token,0,&local,&after);
        if(!error){
            ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
            if(!ax_frames_aligned(&before,&after,identity,mm))error=PRO_TK_BAD_CONTEXT;
        }
        if(!error)error=ProSelectionModelitemGet(local,&item);
        if(!error)error=ProSelectionAlloc(assembly_side?&replacement->path:NULL,&item,mapped);
        if(prior)ProSelectionFree(&prior);
        if(local)ProSelectionFree(&local);
        return error;
    }
    AxGeometry before,after;
    swprintf_s(token,_countof(token),L"%ls:%ls",item.type==PRO_AXIS?L"AXIS":L"PLANE",name);
    if((error=ax_geometry(&item,&before))||
       (error=ax_ref_resolve(replacement,token,assembly_side,mapped,&after)))return error;
    ProVector delta,radial;
    ad_sub(after.point,before.point,delta);
    ad_cross(delta,before.direction,radial);
    double distance=before.type==AX_GEOM_PLANE?fabs(ad_dot(delta,before.direction)):sqrt(ad_dot(radial,radial));
    return before.type==after.type&&before.type!=AX_GEOM_CYLINDER&&
        ad_dot(before.direction,after.direction)>1-1e-7&&distance*mm<0.001?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static int ax_selection_on(ProSelection selection,const ProAsmcomppath *path){
    ProAsmcomppath actual={0};
    return selection&&!ProSelectionAsmcomppathGet(selection,&actual)&&actual.table_num==path->table_num&&
        !memcmp(actual.comp_id_table,path->comp_id_table,(size_t)path->table_num*sizeof(path->comp_id_table[0]));
}

static ProError ax_copy_occurrence(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent source={0},old_base={0},new_base={0};
    ProAsmcomppath root;
    ProAsmcomp inserted={0};
    ProAsmcompconstraint *rows=NULL;
    ProMatrix original_pose,actual;
    int count=0,mapped_count=0;
    double model_mm;
    ProError error;
#define COPY_STEP(call) do{if((error=(call))){trace("assembly copy " #call,error);goto done;}}while(0)
    COPY_STEP(ax_root_path(assembly,&root));
    COPY_STEP(ax_component_resolve(assembly,cmd->arg[2],&source));
    COPY_STEP(ax_ordinary_direct(&source.feature));
    COPY_STEP(ax_check_default_set(&source.feature));
    COPY_STEP(ax_ref_occurrence(assembly,cmd->arg[3],&old_base));
    COPY_STEP(ax_ref_occurrence(assembly,cmd->arg[4],&new_base));
    COPY_STEP(qa_mm_per_unit(new_base.model,&model_mm));
    if(!part_close(mm,model_mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    COPY_STEP(ax_actual_pose(assembly,&source.feature,original_pose));
    COPY_STEP(ax_read_constraints(&source.feature,&root,&rows,&count));
    if(!count||count>AX_PLACEMENT_LIMIT){error=PRO_TK_BAD_CONTEXT;goto done;}
    COPY_STEP(ax_simple_rows(rows,count));
    for(int i=0;i<count;i++){
        ProSelection old=NULL,mapped=NULL,component_ref=NULL,local=NULL;
        ProDatumside side,component_side;
        ProModelitem item;
        COPY_STEP(ProAsmcompconstraintCompreferenceGet(rows[i],&component_ref,&component_side));
        COPY_STEP(ProSelectionModelitemGet(component_ref,&item));
        COPY_STEP(ProSelectionAlloc(NULL,&item,&local));
        error=ProAsmcompconstraintCompreferenceSet(rows[i],local,component_side);
        ProSelectionFree(&local);
        if(error)goto done;
        COPY_STEP(ProAsmcompconstraintAsmreferenceGet(rows[i],&old,&side));
        if(!ax_selection_on(old,&old_base.path))continue;
        error=ax_map_interface(old,&new_base,1,&mapped,mm);
        if(!error)error=ProAsmcompconstraintAsmreferenceSet(rows[i],mapped,side);
        if(mapped)ProSelectionFree(&mapped);
        if(error)goto done;
        mapped_count++;
    }
    if(!mapped_count){error=PRO_TK_E_NOT_FOUND;goto done;}
    COPY_STEP(ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)source.model,original_pose,&inserted));
    COPY_STEP(ProAsmcompConstraintsSet(&root,&inserted,rows));
    COPY_STEP(part_regenerate(assembly));
    COPY_STEP(ax_compare_constraints(&inserted,&root,rows,count,mm));
    COPY_STEP(ax_actual_pose(assembly,&inserted,actual));
    for(int i=0;i<count;i++){
        ProAsmcompConstrType type;
        COPY_STEP(ProAsmcompconstraintTypeGet(rows[i],&type));
        if(type!=PRO_ASM_CSYS)COPY_STEP(ax_verify_relation(rows[i],actual,mm));
    }
    COPY_STEP(ax_actual_pose(assembly,&source.feature,actual));
    if(!ax_same_pose(original_pose,actual,mm)){ax_uncertain(out);error=PRO_TK_GENERAL_ERROR;goto done;}
    part_text_add(out,L"已复制 %ls 为 ID:%d，保留 %d 条定位约束，%d 条重新绑定，未保存。\n",
        cmd->arg[2],inserted.id,count,mapped_count);
done:
    if(error)ax_discard_occurrence(assembly,&inserted,out);
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
#undef COPY_STEP
}

static ProError ax_replacement_check(ProMdl assembly,AxComponent *target,ProAsmcomppath *root,
    ProAsmcompconstraint *rows,int count,ProMatrix before,double mm){
    return ax_placement_restored(assembly,target,root,rows,count,before,mm)?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError ax_replace_occurrence(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent target={0},replacement={0},staged={0};
    ProAsmcomppath root;
    ProAsmcompconstraint *rows=NULL;
    ProMdl copy=NULL;
    ProPath backup;
    ProMdlName copy_name;
    ProMatrix before;
    ProMdl actual=NULL;
    FdTxn feature_ids={0};
    int count=0,loaded=0,delete_attempted=0;
    ProError error;
    double replacement_mm;
#define REPLACE_STEP(call) do{if((error=(call))){trace("assembly replace " #call,error);goto done;}}while(0)
    REPLACE_STEP(ax_root_path(assembly,&root));
    REPLACE_STEP(ax_component_named(assembly,cmd->arg[2],cmd->arg[3],&target));
    REPLACE_STEP(ax_ordinary_direct(&target.feature));
    REPLACE_STEP(ax_dependencies(&target.feature,out));
    REPLACE_STEP(ax_check_default_set(&target.feature));
    REPLACE_STEP(ax_actual_pose(assembly,&target.feature,before));
    REPLACE_STEP(ax_load_part(cmd->arg[4],&replacement.model,&loaded));
    REPLACE_STEP(part_health(replacement.model));
    REPLACE_STEP(qa_mm_per_unit(replacement.model,&replacement_mm));
    if(!part_close(mm,replacement_mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    REPLACE_STEP(ax_read_constraints(&target.feature,&root,&rows,&count));
    REPLACE_STEP(ax_simple_rows(rows,count));
    if(count>AX_PLACEMENT_LIMIT){error=PRO_TK_BAD_CONTEXT;goto done;}
    if(!count)REPLACE_STEP(ax_packaged(&target.feature));
    for(int i=0;i<count;i++){
        ProSelection old=NULL,mapped=NULL;
        ProDatumside side;
        REPLACE_STEP(ProAsmcompconstraintCompreferenceGet(rows[i],&old,&side));
        error=ax_map_interface(old,&replacement,0,&mapped,mm);
        if(!error)error=ProAsmcompconstraintCompreferenceSet(rows[i],mapped,side);
        if(mapped)ProSelectionFree(&mapped);
        if(error){
            part_text_add(out,L"第 %d 条约束未找到同名且几何一致的零件参照。\n",i+1);
            goto done;
        }
    }
    REPLACE_STEP(ax_delete_backup(assembly,&copy,backup,copy_name));
    part_text_add(out,L"恢复副本 %ls\\%ls.asm。\n",backup,copy_name);
    REPLACE_STEP(ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)replacement.model,before,&staged.feature));
    if(count)REPLACE_STEP(ProAsmcompConstraintsSet(&root,&staged.feature,rows));
    REPLACE_STEP(part_regenerate(assembly));
    REPLACE_STEP(ProAsmcompMdlGet(&staged.feature,&actual));
    if(actual!=replacement.model){error=PRO_TK_GENERAL_ERROR;goto done;}
    REPLACE_STEP(ax_replacement_check(assembly,&staged,&root,rows,count,before,mm));
    REPLACE_STEP(ProSolidFeatVisit((ProSolid)assembly,ax_public_feature,NULL,&feature_ids));
    delete_attempted=1;
    REPLACE_STEP(model_remove_feature((ProSolid)assembly,target.feature.id));
    REPLACE_STEP(part_regenerate(assembly));
    REPLACE_STEP(ax_delete_check(assembly,target.feature.id,&feature_ids,1));
    REPLACE_STEP(ax_replacement_check(assembly,&staged,&root,rows,count,before,mm));
    part_text_add(out,L"已将 %ls 的 %ls 替换为 %ls。新实例 ID:%d，位置和 %d 条定位约束已核对，顶层装配未保存。\n",
        cmd->arg[2],cmd->arg[3],cmd->arg[4],staged.feature.id,count);
done:
    if(error){
        if(delete_attempted&&ax_delete_check(assembly,target.feature.id,&feature_ids,0))ax_uncertain(out);
        else ax_discard_occurrence(assembly,&staged.feature,out);
    }
    if(rows)ProAsmcompconstraintArrayFree(rows);
    if(copy)ProMdlErase(copy);
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(replacement.model);
    return error;
#undef REPLACE_STEP
}

typedef struct {
    AxComponent *base;
    ProAsmcompconstraint row;
    ProDatumside side;
    int assembly_side,limit,matched,seen;
    ProMatrix pose;
    double mm;
    ProSelection chosen;
    PartText *out;
} AxCandidates;

static ProError ax_try_candidate(ProGeomitem *item,ProError status,ProAppData data){
    AxCandidates *search=data;
    if(status)return status;
    if(++search->seen>128){search->limit=1;return PRO_TK_USER_ABORT;}
    AxGeometry geometry;
    if(ax_geometry(item,&geometry))return PRO_TK_NO_ERROR;
    ProSelection pick=NULL;
    ProError error=ProSelectionAlloc(search->assembly_side?&search->base->path:NULL,item,&pick);
    if(error)return error;
    error=search->assembly_side?ProAsmcompconstraintAsmreferenceSet(search->row,pick,search->side):
        ProAsmcompconstraintCompreferenceSet(search->row,pick,search->side);
    if(!error&&!ax_verify_relation(search->row,search->pose,search->mm)&&
       (!search->chosen||!ax_selection_equal(search->chosen,pick,0))){
        search->matched++;
        ProFeature owner;
        ProName name=L"";
        if(item->type==PRO_AXIS&&!ProGeomitemFeatureGet(item,&owner)&&
           !ProModelitemNameGet(&owner,name)&&fd_name(name))part_text_add(search->out,L"候选参照 AXIS:%ls\n",name);
        else part_text_add(search->out,L"候选参照 %ls:%d\n",item->type==PRO_AXIS?L"轴 ID":L"SURFACE",item->id);
        if(!search->chosen){search->chosen=pick;pick=NULL;}
    }
    if(pick)ProSelectionFree(&pick);
    return error;
}

static ProError ax_candidate_feature(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProFeatStatus state;
    ProError error=ProFeatureStatusGet(feature,&state);
    if(error||state!=PRO_FEAT_ACTIVE)return error;
    error=ProFeatureGeomitemVisit(feature,PRO_SURFACE,ax_try_candidate,NULL,data);
    if(!error||error==PRO_TK_E_NOT_FOUND)error=ProFeatureGeomitemVisit(feature,PRO_AXIS,ax_try_candidate,NULL,data);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}

static ProError ax_change_occurrence_ref(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent target={0},base={0};
    ProAsmcomppath root;
    ProAsmcompconstraint *rows=NULL,*original=NULL;
    ProSelection selected=NULL,old_selection=NULL;
    ProDatumside side=PRO_DATUM_SIDE_NONE;
    ProAsmcompConstrType type;
    ProMatrix before,after;
    ProWVerstamp stamp=NULL;
    int index,count=0,original_count=0,attempted=0;
    int assembly_side=!wcscmp(cmd->arg[4],L"BASE");
    double ref_mm;
    AxGeometry shape;
    ProError error;
#define REF_STEP(call) do{if((error=(call))){trace("assembly ref edit " #call,error);goto done;}}while(0)
    part_integer(cmd->arg[3],&index);index--;
    REF_STEP(ax_root_path(assembly,&root));
    if(kind==AX_REPAIR_REF){
        ProIdTable ids={0};
        ax_path_parse(cmd->arg[2],ids);
        REF_STEP(ax_component_feature(assembly,ids[0],&target.feature));
        REF_STEP(ProAsmcompMdlGet(&target.feature,&target.model));
    }else REF_STEP(ax_component_resolve(assembly,cmd->arg[2],&target));
    REF_STEP(ax_ordinary_direct(&target.feature));
    REF_STEP(ax_dependencies(&target.feature,out));
    REF_STEP(ax_check_default_set(&target.feature));
    REF_STEP(ax_read_constraints(&target.feature,&root,&original,&original_count));
    REF_STEP(ax_read_constraints(&target.feature,&root,&rows,&count));
    if(count!=original_count||index>=count||count>AX_PLACEMENT_LIMIT){error=PRO_TK_E_NOT_FOUND;goto done;}
    REF_STEP(ax_simple_rows(rows,count));
    REF_STEP(ProAsmcompconstraintTypeGet(rows[index],&type));
    if(type==PRO_ASM_CSYS){error=PRO_TK_INVALID_TYPE;goto done;}
    if(assembly_side)REF_STEP(ax_ref_occurrence(assembly,cmd->arg[5],&base));
    else base=target;
    if(assembly_side&&base.path.table_num&&base.path.comp_id_table[0]==target.feature.id){error=PRO_TK_BAD_CONTEXT;goto done;}
    REF_STEP(qa_mm_per_unit(base.model,&ref_mm));
    if(!part_close(mm,ref_mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    if(assembly_side)ProAsmcompconstraintAsmreferenceGet(original[index],&old_selection,&side);
    else ProAsmcompconstraintCompreferenceGet(original[index],&old_selection,&side);
    if(kind==AX_EDIT_REF&&(!old_selection||ProSelectionVerify(old_selection))){error=PRO_TK_BAD_CONTEXT;goto done;}
    if(!wcscmp(cmd->arg[6],L"AUTO")){
        AxCandidates candidates={0};
        candidates.base=&base;candidates.row=rows[index];candidates.side=side;candidates.assembly_side=assembly_side;
        candidates.mm=mm;candidates.out=out;
        REF_STEP(ax_actual_pose(assembly,&target.feature,candidates.pose));
        error=ProSolidFeatVisit((ProSolid)base.model,ax_candidate_feature,NULL,&candidates);
        selected=candidates.chosen;
        if(error||candidates.limit||candidates.matched!=1){
            part_text_add(out,L"无法唯一确定替代参照（找到 %d 个候选）。请指定实际参照。\n",candidates.matched);
            if(!error)error=PRO_TK_E_AMBIGUOUS;
            goto done;
        }
    }else REF_STEP(ax_ref_resolve(&base,cmd->arg[6],assembly_side,&selected,&shape));
    ProSelection opposite=NULL;
    ProDatumside opposite_side;
    ProModelitem selected_item,opposite_item;
    AxGeometry opposite_geometry;
    if(assembly_side)REF_STEP(ProAsmcompconstraintCompreferenceGet(rows[index],&opposite,&opposite_side));
    else REF_STEP(ProAsmcompconstraintAsmreferenceGet(rows[index],&opposite,&opposite_side));
    if(!opposite||!selected){error=PRO_TK_E_NOT_FOUND;goto done;}
    REF_STEP(ProSelectionModelitemGet(selected,&selected_item));
    REF_STEP(ProSelectionModelitemGet(opposite,&opposite_item));
    REF_STEP(ax_geometry(&selected_item,&shape));
    REF_STEP(ax_geometry(&opposite_item,&opposite_geometry));
    if(shape.type!=opposite_geometry.type||
       (type==PRO_ASM_INSERT&&shape.type!=AX_GEOM_CYLINDER)||
       (type==PRO_ASM_ALIGN&&shape.type==AX_GEOM_CYLINDER)||
       (type!=PRO_ASM_ALIGN&&type!=PRO_ASM_INSERT&&shape.type!=AX_GEOM_PLANE)){
        error=PRO_TK_INVALID_TYPE;goto done;
    }
    if(old_selection&&!ProSelectionVerify(old_selection)&&ax_selection_equal(old_selection,selected,assembly_side?0:target.feature.id)){
        error=PRO_TK_NO_CHANGE;goto done;
    }
    if(assembly_side)REF_STEP(ProAsmcompconstraintAsmreferenceSet(rows[index],selected,side));
    else REF_STEP(ProAsmcompconstraintCompreferenceSet(rows[index],selected,side));
    REF_STEP(ProMdlVerstampGet(assembly,&stamp));
    int healthy=!part_health(assembly);
    int had_pose=!ax_actual_pose(assembly,&target.feature,before);
    attempted=1;
    REF_STEP(ProAsmcompConstraintsSet(&root,&target.feature,rows));
    REF_STEP(part_regenerate(assembly));
    REF_STEP(ax_compare_constraints(&target.feature,&root,rows,count,mm));
    REF_STEP(ax_actual_pose(assembly,&target.feature,after));
    REF_STEP(ax_verify_relation(rows[index],after,mm));
    part_text_add(out,L"%ls 的第 %d 条约束%ls，其他约束保持，重生成通过，未保存。\n",
        cmd->arg[2],index+1,kind==AX_REPAIR_REF?L"已修复参照":L"已改绑参照");
done:
    if(error&&attempted){
        ProWVerstamp current=NULL;
        int restored=!ProMdlVerstampGet(assembly,&current)&&ProVerstampEqual(stamp,current);
        if(current)ProVerstampFree(&current);
        if(!restored&&healthy&&had_pose)
            restored=ax_placement_restored(assembly,&target,&root,original,count,before,mm);
        if(!restored){
            ProError undo=healthy?ProAsmcompConstraintsSet(&root,&target.feature,original):PRO_TK_BAD_CONTEXT;
            if(!undo)undo=part_regenerate(assembly);
            restored=!undo&&healthy&&had_pose&&ax_placement_restored(assembly,&target,&root,original,count,before,mm);
        }
        if(restored)part_text_add(out,L"已核对原定位状态恢复。\n");
        else ax_uncertain(out);
    }
    if(stamp)ProVerstampFree(&stamp);
    if(selected)ProSelectionFree(&selected);
     if(rows)ProAsmcompconstraintArrayFree(rows);
    if(original)ProAsmcompconstraintArrayFree(original);
    return error;
#undef REF_STEP
}
