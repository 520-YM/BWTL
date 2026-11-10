typedef struct {ProVector point,normal;double nearest,scale;} ShThickness;
static ProError sh_parallel_wall(ProSurface surface,ProError status,ProAppData data){
    ShThickness *s=data;ProGeomitemdata *raw=NULL;ProSrftype type;ProError e;if(status)return status;
    if(ProSurfaceTypeGet(surface,&type)||type!=PRO_SRF_PLANE)return 0;
    if((e=ProSurfaceDataGet(surface,&raw)))return e;
    ProPlanedata *plane=&raw->data.p_surface_data->srf_shape.plane;ProVector delta;ad_sub(plane->origin,s->point,delta);
    double distance=fabs(ad_dot(delta,s->normal));
    if(fabs(fabs(ad_dot(plane->e3,s->normal))-1)<1e-6&&distance>.001*s->scale&&distance<s->nearest)s->nearest=distance;
    ProGeomitemdataFree(&raw);return 0;
}
static ProError sh_convert(FdTxn *tx,const PartCommand *cmd){
    FdRef face={0};ProSolidBody *bodies=NULL;ProSelection selection=NULL;ProReference reference=NULL;ProError e;int n=0;
#define SC(call) do{if((e=(call)))goto done;}while(0)
    SC(md_solid_face(tx,cmd->arg[3],&face));
    {ShThickness s={0};s.nearest=1e100;s.scale=tx->scale;SC(fd_plane_data(&face.item,s.point,s.normal));SC(ProSolidSurfaceVisit((ProSolid)tx->model,sh_parallel_wall,NULL,&s));
     if(fabs(s.nearest-as_arg(cmd,4)*tx->scale)>fmax(.001*tx->scale,as_arg(cmd,4)*tx->scale*1e-6)){e=PRO_TK_BAD_INPUTS;goto done;}}
    SC(fd_ref_element(tx,tx->root,PRO_E_SMT_DRV_SURF,&face));
    SC(ProSolidBodiesCollect((ProSolid)tx->model,&bodies));SC(ProArraySizeGet((ProArray)bodies,&n));if(n!=1){e=PRO_TK_BAD_CONTEXT;goto done;}
    SC(ProSelectionAlloc(NULL,(ProModelitem *)&bodies[0],&selection));SC(ProSelectionToReference(selection,&reference));tx->refs[tx->ref_count++]=reference;
    {ProElement el;SC(model_element(tx->root,PRO_E_SMT_CONV_BODY,0,0,&el));SC(ProElementReferenceSet(el,reference));}
    SC(model_element(tx->root,PRO_E_DTM_SRF_TYPE,1,PRO_DTM_SRF_AS_WALL,NULL));
    SC(model_element(tx->root,PRO_E_STD_SMT_THICKNESS,2,as_arg(cmd,4)*tx->scale,NULL));
    SC(model_element(tx->root,PRO_E_STD_MATRLSIDE,1,0,NULL));SC(model_element(tx->root,PRO_E_STD_SMT_SWAP_DRV_SIDE,1,0,NULL));
    SC(model_element(tx->root,PRO_E_SMT_ADJACENT_RND_OPTS,1,PRO_SMT_ADJ_RND_RECREATE,NULL));
    SC(model_element(tx->root,PRO_E_SMT_KEEP_NOT_CLASS_SURFS,1,PRO_SMT_IGNORE_NOT_CLASS_SURFS,NULL));SC(fd_create(tx,0));
done:fd_ref_free(&face);if(selection)ProSelectionFree(&selection);if(bodies)ProArrayFree((ProArray *)&bodies);return e;
#undef SC
}
