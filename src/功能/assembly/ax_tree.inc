#define AX_MAX_OCCURRENCES 2048
typedef struct {
    int ids[AX_PATH_DEPTH],depth;
    ProMdl model;
    ProMdlType type;
    ProFeatStatus state;
    ProFamilyMdlName name;
} AxOccurrence;

typedef struct {
    int ids[AX_PATH_DEPTH],depth,count,parts,suppressed,unavailable,limited,strict;
    AxOccurrence occurrences[AX_MAX_OCCURRENCES];
} AxScan;

static int ax_suppressed(ProFeatStatus state){
    return state==PRO_FEAT_SUPPRESSED||state==PRO_FEAT_FAMTAB_SUPPRESSED||
        state==PRO_FEAT_PROG_SUPPRESSED||state==PRO_FEAT_SIMP_REP_SUPPRESSED;
}

static ProError ax_visit_component(ProFeature *feature,ProError status,ProAppData context){
    if(status)return status;
    AxScan *scan=context;
    ProFeattype type;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(error||type!=PRO_FEAT_COMPONENT)return error;
    if(scan->count==AX_MAX_OCCURRENCES||scan->depth==AX_PATH_DEPTH){
        scan->limited=1;
        return PRO_TK_USER_ABORT;
    }
    scan->ids[scan->depth++]=feature->id;
    AxOccurrence *occurrence=&scan->occurrences[scan->count++];
    occurrence->depth=scan->depth;
    memcpy(occurrence->ids,scan->ids,scan->depth*sizeof(int));
    error=ProFeatureStatusGet(feature,&occurrence->state);
    if(error)goto done;

    error=ProAsmcompMdldataGet(feature,&occurrence->type,NULL,occurrence->name);
    if(error){
        occurrence->name[0]=0;
        occurrence->type=PRO_MDL_UNUSED;
        error=PRO_TK_NO_ERROR;
    }
    if(ax_suppressed(occurrence->state)){
        scan->suppressed++;
        goto done;
    }
    if(occurrence->state!=PRO_FEAT_ACTIVE||ProAsmcompMdlGet(feature,&occurrence->model)||!occurrence->model){
        scan->unavailable++;
        goto done;
    }
    ProMdlName actual_name;
    if(!ProMdlMdlnameGet(occurrence->model,actual_name))
        wcscpy_s(occurrence->name,_countof(occurrence->name),actual_name);
    if((error=ProMdlTypeGet(occurrence->model,&occurrence->type)))goto done;
    if(occurrence->type==PRO_MDL_PART){
        scan->parts++;
        if(scan->strict&&(error=part_health(occurrence->model)))goto done;
    }else if(occurrence->type==PRO_MDL_ASSEMBLY){
        if(scan->strict&&(error=ax_master(occurrence->model)))goto done;
        if(scan->strict&&(error=part_health(occurrence->model)))goto done;
        error=ProSolidFeatVisit((ProSolid)occurrence->model,ax_visit_component,NULL,scan);
        if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    }else scan->unavailable++;
done:
    scan->depth--;
    return error;
}

static ProError ax_scan(ProMdl assembly,int strict,AxScan **result){
    AxScan *scan=calloc(1,sizeof(*scan));
    *result=NULL;
    if(!scan)return PRO_TK_OUT_OF_MEMORY;
    scan->strict=strict;
    ProError error=ProSolidFeatVisit((ProSolid)assembly,ax_visit_component,NULL,scan);
    if(error==PRO_TK_E_NOT_FOUND||(error==PRO_TK_USER_ABORT&&scan->limited))error=PRO_TK_NO_ERROR;
    if(!error&&strict&&(scan->limited||scan->unavailable))error=PRO_TK_INCOMPLETE;
    if(error){free(scan);return error;}
    *result=scan;
    return PRO_TK_NO_ERROR;
}

static const wchar_t *ax_state_label(ProFeatStatus state){
    if(state==PRO_FEAT_ACTIVE)return L"活动";
    if(state==PRO_FEAT_SUPPRESSED)return L"普通抑制";
    if(state==PRO_FEAT_FAMTAB_SUPPRESSED)return L"族表抑制";
    if(state==PRO_FEAT_SIMP_REP_SUPPRESSED)return L"简化表示排除";
    if(state==PRO_FEAT_PROG_SUPPRESSED)return L"程序抑制";
    if(state==PRO_FEAT_UNREGENERATED)return L"待重生成";
    return L"非活动或不可用";
}

static ProError ax_tree(ProMdl assembly,const wchar_t *name,PartText *out){
    AxScan *scan=NULL;
    double scale;
    ProError error=qa_mm_per_unit(assembly,&scale);
    if(!error)error=ax_scan(assembly,0,&scan);
    if(error)return error;
    part_text_add(out,L"装配 %ls：组件实例 %d，活动零件 %d，抑制或排除 %d，不可读取 %d。\n",
        name,scan->count,scan->parts,scan->suppressed,scan->unavailable);
    for(int i=0;i<scan->count&&i<160;i++){
        AxOccurrence *item=&scan->occurrences[i];
        wchar_t label[256];
        if(!ax_path_label(item->ids,item->depth,label,_countof(label)))continue;
        const wchar_t *kind=item->type==PRO_MDL_ASSEMBLY?L"子装配":item->type==PRO_MDL_PART?L"零件":L"组件";
        part_text_add(out,L"%ls %ls %ls，%ls",label,kind,item->name[0]?item->name:L"名称不可用",
            ax_state_label(item->state));
        if(item->model&&item->state==PRO_FEAT_ACTIVE){
            ProIdTable ids={0};
            ProAsmcomppath path;
            ProMatrix pose;
            memcpy(ids,item->ids,item->depth*sizeof(int));
            if(!ProAsmcomppathInit((ProSolid)assembly,ids,item->depth,&path)&&!ProAsmcomppathTrfGet(&path,PRO_B_TRUE,pose))
                part_text_add(out,L"，原点 (%.8g, %.8g, %.8g) mm",pose[3][0]*scale,pose[3][1]*scale,pose[3][2]*scale);
        }
        part_text_add(out,L"\n");
    }
    if(scan->limited||scan->count>160)part_text_add(out,L"组件路径显示达到上限。\n");
    free(scan);
    return PRO_TK_NO_ERROR;
}
