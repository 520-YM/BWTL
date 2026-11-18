
typedef struct {
    FdTxn txn;
    double width, depth, thickness;
} CabinetPart;

static ProError cbp_convert(CabinetPart *part){
    ProMdlName name;
    ProError status=ProMdlMdlnameGet(part->txn.model,name);
    int face;
    if(!status)status=quick_top(&part->txn,part->thickness,&face);
    if(!status)status=cm_native_command(part->txn.model,L"CREO_OP SMT_CONVERT %ls STOCK_SHEET SURFACE:%d %.12g",
        name,face,part->thickness);
    return status;
}

static ProError cbp_seed(CabinetPart *part,const wchar_t *name,double w,double d,double t,int sheet){
    if(!ax_study_key(name)||w<=0||d<=0||t<=0||!isfinite(w)||!isfinite(d)||!isfinite(t))
        return PRO_TK_BAD_INPUTS;
    if(!part_target_available(NULL,name,L"prt"))return PRO_TK_E_FOUND;
    if(sheet&&(cm_recovery_uncertain||sh_recovery_uncertain))return PRO_TK_BAD_CONTEXT;

    ProMdlName id;
    wcscpy_s(id,_countof(id),name);
    part->width=w;
    part->depth=d;
    part->thickness=t;
    const ModelPlan blank={.a=w,.b=d,.height=t,.ready=1};
    const char *stage="";
    ProError status=quick_seed(id,&part->txn.model);
    if(!status)status=model_build_internal(&blank,id,&stage,part->txn.model,NULL,NULL);
    double mm;
    if(!status)status=qa_mm_per_unit(part->txn.model,&mm);
    if(status)return status;

    part->txn.scale=1/mm;
    status=quick_box_check(&part->txn,w,d,t,0);
    return status||!sheet?status:cbp_convert(part);
}

static ProError cbp_end(CabinetPart *part,ProError status,const wchar_t *stage,
    const wchar_t *label,ProMdl *result,PartText *out){
    *result=NULL;
    if(status){
        part_text_add(out,L"%ls在%ls未通过（Creo %d）。",label,stage,status);
        if(part->txn.model){
            ProError erased=ProMdlErase(part->txn.model);
            if(erased)part_text_add(out,L"Creo 未能释放该次新建试件（%d）。",erased);
        }
        part_text_add(out,L"\n");
    }else *result=part->txn.model;
    return status;
}

static ProError cbp_finish(const CabinetPart *part,int expected_bends,double radius){
    ProError status=part_regenerate(part->txn.model);
    if(!status)status=part_health(part->txn.model);
    FabBends measured={.model=part->txn.model};
    if(!status)status=fab_thickness(part->txn.model,&measured.mm,&measured.thickness);
    if(!status&&fabs(measured.thickness-part->thickness)>0.001)status=PRO_TK_INCOMPLETE;
    if(!status)status=ProSolidSurfaceVisit((ProSolid)part->txn.model,fab_bend_visit,NULL,&measured);
    if(!status&&measured.count!=expected_bends)status=PRO_TK_INCOMPLETE;
    for(int i=0;!status&&i<measured.count;i++){
        if(fabs(measured.rows[i].radius-radius)>0.001||fabs(fabs(measured.rows[i].angle)-90)>0.001)
            status=PRO_TK_INCOMPLETE;
    }
    return status;
}

/* The unbent stock is generated in the first quadrant. The edge search uses
   the real drive-face contour so a wrongly oriented or split edge fails. */
static ProError cbp_stock_edge(CabinetPart *part,int direction,int end,CabinetEdge *edge){
    CabinetEdge expected={0};
    int cross=1-direction,face=0;
    double size[]={part->width,part->depth};
    expected.a[cross]=expected.b[cross]=(end?size[cross]:0)*part->txn.scale;
    expected.a[2]=expected.b[2]=part->thickness*part->txn.scale;
    expected.b[direction]=size[direction]*part->txn.scale;
    expected.axis[direction]=1;
    CabinetEdgeSearch search={.model=part->txn.model,.line=&expected,.to=expected.b[direction],
        .tolerance=0.001*part->txn.scale};
    ProError status=quick_top(&part->txn,part->thickness,&face);
    if(status)return status;
    search.face_id=face;
    status=cm_edges_on_face(&search);
    if(!status&&search.matches!=1)status=PRO_TK_E_NOT_FOUND;
    if(!status)*edge=search.edge;
    return status;
}

static ProError cbp_fold_up(CabinetPart *part,const CabinetEdge *edge,const wchar_t *name,
    double first,double last,double height,double radius,ProFeature *wall,CabinetEdge *tip){
    ProError status=cm_attach(&part->txn,name,edge,first*part->txn.scale,last*part->txn.scale,
        height,90,radius,L"NEG",wall);
    if(!status)status=cm_wall_tip(part->txn.model,wall,edge,tip);
    if(!status&&fmin(tip->a[2],tip->b[2])-part->thickness*part->txn.scale<=0.001*part->txn.scale)
        status=PRO_TK_INCOMPLETE;
    return status;
}

static ProError cbp_openings(CabinetPart *part,PartCommand *setup,PanelFrame *frame,
    const wchar_t *sketch,const wchar_t *feature,PanelProfile *profiles,int count,int sheet){
    FdTxn *tx=&part->txn;
    double area=0;
    ProError status=PRO_TK_NO_ERROR;
    for(int i=0;!status&&i<count;i++){
        area+=pn_area(&profiles[i]);
        status=pn_profile_probes(tx,frame,&profiles[i],part->thickness,0);
    }
    ProMassProperty stock;
    if(!status)status=qa_geometry_mass(tx->model,&stock);
    if(status)return status;
    tx->volume=stock.volume;
    ProMatrix local;
    status=pn_sketch(tx->model,setup,frame,sketch,profiles,count,sheet,local);
    if(!status)status=pn_cut(tx,frame,feature,sketch,local,part->thickness,sheet);
    if(!status)status=md_expected_volume(tx,-area*part->thickness);
    for(int i=0;!status&&i<count;i++)status=pn_profile_probes(tx,frame,&profiles[i],part->thickness,1);
    return status;
}

static ProError cbp_top_openings(CabinetPart *part,const wchar_t *sketch,const wchar_t *feature,
    PanelProfile *profiles,int count,int sheet){
    PartCommand setup={0};
    PanelFrame frame={0};
    ProError status=quick_frame(&part->txn,part->thickness,&setup,&frame);
    return status?status:cbp_openings(part,&setup,&frame,sketch,feature,profiles,count,sheet);
}

static PanelProfile cbp_circle(double x,double y,double diameter){
    return (PanelProfile){PN_CIRCLE,x,y,diameter,diameter,0,0};
}
