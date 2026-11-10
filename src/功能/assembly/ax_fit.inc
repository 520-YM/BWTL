static int ax_same_occurrence(const AxComponent *a,const AxComponent *b){
    return a->path.table_num==b->path.table_num&&
        !memcmp(a->path.comp_id_table,b->path.comp_id_table,a->path.table_num*sizeof(int));
}

static ProError ax_part_selection(AxComponent *component,ProSelection *selection){
    ProMdlType type;
    ProModelitem item;
    ProMassProperty geometry;
    ProError error=ProMdlTypeGet(component->model,&type);
    if(error)return error;
    if(type!=PRO_MDL_PART)return PRO_TK_BAD_CONTEXT;
    if((error=part_health(component->model))||
       (error=qa_geometry_mass(component->model,&geometry))||
       (error=ProMdlToModelitem(component->model,&item)))return error;
    return ProSelectionAlloc(&component->path,&item,selection);
}

static ProError ax_interference_volume(ProSelection a,ProSelection b,double *volume){
    ProInterferenceData data=NULL;
    *volume=0;
    double distance;
    ProBoolean interfering=PRO_B_FALSE;
    Pro3dPnt nearest[2];
    ProError clearance=ProFitClearanceCompute(a,PRO_FIT_INCLUDE_NONE,b,PRO_FIT_INCLUDE_NONE,
        PRO_FIT_WHOLE_SURFACE,NULL,PRO_B_FALSE,&distance,NULL,&interfering,nearest);
    if(!clearance&&!interfering&&isfinite(distance)&&distance>=0)return PRO_TK_NO_ERROR;
    ProError error=ProFitInterferenceCompute(a,b,PRO_B_FALSE,PRO_B_FALSE,&data);
    if(error)trace("assembly native interference",error);
    if(!error&&data)error=ProFitInterferencevolumeCompute(data,volume);
    if(error&&data)trace("assembly native interference volume",error);
    if(data)ProInterferenceDataFree(data);
    if(!error&&(!isfinite(*volume)||*volume<0))error=PRO_TK_GENERAL_ERROR;
    return error;
}

static ProError ax_fit_pair(ProMdl assembly,const PartCommand *cmd,double scale,PartText *out){
    AxComponent a={0},b={0};
    ProSelection selections[2]={NULL,NULL};
    ProError error;
    if((error=ax_component_resolve(assembly,cmd->arg[2],&a))||
       (error=ax_component_resolve(assembly,cmd->arg[3],&b)))goto done;
    if(ax_same_occurrence(&a,&b)){
        part_text_add(out,L"请选择两个不同的组件实例路径。\n");
        error=PRO_TK_BAD_INPUTS;
        goto done;
    }
    if((error=ax_part_selection(&a,&selections[0]))||(error=ax_part_selection(&b,&selections[1])))goto done;
    if(ax_kind(cmd->arg[0])==AX_CLEARANCE){
        double distance,limit=wcstod(cmd->arg[4],NULL);
        ProBoolean interfering=PRO_B_FALSE;
        Pro3dPnt nearest[2];
        error=ProFitClearanceCompute(selections[0],PRO_FIT_INCLUDE_NONE,selections[1],PRO_FIT_INCLUDE_NONE,
            PRO_FIT_WHOLE_SURFACE,NULL,PRO_B_FALSE,&distance,NULL,&interfering,nearest);
        if(error)goto done;
        if(interfering){
            part_text_add(out,L"%ls 与 %ls 存在干涉，最小间隙要求 %.9g mm 未满足。\n",
                cmd->arg[2],cmd->arg[3],limit);
        }else{
            if(!isfinite(distance)||distance<0){error=PRO_TK_GENERAL_ERROR;goto done;}
            distance*=scale;
            part_text_add(out,L"%ls 与 %ls 的最小间隙 %.10g mm，要求 ≥ %.9g mm，%ls。\n"
                L"最近点 (%.8g, %.8g, %.8g) 与 (%.8g, %.8g, %.8g) mm，使用顶层坐标。\n",
                cmd->arg[2],cmd->arg[3],distance,limit,qa_at_least(distance,limit)?L"符合":L"低于要求",
                nearest[0][0]*scale,nearest[0][1]*scale,nearest[0][2]*scale,
                nearest[1][0]*scale,nearest[1][1]*scale,nearest[1][2]*scale);
        }
    }else{
        double volume;
        error=ax_interference_volume(selections[0],selections[1],&volume);
        if(!error)part_text_add(out,L"%ls 与 %ls %ls，实体干涉体积 %.12g mm³。\n",
            cmd->arg[2],cmd->arg[3],volume>0?L"存在干涉":L"未检出正体积干涉",volume*scale*scale*scale);
    }
done:
    for(int i=0;i<2;i++)if(selections[i])ProSelectionFree(&selections[i]);
    return error;
}

static ProError ax_fit_all(ProMdl assembly,double scale,PartText *out){
    AxScan *scan=NULL;
    ProSelection selection[16]={0};
    wchar_t names[16][256];
    int count=0,interferences=0,pairs=0;
    ProError error=ax_scan(assembly,1,&scan);
    if(error)return error;
    if(scan->parts<2||scan->parts>16){
        part_text_add(out,L"全装配检查需要2至16个活动零件实例，可改用指定两组件检查。\n");
        error=PRO_TK_BAD_CONTEXT;
        goto done;
    }
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model||item->type!=PRO_MDL_PART)continue;
        AxComponent component={0};
        if(!ax_path_label(item->ids,item->depth,names[count],_countof(names[0]))){
            error=PRO_TK_BAD_INPUTS;goto done;
        }
        if((error=ax_component_resolve(assembly,names[count],&component))||
           (error=ax_part_selection(&component,&selection[count])))goto done;
        count++;
    }
    for(int i=0;i<count;i++)for(int j=i+1;j<count;j++){
        double volume;
        if((error=ax_interference_volume(selection[i],selection[j],&volume)))goto done;
        pairs++;
        if(volume>0){
            interferences++;
            part_text_add(out,L"%ls ↔ %ls，干涉体积 %.12g mm³。\n",
                names[i],names[j],volume*scale*scale*scale);
        }
    }
    part_text_add(out,L"检查 %d 个活动零件的 %d 对组合，正体积干涉 %d 对。\n",count,pairs,interferences);
done:
    for(int i=0;i<_countof(selection);i++)if(selection[i])ProSelectionFree(&selection[i]);
    free(scan);
    return error;
}
