/* The PTC rectangular flat-wall section has two signed horizontal
   extensions. Validate its three line profile before editing those dimensions. */
static ProError cm_wall_offsets(ProSection section,double *start,double *end){
    ProSecdimType first,last;
    ProError error;
    for(int id=2;id<=6;id+=2){
        Pro2dEntdef *entity;
        if((error=ProSectionEntityGet(section,id,&entity)))return error;
        if(!entity||entity->type!=PRO_2D_LINE)return PRO_TK_BAD_CONTEXT;
        Pro2dLinedef *line=(Pro2dLinedef *)entity;
        if(id==4?fabs(line->end1[1]-line->end2[1])>1e-7:fabs(line->end1[0]-line->end2[0])>1e-7)
            return PRO_TK_BAD_CONTEXT;
    }
    if((error=ProSecdimTypeGet(section,18,&first))||(error=ProSecdimTypeGet(section,14,&last)))return error;
    if(first!=PRO_TK_DIM_LINE_POINT||last!=PRO_TK_DIM_LINE_POINT)return PRO_TK_BAD_CONTEXT;
    if((error=ProSecdimValueGet(section,18,start)))return error;
    return ProSecdimValueGet(section,14,end);
}

static ProError cm_wall_profile(ProElement tree,double start,double end){
    ProElement type;
    ProSection section;
    ProBoolean intent;
    int mode;
    double old_start,old_end;
    ProError error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_WALL_TYPE},1,&type);
    if(!error)error=ProElementIntegerGet(type,NULL,&mode);
    if(error)return error;
    if(mode!=PRO_SMT_WALL_TYPE_FLAT)return PRO_TK_BAD_CONTEXT;
    if((error=fd_section_from_tree(tree,&section))||(error=cm_wall_offsets(section,&old_start,&old_end))||
       (error=ProSectionIntentManagerModeGet(section,&intent)))return error;
    if(intent&&(error=ProSectionIntentManagerModeSet(section,PRO_B_FALSE)))return error;
    if((error=ProSecdimValueSet(section,18,start))||(error=ProSecdimValueSet(section,14,end)))return error;
    ProWSecerror errors=NULL;
    error=ProSecerrorAlloc(&errors);
    if(!error){error=ProSectionRegenerate(section,&errors);ProSecerrorFree(&errors);}
    return error;
}

static ProError cm_apply_tree(ProFeature *feature,ProElement tree){
    ProFeatureCreateOptions *options=NULL;
    ProErrorlist errors={0};
    ProError error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        options[0]=PRO_FEAT_CR_NO_OPTS;
        error=ProFeatureWithoptionsRedefine(NULL,feature,tree,options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);
        ProArrayFree((ProArray *)&options);
    }
    if(!error)error=part_regenerate(feature->owner);
    if(!error)error=part_health(feature->owner);
    return error;
}

static ProError cm_wall_edit(ProFeature *feature,double start,double end){
    ProElement tree=NULL;
    ProError error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=cm_wall_profile(tree,start,end);
    if(!error)error=cm_apply_tree(feature,tree);
    if(tree)ProFeatureElemtreeFree(feature,tree);
    return error;
}

static ProError cm_native_command(ProMdl model,const wchar_t *format,...){
    wchar_t protocol[1300],receipt[PART_TEXT_MAX];
    PartCommand command;
    va_list args;
    va_start(args,format);
    int size=vswprintf_s(protocol,_countof(protocol),format,args);
    va_end(args);
    if(size<0||part_command_parse(protocol,&command)!=1)return PRO_TK_BAD_INPUTS;
    return sh_execute(&command,model,receipt,_countof(receipt));
}

static ProError cm_wall_tip(ProMdl model,ProFeature *feature,const CabinetEdge *source,CabinetEdge *tip){
    CabinetFace face;
    CabinetSpan span;
    ProError error=cm_wall_face(feature,&face);
    if(error)return error;
    if((error=cm_face_span(model,face.id,source->axis,&span)))return error;
    CabinetEdgeSearch search={0};
    search.model=model;
    search.face_id=face.id;
    search.parallel=source->axis;
    if((error=cm_edges_on_face(&search)))return error;
    if(!search.matches||fabs(ad_dot(search.edge.axis,source->axis))<1-1e-8||
       !cm_near(search.edge.length,span.max-span.min,1))return PRO_TK_BAD_CONTEXT;
    *tip=search.edge;
    return PRO_TK_NO_ERROR;
}

typedef struct {
    const CabinetEdge *edge;
    double min,max,scale;
    int count,mismatch;
} CabinetBendSpan;
static ProError cm_bend_span_surface(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    CabinetBendSpan *span=data;
    ProSurface surface;
    ProSmtSurfType side;
    ProSrftype shape;
    double angle;
    if(ProGeomitemToSurface(item,&surface)||ProSmtSurfaceTypeGet((ProPart)item->owner,surface,&side)||
       side!=PRO_SMT_SURF_FACE||ProSurfaceTypeGet(surface,&shape)||shape!=PRO_SRF_CYL||
       ProSmtBendsrfInfoGet((ProPart)item->owner,surface,NULL,NULL,&angle,NULL,NULL))return PRO_TK_NO_ERROR;
    CabinetSpan bounds;
    ProError error=cm_face_span(item->owner,item->id,span->edge->axis,&bounds);
    if(error)return error;
    span->count++;
    if(!cm_near(bounds.min,span->min,span->scale)||!cm_near(bounds.max,span->max,span->scale))span->mismatch=1;
    return PRO_TK_NO_ERROR;
}

static ProError cm_wall_span_check(ProFeature *wall,const CabinetEdge *edge,double from,double to,double scale){
    CabinetFace face;
    CabinetSpan bounds;
    ProError error=cm_wall_face(wall,&face);
    if(!error)error=cm_face_span(wall->owner,face.id,edge->axis,&bounds);
    if(error)return error;
    double origin=ad_dot(edge->a,edge->axis);
    if(!cm_near(bounds.min,origin+from,scale)||!cm_near(bounds.max,origin+to,scale))
        return PRO_TK_GENERAL_ERROR;
    CabinetBendSpan span={edge,origin+from,origin+to,scale,0,0};
    error=ProFeatureGeomitemVisit(wall,PRO_SURFACE,cm_bend_span_surface,NULL,&span);
    return error?error:span.count&&!span.mismatch?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError cm_attach(FdTxn *tx,const wchar_t *name,const CabinetEdge *edge,double from,double to,
    double height,double angle,double radius,const wchar_t *side,ProFeature *wall){
    if(from<0||to>edge->length+1e-6*tx->scale||to-from<fmax(0.1,radius)*tx->scale)return PRO_TK_BAD_INPUTS;
    ProMdlName model_name;
    ProError error=ProMdlMdlnameGet(tx->model,model_name);
    if(!error)error=cm_native_command(tx->model,L"CREO_OP SMT_FLANGE %ls %ls EDGE:%d %.12g %.12g %.12g %ls",
        model_name,name,edge->id,height,angle,radius,side);
    if(!error)error=cm_feature_find(tx->model,name,wall);
    if(error)return error;
    double end=fmin(0,to-edge->length);
    if(from>1e-6*tx->scale||end< -1e-6*tx->scale){
        if((error=cm_wall_edit(wall,-from,end)))return error;
    }
    return cm_wall_span_check(wall,edge,from,to,tx->scale);
}

static ProError cm_interval_edge(ProMdl model,const CabinetEdge *original,double from,double to,
    double tolerance,CabinetEdge *current,double *local_from,double *local_to){
    CabinetEdgeSearch search={0};
    search.model=model;
    search.face_id=original->face;
    search.line=original;
    search.from=from;
    search.to=to;
    search.tolerance=tolerance;
    ProError error=cm_edges_on_face(&search);
    if(error)return error;
    if(search.matches!=1)return PRO_TK_BAD_CONTEXT;
    *current=search.edge;
    ProVector p,q;
    cm_point(original->a,original->axis,from,p);
    cm_point(original->a,original->axis,to,q);
    ad_sub(p,current->a,p);
    ad_sub(q,current->a,q);
    double a=ad_dot(p,current->axis),b=ad_dot(q,current->axis);
    *local_from=fmax(0,fmin(a,b));
    *local_to=fmin(current->length,fmax(a,b));
    return PRO_TK_NO_ERROR;
}
