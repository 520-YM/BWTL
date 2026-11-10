static ProError ax_read_constraints(ProAsmcomp *component,ProAsmcomppath *owner,
    ProAsmcompconstraint **rows,int *count){
    *rows=NULL;*count=0;
    ProError error=ProAsmcompConstraintsWithComppathGet(component,owner,rows);
    if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    return error||!*rows?error:ProArraySizeGet((ProArray)*rows,count);
}

static int ax_offset_type(ProAsmcompConstrType type){
    return type==PRO_ASM_ALIGN_OFF||type==PRO_ASM_MATE_OFF;
}

static const wchar_t *ax_constraint_label(ProAsmcompConstrType type){
    switch(type){
    case PRO_ASM_FIX:return L"固定";
    case PRO_ASM_MATE:return L"贴合";
    case PRO_ASM_ORIENT:return L"平行";
    case PRO_ASM_ALIGN:return L"重合或同轴";
    case PRO_ASM_INSERT:return L"圆柱同轴";
    case PRO_ASM_ALIGN_OFF:case PRO_ASM_MATE_OFF:return L"偏距";
    case PRO_ASM_CSYS:return L"坐标系重合";
    default:return L"其他原生约束";
    }
}

static void ax_selection_label(ProSelection selection,wchar_t *label,size_t capacity){
    ProModelitem item;
    ProAsmcomppath path;
    wchar_t occurrence[AX_PATH_DEPTH*12+4]=L"ROOT/本组件";
    if(!selection||ProSelectionVerify(selection)||ProSelectionModelitemGet(selection,&item)){
        wcscpy_s(label,capacity,L"参照缺失或无法解析");
        return;
    }
    if(!ProSelectionAsmcomppathGet(selection,&path)&&path.table_num>0)
        ax_path_label(path.comp_id_table,path.table_num,occurrence,_countof(occurrence));
    swprintf_s(label,capacity,L"%ls %ls:%d",occurrence,
        item.type==PRO_SURFACE?L"SURFACE":item.type==PRO_AXIS?L"AXIS":item.type==PRO_CSYS?L"CSYS":L"ITEM",item.id);
}

static ProError ax_placement_status(ProMdl assembly,const wchar_t *token,double mm,PartText *out){
    ProIdTable ids={0};
    int n=ax_path_parse(token,ids);
    if(!n)return PRO_TK_BAD_INPUTS;
    ProMdl model=assembly;
    ProAsmcomp feature;
    ProFeatStatus state;
    ProError error;
    for(int i=0;i<n;i++){
        if((error=ax_component_feature(model,ids[i],&feature))||
           (error=ProFeatureStatusGet(&feature,&state)))return error;
        if(i<n-1&&((error=ProAsmcompMdlGet(&feature,&model))||(error=ax_master(model))))return error;
    }
    ProAsmcomppath owner;
    if((error=ProAsmcomppathInit((ProSolid)assembly,ids,n-1,&owner)))return error;
    ProBoolean packed=PRO_B_FALSE,under=PRO_B_FALSE;
    ProError packed_error=ProAsmcompIsPackaged(&feature,&packed);
    ProError under_error=ProAsmcompIsUnderconstrained(&feature,&under);
    part_text_add(out,L"%ls Creo 特征状态 %d，%ls。\n",token,state,
        packed_error||under_error?L"定位状态无法判定":packed?L"未完成定位（封装）":
        state!=PRO_FEAT_ACTIVE?L"非活动状态，请核对重生成":under?L"欠约束":L"Creo 未报告欠约束");
    ProAsmcompconstraint *rows=NULL;
    int count;
    error=ax_read_constraints(&feature,&owner,&rows,&count);
    if(!error)part_text_add(out,L"读取到 %d 条定位约束。\n",count);
    for(int i=0;!error&&i<count;i++){
        ProAsmcompConstrType type;
        if((error=ProAsmcompconstraintTypeGet(rows[i],&type)))break;
        part_text_add(out,L"%d %ls",i+1,ax_constraint_label(type));
        if(type!=PRO_ASM_FIX){
            ProSelection base=NULL,part=NULL;
            ProDatumside side;
            wchar_t a[260],b[260];
            ProAsmcompconstraintAsmreferenceGet(rows[i],&base,&side);
            ProAsmcompconstraintCompreferenceGet(rows[i],&part,&side);
            ax_selection_label(base,a,_countof(a));
            ax_selection_label(part,b,_countof(b));
            part_text_add(out,L" %ls ↔ %ls",a,b);
        }
        if(ax_offset_type(type)){
            double offset;
            if(!ProAsmcompconstraintOffsetGet(rows[i],&offset))part_text_add(out,L" 偏距 %.10g mm",offset*mm);
        }
        part_text_add(out,L"\n");
    }
    if(rows)ProAsmcompconstraintArrayFree(rows);
    if(error)part_text_add(out,L"Creo 未能完整读取约束，需检查组件状态和可能丢失的参照。\n");
    return error;
}
