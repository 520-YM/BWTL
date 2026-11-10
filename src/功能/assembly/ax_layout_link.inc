static ProError ax_layout_link(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent source={0},target={0};
    ProCsysdata local,root_frame,target_frame;
    ProSelection temp=NULL,root_selection=NULL,part_selection=NULL;
    ProAsmcompconstraint row=NULL,*constraints=NULL;
    ProAsmcomppath root;
    ProMatrix source_pose,before,initial,actual;
    ProBoolean skeleton=PRO_B_FALSE;
    ProModelitem item;
    double units;
    int attempted=0,source_order,target_order;
    ProError error;
#define LK(call) do{if((error=(call))){trace("layout link " #call,error);goto done;}}while(0)
    LK(ax_component_resolve(assembly,cmd->arg[2],&source));
    LK(ax_component_resolve(assembly,cmd->arg[3],&target));
    LK(ProMdlIsSkeleton(source.model,&skeleton));
    LK(ax_ordinary_direct(&target.feature));
    LK(ax_leaf_without_children(&target.feature));
    LK(ax_packaged(&target.feature));
    LK(ProFeatureNumberGet(&source.feature,&source_order));
    LK(ProFeatureNumberGet(&target.feature,&target_order));
    LK(qa_mm_per_unit(source.model,&units));
    if(!skeleton||source_order>=target_order||!part_close(units,mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    LK(qa_mm_per_unit(target.model,&units));
    if(!part_close(units,mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    LK(ax_device_pose(&source,source_pose));
    LK(ax_actual_pose(assembly,&target.feature,before));
    LK(ProAsmcompPositionGet(&target.feature,initial));
    LK(ax_csys_selection(source.model,cmd->arg[4],0,&temp,&local));
    LK(ProSelectionModelitemGet(temp,&item));
    LK(ProSelectionAlloc(&source.path,&item,&root_selection));
    LK(ax_csys_selection(target.model,cmd->arg[5],0,&part_selection,&target_frame));
    LK(ProPntTrfEval(local.origin,source_pose,root_frame.origin));
    LK(ProVectorTrfEval(local.x_vector,source_pose,root_frame.x_vector));
    LK(ProVectorTrfEval(local.y_vector,source_pose,root_frame.y_vector));
    LK(ProVectorTrfEval(local.z_vector,source_pose,root_frame.z_vector));
    LK(ax_root_path(assembly,&root));
    LK(ProAsmcompconstraintAlloc(&row));
    LK(ProAsmcompconstraintTypeSet(row,PRO_ASM_CSYS));
    LK(ProAsmcompconstraintAsmreferenceSet(row,root_selection,PRO_DATUM_SIDE_NONE));
    LK(ProAsmcompconstraintCompreferenceSet(row,part_selection,PRO_DATUM_SIDE_NONE));
    LK(ProArrayAlloc(1,sizeof(*constraints),1,(ProArray *)&constraints));
    constraints[0]=row;
    attempted=1;
    LK(ProAsmcompConstraintsSet(&root,&target.feature,constraints));
    LK(part_regenerate(assembly));
    LK(ax_compare_constraints(&target.feature,&root,constraints,1,mm));
    LK(ax_actual_pose(assembly,&target.feature,actual));
    if(!ax_frames_aligned(&root_frame,&target_frame,actual,mm)){error=PRO_TK_GENERAL_ERROR;goto done;}
    part_text_add(out,L"%ls 已与骨架 %ls 的 %ls 建立 Creo 坐标系重合约束，读回和实际姿态核对通过。未保存。\n",
        cmd->arg[3],cmd->arg[2],cmd->arg[4]);
done:
    if(error&&attempted){
        int unchanged=!ax_packaged(&target.feature)&&!ax_actual_pose(assembly,&target.feature,actual)&&
            ax_same_pose(before,actual,mm)&&!part_health(assembly);
        if(!unchanged&&(ProAsmcompAllConstrRemove(&target.feature)||
           ax_set_position(assembly,&target.feature,initial)||ax_packaged(&target.feature)||
           ax_actual_pose(assembly,&target.feature,actual)||!ax_same_pose(before,actual,mm)))ax_uncertain(out);
    }
    if(constraints)ProAsmcompconstraintArrayFree(constraints);
    else if(row)ProAsmcompconstraintFree(row);
    if(part_selection)ProSelectionFree(&part_selection);
    if(root_selection)ProSelectionFree(&root_selection);
    if(temp)ProSelectionFree(&temp);
    return error;
#undef LK
}

typedef struct {ProMatrix pose;ProWVerstamp stamp;int present;} AxLayoutBefore;

static ProError ax_layout_measure(ProMdl assembly,AxOccurrence *entry,ProMatrix pose){
    ProAsmcomppath path;
    ProIdTable ids={0};
    memcpy(ids,entry->ids,(size_t)entry->depth*sizeof(ids[0]));
    ProError error=ProAsmcomppathInit((ProSolid)assembly,ids,entry->depth,&path);
    return error?error:ProAsmcomppathTrfGet(&path,PRO_B_TRUE,pose);
}

static ProError ax_layout_dimension(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent owner={0};
    AxScan *before=NULL;
    AxLayoutBefore *measure=NULL;
    UxDimension dim;
    ProBoolean driven=PRO_B_FALSE,skeleton=PRO_B_FALSE;
    ProDimensiontype type;
    ProError error;
    double scale,actual,previous=0;
    double old_mm=as_arg(cmd,4),new_mm=as_arg(cmd,5);
    int attempted=0,moved=0,model_changes=0;
#define LD(call) do{if((error=(call))){trace("layout dimension " #call,error);goto done;}}while(0)
    LD(ax_ref_occurrence(assembly,cmd->arg[2],&owner));
    if(owner.path.table_num){
        LD(ProMdlIsSkeleton(owner.model,&skeleton));
        if(!skeleton||((!_wcsicmp(cmd->arg[3],L"BW_WIDTH")||!_wcsicmp(cmd->arg[3],L"BW_DEPTH")||
           !_wcsicmp(cmd->arg[3],L"BW_HEIGHT"))&&new_mm<20)){error=PRO_TK_BAD_CONTEXT;goto done;}
    }
    LD(qa_mm_per_unit(owner.model,&scale));
    LD(ux_dimension(owner.model,cmd->arg[3],&dim));
    LD(ProDimensionTypeGet(&dim.dim,&type));
    LD(ProDimensionIsReldriven(&dim.dim,&driven));
    previous=dim.value;
    if(type!=PRODIMTYPE_LINEAR||driven||!part_close(previous*scale,old_mm)){
        part_text_add(out,L"当前尺寸不是可写的线性控制尺寸，或实际旧值与请求不符。\n");
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    if(part_close(old_mm,new_mm)){error=PRO_TK_NO_CHANGE;goto done;}
    LD(ax_scan(assembly,1,&before));
    measure=calloc(before->count?before->count:1,sizeof(*measure));
    if(!measure){error=PRO_TK_OUT_OF_MEMORY;goto done;}
    for(int i=0;i<before->count;i++){
        AxOccurrence *item=&before->occurrences[i];
        if(!item->model||item->state!=PRO_FEAT_ACTIVE)continue;
        LD(ax_layout_measure(assembly,item,measure[i].pose));
        LD(ProMdlVerstampGet(item->model,&measure[i].stamp));
        measure[i].present=1;
    }
    attempted=1;
    LD(ProDimensionValueSet(&dim.dim,new_mm/scale));
    if(owner.model!=assembly)LD(part_regenerate(owner.model));
    LD(part_regenerate(assembly));
    LD(ProDimensionValueGet(&dim.dim,&actual));
    if(!part_close(actual*scale,new_mm)){error=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<before->count;i++){
        if(!measure[i].present)continue;
        AxOccurrence *entry=&before->occurrences[i];
        ProMatrix pose;
        ProWVerstamp stamp=NULL;
        wchar_t label[256];
        LD(ax_layout_measure(assembly,entry,pose));
        LD(ProMdlVerstampGet(entry->model,&stamp));
        int changed=!ProVerstampEqual(stamp,measure[i].stamp);
        ProVerstampFree(&stamp);
        int shifted=!ax_same_pose(pose,measure[i].pose,mm);
        moved+=shifted;model_changes+=changed;
        if((shifted||changed)&&ax_path_label(entry->ids,entry->depth,label,_countof(label)))
            part_text_add(out,L"%ls %ls%ls%ls。\n",label,entry->name,shifted?L" 位置/方向已变":L"",
                changed?L" 模型版本戳已变":L"");
    }
    part_text_add(out,L"%ls 已从 %.8g 改为 %.8g mm。定位发生变化 %d 个实例，模型版本戳变化 %d 个实例，重生成通过，未保存。\n",
        cmd->arg[3],old_mm,actual*scale,moved,model_changes);
done:
    if(error&&attempted){
        int bad=ProDimensionValueSet(&dim.dim,previous)!=0;
        if(owner.model!=assembly&&part_regenerate(owner.model))bad=1;
        if(part_regenerate(assembly)||ProDimensionValueGet(&dim.dim,&actual)||!part_close(actual,previous))bad=1;
        for(int i=0;i<before->count;i++)if(measure[i].present){
            ProMatrix pose;
            if(ax_layout_measure(assembly,&before->occurrences[i],pose)||!ax_same_pose(pose,measure[i].pose,mm))bad=1;
        }
        if(bad)ax_uncertain(out);
    }
    if(measure)for(int i=0;i<before->count;i++)if(measure[i].stamp)ProVerstampFree(&measure[i].stamp);
    free(measure);free(before);
    return error;
#undef LD
}