static int ax_field_path_matches(const ProAsmcomppath *path,const AxOccurrence *item){
    return path->table_num==item->depth&&
        !memcmp(path->comp_id_table,item->ids,item->depth*sizeof(int));
}

static int ax_field_same_part(const AxOccurrence *a,const AxOccurrence *b){
    return a->type==PRO_MDL_PART&&b->type==PRO_MDL_PART&&
        ((a->model&&a->model==b->model)||(a->name[0]&&b->name[0]&&!_wcsicmp(a->name,b->name)));
}

static ProError ax_field_audit(ProMdl assembly,const wchar_t *name,PartText *out){
    ProFeature feature;
    ProFeatStatus state;
    ProFeatintersection *intersections=NULL;
    AxScan *scan=NULL;
    unsigned char affected[AX_MAX_OCCURRENCES]={0};
    int count=0,other=0;
    ProError error=ux_named_feature(assembly,name,&feature);
    if(!error)error=ProFeatureStatusGet(&feature,&state);
    if(error)return error;
    if(state!=PRO_FEAT_ACTIVE)return PRO_TK_BAD_CONTEXT;
    error=ProFeatureAsmintersectionsCollect(&feature,&intersections);
    if(error==PRO_TK_E_NOT_FOUND){
        part_text_add(out,L"Creo 未返回 %ls 的实例相交记录，无法确认加工范围。\n",name);
        goto done;
    }
    if(error||(error=ProArraySizeGet((ProArray)intersections,&count))||
       (error=ax_scan(assembly,0,&scan)))goto done;
    if(!count){error=PRO_TK_E_NOT_FOUND;goto done;}
    part_text_add(out,L"装配特征 %ls 的 Creo 相交记录共 %d 条。\n",name,count);
    for(int i=0;i<count;i++){
        ProAsmcomppath path;
        int level,feature_id;
        ProFeatIntersectionType type;
        if((error=ProFeatintersectionDataGet(intersections[i],&path,&level,&feature_id))||
           (error=ProFeatintersectionTypeGet(intersections[i],&type)))goto done;
        if(path.table_num<=0||path.table_num>AX_PATH_DEPTH){error=PRO_TK_INCOMPLETE;goto done;}
        int match=0;
        for(;match<scan->count;match++)if(ax_field_path_matches(&path,&scan->occurrences[match]))break;
        if(match==scan->count){error=PRO_TK_INCOMPLETE;goto done;}
        affected[match]=1;
        wchar_t label[256];
        ax_path_label(path.comp_id_table,path.table_num,label,_countof(label));
        const wchar_t *scope=level==0?L"仅顶层装配":
            level==path.table_num?L"可见于零件":L"子装配级";
        part_text_add(out,L"%ls %ls，%ls（可见层级 %d%ls）。\n",
            label,scan->occurrences[match].name,scope,level,
            type==PART_LEVEL_FULL_COPY?L"，零件级完整副本":L"");
    }
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(affected[i]||item->type!=PRO_MDL_PART)continue;
        for(int j=0;j<scan->count;j++){
            AxOccurrence *hit=&scan->occurrences[j];
            if(!affected[j])continue;
            if(!ax_field_same_part(item,hit))continue;
            wchar_t label[256];
            ax_path_label(item->ids,item->depth,label,_countof(label));
            part_text_add(out,L"同模型实例 %ls %ls，未列入 Creo 相交记录（%ls）。\n",
                label,item->name,ax_state_label(item->state));
            other++;
            break;
        }
    }
    part_text_add(out,L"在已扫描的装配树中，另有 %d 个同模型实例未列入相交记录。\n",other);
    if(scan->suppressed||scan->unavailable||scan->limited){
        part_text_add(out,L"装配树含抑制、不可读或超出扫描范围的实例，同模型实例清单可能不完整。\n");
        error=PRO_TK_INCOMPLETE;
    }
done:
    if(intersections)ProFeatintersectionProarrayFree(intersections);
    free(scan);
    return error;
}
