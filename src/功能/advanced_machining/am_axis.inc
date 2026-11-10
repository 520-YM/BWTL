typedef struct {ProMdl model;AmHole *hole;ProVector direction,normal;double clearance,scale;int count,boundary;ProSurface surface;} AmEntry;
static ProError am_entry_edge(ProEdge edge,ProError status,ProAppData data){
    AmEntry *entry=data;ProVector p;double parameter;
    if(status)return status;
    if(!ProEdgeParamEval(edge,entry->hole->center,&parameter)&&parameter>=-1e-7&&parameter<=1+1e-7&&
       !ProEdgeXyzdataEval(edge,fmax(0,fmin(1,parameter)),p,NULL,NULL,NULL)&&
       ad_near(p,entry->hole->center,0.001*entry->scale))entry->boundary=1;
    return 0;
}
static ProError am_entry_contour(ProContour contour,ProError status,ProAppData data){
    AmEntry *entry=data;if(status)return status;
    return ProContourEdgeVisit(entry->surface,contour,am_entry_edge,NULL,entry);
}
static ProError am_entry_face(ProSurface surface,ProError status,ProAppData data){
    AmEntry *entry=data;ProUvParam uv;ProUvStatus inside;ProVector p,n;ProSrftype type;double dot;ProError e;
    if(status)return status;
    if(ProSurfaceParamEval((ProSolid)entry->model,surface,entry->hole->center,uv)||
       ProSurfaceXyzdataEval(surface,uv,p,NULL,NULL,n)||!ad_near(p,entry->hole->center,0.001*entry->scale))return 0;
    if(ProSurfaceUvpntVerify((ProSolid)entry->model,surface,uv,&inside)||inside!=PRO_UV_INSIDE){
        entry->surface=surface;entry->boundary=0;
        if(ProSurfaceContourVisit(surface,am_entry_contour,NULL,entry)||!entry->boundary)return 0;
    }
    dot=ad_dot(n,entry->direction);if(dot>=-0.05)return 0;
    /* Creo can split a smooth cylinder at its parameter seam. Treat matching
       normals there as one entrance, while still rejecting a sharp corner. */
    if(entry->count){if(entry->count<0||ad_dot(n,entry->normal)<1-1e-6)entry->count=-1;return 0;}
    if((e=ProSurfaceTypeGet(surface,&type)))return e;
    entry->clearance=entry->hole->radius*sqrt(fmax(0,1-dot*dot))/fabs(dot)+0.001*entry->scale;
    if(type!=PRO_SRF_PLANE)entry->clearance+=entry->hole->radius;
    memcpy(entry->normal,n,sizeof(ProVector));entry->count=1;return 0;
}
static ProError am_axis(FdTxn *tx,const PartCommand *cmd,AmHole *geometry,int spotface){
    FdRef point={0},axis={0};ProVector anchor,delta,cross;ProElement placement,com,from,element;ProError e;
    AmEntry entry={0};int positive=!wcscmp(cmd->arg[spotface?7:8],L"POS");
    double depth=as_arg(cmd,spotface?6:7)*tx->scale;
    if((e=fd_ref_resolve(tx->model,cmd->arg[3],PRO_POINT,&point))||
       (e=ad_point(&point,geometry->center))||(e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_AXIS,&axis))||
       (e=ad_line(&axis,anchor,geometry->axis)))goto done;
    ad_sub(geometry->center,anchor,delta);ad_cross(delta,geometry->axis,cross);
    if(sqrt(ad_dot(cross,cross))>0.001*tx->scale){e=PRO_TK_BAD_INPUTS;goto done;}
    geometry->known=1;geometry->radius=as_arg(cmd,5)*tx->scale/2;
    entry.model=tx->model;entry.hole=geometry;entry.scale=tx->scale;
    for(int i=0;i<3;i++)entry.direction[i]=geometry->axis[i]*(positive?1:-1);
    if((e=ProSolidSurfaceVisit((ProSolid)tx->model,am_entry_face,NULL,&entry)))goto done;
    trace("compatible machining entrance",entry.count);
    if(entry.count!=1){e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=am_flat_hole(tx,2*geometry->radius,depth))||
       (e=model_element(tx->root,PRO_E_HLE_PLACEMENT,0,0,&placement))||
       (e=fd_ref_element(tx,placement,PRO_E_HLE_PRIM_REF,&point))||
       (e=model_element(placement,PRO_E_HLE_PL_TYPE,1,PRO_HLE_PL_TYPE_ON_PNT,NULL))||
       (e=fd_ref_element(tx,placement,PRO_E_HLE_FT_DIR_REF,&axis))||
       (e=model_element(placement,PRO_E_HLE_FT_DIR_OPT,1,PRO_HLE_FT_PARALLEL_DIR,NULL))||
       (e=mc_tree_element(tx->root,(ProElemId[]){PRO_E_HLE_COM},1,&com))||
       (e=model_element(com,PRO_E_HLE_CRDIR_FLIP,1,positive?PRO_HLE_CR_IN_SIDE_ONE:PRO_HLE_CR_IN_SIDE_TWO,NULL))||
       (e=mc_tree_element(com,(ProElemId[]){PRO_E_HOLE_STD_DEPTH,PRO_E_HOLE_DEPTH_FROM},2,&from))||
       (e=mc_tree_element(from,(ProElemId[]){PRO_E_HOLE_DEPTH_FROM_TYPE},1,&element))||
       (e=ProElementIntegerSet(element,PRO_HLE_STRGHT_BLIND_DEPTH))||
       (e=model_element(from,PRO_E_EXT_DEPTH_FROM_VALUE,2,entry.clearance,NULL)))goto done;
    e=fd_create(tx,0);
done:fd_ref_free(&point);fd_ref_free(&axis);return e;
}
