typedef struct {
    ProVector center,axis;
    double radius,height,scale;
    int found;
} CabinetCollar;

typedef struct {
    ProMdl model;
    ProSurface face;
    ProVector center,axis;
    double radius,scale,length;
    int ids[16],count,selected,contains_selected;
} CabinetRing;

static ProError cm_ring_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    CabinetRing *ring=data;
    ProEnttype type;
    ProVector point,d1,d2,d3,delta;
    double length;
    int id;
    if(ProEdgeTypeGet(edge,&type)||(type!=PRO_ENT_CIRCLE&&type!=PRO_ENT_ARC))return PRO_TK_NO_ERROR;
    for(int i=0;i<3;i++){
        ProError error=ProEdgeXyzdataEval(edge,i*0.5,point,d1,d2,d3);
        if(error)return error;
        ad_sub(point,ring->center,delta);
        if(!cm_near(cm_distance(point,ring->center),ring->radius,ring->scale)||
           !cm_near(ad_dot(delta,ring->axis),0,ring->scale))return PRO_TK_NO_ERROR;
    }
    ProError error=ProEdgeIdGet(edge,&id);
    if(error)return error;
    for(int i=0;i<ring->count;i++)if(id==ring->ids[i])return PRO_TK_NO_ERROR;
    if(ring->count==_countof(ring->ids))return PRO_TK_BAD_CONTEXT;
    if((error=ProEdgeLengthEval(edge,&length)))return error;
    ring->ids[ring->count++]=id;
    ring->length+=length;
    if(id==ring->selected)ring->contains_selected=1;
    return PRO_TK_NO_ERROR;
}

static ProError cm_ring_contour(ProContour contour,ProError status,ProAppData data){
    CabinetRing *ring=data;
    return status?status:ProContourEdgeVisit(ring->face,contour,cm_ring_edge,NULL,ring);
}

static ProError cm_ring_collection(CabinetRing *ring,ProElement element){
    ProCollection collection=NULL;
    ProError error=ProCrvcollectionAlloc(&collection);
    for(int i=0;!error&&i<ring->count;i++){
        ProModelitem edge={PRO_EDGE,ring->ids[i],ring->model};
        ProSelection selection=NULL;
        ProReference reference=NULL;
        ProCrvcollinstr instruction=NULL;
        error=ProSelectionAlloc(NULL,&edge,&selection);
        if(!error)error=ProSelectionToReference(selection,&reference);
        if(!error)error=ProCrvcollinstrAlloc(PRO_CURVCOLL_ADD_ONE_INSTR,&instruction);
        if(!error)error=ProCrvcollinstrReferenceAdd(instruction,reference);
        if(!error)error=ProCrvcollectionInstructionAdd(collection,instruction);
        if(instruction)ProCrvcollinstrFree(instruction);
        if(reference)ProReferenceFree(reference);
        if(selection)ProSelectionFree(&selection);
    }
    if(!error)error=ProElementCollectionSet(element,collection);
    if(collection)ProCollectionFree(&collection);
    return error;
}

static ProError cm_collar_surface(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    CabinetCollar *collar=data;
    ProSurface surface;
    ProSrftype type;
    ProSmtSurfType side;
    if(ProGeomitemToSurface(item,&surface)||ProSurfaceTypeGet(surface,&type)||type!=PRO_SRF_CYL||
       ProSmtSurfaceTypeGet((ProPart)item->owner,surface,&side)||
       (side!=PRO_SMT_SURF_FACE&&side!=PRO_SMT_SURF_OFFSET))return PRO_TK_NO_ERROR;
    ProGeomitemdata *raw=NULL;
    ProError error=ProSurfaceDataGet(surface,&raw);
    if(error)return error;
    ProCylinderdata *c=&raw->data.p_surface_data->srf_shape.cylinder;
    ProVector delta,offset;
    ad_sub(c->origin,collar->center,delta);
    ad_cross(delta,collar->axis,offset);
    if(fabs(ad_dot(c->e3,collar->axis))>1-1e-8&&sqrt(ad_dot(offset,offset))<0.0002*collar->scale&&
       c->radius>0.1*collar->scale){
        CabinetSpan span;
        error=cm_face_span(item->owner,item->id,collar->axis,&span);
        double origin=ad_dot(collar->center,collar->axis);
        if(!error&&fmax(fabs(span.min-origin),fabs(span.max-origin))>collar->height*0.8){
            collar->found++;
            if(c->radius<collar->radius)collar->radius=c->radius;
        }
    }
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError cm_collar(FdTxn *outer,const PartCommand *cmd,PartText *out){
    FdRef edge={0};
    FdTxn tx={0};
    ProEdge native;
    ProEnttype edge_type;
    ProSurface surfaces[2],face=NULL,bore=NULL;
    ProGeomitem geom;
    ProVector plane_point,delta;
    ProSection stock=NULL;
    ProElement element,section,depth,side,sketch_element=NULL;
    ProPath path;
    wchar_t module[MAX_PATH],*end;
    CabinetCollar collar={0};
    CabinetRing ring={0};
    ProGeomitemdata *bore_data=NULL;
    ProError error;
    int attached=0;
    double height=as_arg(cmd,4),radius=as_arg(cmd,5),diameter;
#define CM_C(call) do{if((error=(call))){trace("collar " #call,error);goto done;}}while(0)
    CM_C(fd_ref_resolve(outer->model,cmd->arg[3],PRO_EDGE,&edge));
    CM_C(ProGeomitemToEdge(&edge.item,&native));
    CM_C(ProEdgeTypeGet(native,&edge_type));
    if(edge_type!=PRO_ENT_CIRCLE&&edge_type!=PRO_ENT_ARC){error=PRO_TK_BAD_CONTEXT;goto done;}
    CM_C(ProEdgeNeighborsGet(native,NULL,NULL,&surfaces[0],&surfaces[1]));
    for(int i=0;i<2;i++){
        ProSmtSurfType type;
        if(!ProSmtSurfaceTypeGet((ProPart)outer->model,surfaces[i],&type)){
            if(type==PRO_SMT_SURF_FACE)face=surfaces[i];
            if(type==PRO_SMT_SURF_SIDE)bore=surfaces[i];
        }
    }
    if(!face||!bore){error=PRO_TK_BAD_CONTEXT;goto done;}
    CM_C(ProSurfaceToGeomitem((ProSolid)outer->model,face,&geom));
    CM_C(fd_plane_data(&geom,plane_point,collar.axis));
    ProSrftype bore_type;
    CM_C(ProSurfaceTypeGet(bore,&bore_type));
    if(bore_type!=PRO_SRF_CYL||!ad_unit(collar.axis)){error=PRO_TK_BAD_CONTEXT;goto done;}
    CM_C(ProSurfaceDataGet(bore,&bore_data));
    ProCylinderdata *cylinder=&bore_data->data.p_surface_data->srf_shape.cylinder;
    if(fabs(ad_dot(cylinder->e3,collar.axis))<1-1e-8){error=PRO_TK_BAD_CONTEXT;goto done;}
    ad_sub(plane_point,cylinder->origin,delta);
    cm_point(cylinder->origin,collar.axis,ad_dot(delta,collar.axis),collar.center);
    diameter=2*cylinder->radius;
    ring.model=outer->model;
    ring.face=face;
    ring.scale=outer->scale;
    ring.radius=cylinder->radius;
    ring.selected=edge.item.id;
    memcpy(ring.center,collar.center,sizeof(ProVector));
    memcpy(ring.axis,collar.axis,sizeof(ProVector));
    CM_C(ProSurfaceContourVisit(face,cm_ring_contour,NULL,&ring));
    if(!ring.contains_selected||!cm_near(ring.length,AS_PI*diameter,outer->scale)){
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    collar.height=height*outer->scale;
    collar.scale=outer->scale;
    collar.radius=1e20;
    if(diameter<=2*radius*outer->scale||height<=radius){error=PRO_TK_BAD_INPUTS;goto done;}
    ProRay ray;
    QualityRay hits;
    memcpy(ray.start_point,collar.center,sizeof(ProVector));
    memcpy(ray.dir_vector,collar.axis,sizeof(ProVector));
    CM_C(qa_ray(outer->model,&ray,0.0001*outer->scale,&hits));
    if(hits.count){error=PRO_TK_BAD_CONTEXT;goto done;}
    if(!GetModuleFileNameW(NULL,module,MAX_PATH)){error=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<3;i++){
        end=wcsrchr(module,L'\\');
        if(!end){error=PRO_TK_BAD_CONTEXT;goto done;}
        *end=0;
    }
    swprintf_s(path,PRO_PATH_SIZE,L"%ls\\text\\smt\\flange_shapes\\i_wall.sec",module);
    CM_C(ProMdlFiletypeLoad(path,PRO_MDLFILE_2DSECTION,PRO_B_FALSE,(ProMdl *)&stock));
    CM_C(sh_begin(&tx,outer->model,outer->name,PRO_FEAT_WALL));
    CM_C(model_element(tx.root,PRO_E_SMT_WALL_TYPE,1,PRO_SMT_WALL_TYPE_FLANGE,NULL));
    CM_C(model_element(tx.root,PRO_E_SMT_FLANGE_TYPE,1,PRO_FLANGE_WALL_TYPE_2D_SWEPT,NULL));
    CM_C(model_element(tx.root,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&element));
    CM_C(cm_ring_collection(&ring,element));
    CM_C(model_element(tx.root,PRO_E_SMT_FLANGE_TRAJ_CRV_NORM,1,PRO_FLANGE_TRAJ_NORM_NO_FLIP,NULL));
    CM_C(model_element(tx.root,PRO_E_STD_SECTION,0,0,&section));
    CM_C(model_element(section,PRO_E_SKETCHER,0,0,&sketch_element));
    CM_C(ProElementSpecialvalueSet(sketch_element,stock));
    attached=1;
    CM_C(model_element(tx.root,PRO_E_SMT_WALL_SHARPS_TO_BENDS,1,PRO_B_TRUE,NULL));
    CM_C(model_element(tx.root,PRO_E_SMT_FLANGE_SEC_FLIP,1,!wcscmp(cmd->arg[6],L"NEG"),NULL));
    CM_C(model_element(tx.root,PRO_E_SMT_FLANGE_DEPTH,0,0,&depth));
    for(int i=0;i<2;i++){
        CM_C(model_element(depth,i?PRO_E_SMT_FLANGE_SIDE_2_DEPTH:PRO_E_SMT_FLANGE_SIDE_1_DEPTH,0,0,&side));
        CM_C(model_element(side,PRO_E_SMT_FLANGE_DEPTH_TYPE,1,PRO_WALL_LEN_TYPE_TO_END,NULL));
    }
    CM_C(sh_fillet(&tx,tx.root,radius));
    CM_C(model_element(tx.root,PRO_E_SMT_WALL_HEIGHT,0,0,&element));
    CM_C(model_element(element,PRO_E_SMT_WALL_HEIGHT_TYPE,1,PRO_SMT_WALL_HEIGHT_NONE,NULL));
    CM_C(model_element(tx.root,PRO_E_SMT_WALL_THICKNESS_FLIP,1,PRO_B_FALSE,NULL));
    CM_C(sh_develop(&tx));
    CM_C(sh_create(&tx));
    CM_C(sh_profile_resize(&tx,height,0));
    CM_C(part_regenerate(outer->model));
    CM_C(part_health(outer->model));
    CM_C(ProFeatureGeomitemVisit(&tx.feature,PRO_SURFACE,cm_collar_surface,NULL,&collar));
    CM_C(qa_ray(outer->model,&ray,0.0001*outer->scale,&hits));
    if(!collar.found||hits.count){error=PRO_TK_GENERAL_ERROR;goto done;}
    part_text_add(out,L"已沿整圆孔边生成圆形翻孔，直段输入 %.8g mm，孔轴仍贯通，已核对同轴翻边内径 %.8g mm。",
        height,2*collar.radius/outer->scale);
done:
    if(attached)ProElementSpecialvalueSet(sketch_element,NULL);
    if(bore_data)ProGeomitemdataFree(&bore_data);
    fd_cleanup(&tx,0);
    return error;
#undef CM_C
}
