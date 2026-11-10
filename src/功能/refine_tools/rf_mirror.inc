typedef struct {FdTxn *tx;ProCollection collection;int source,count;} RfMirrorFaces;

static ProError rf_mirror_surface(ProSurface surface,ProError status,ProAppData data){
    RfMirrorFaces *faces=data;
    FdRef face={0};
    ProFeature owner;
    ProError e;
    if(status)return status;
    if((e=ProSurfaceToGeomitem((ProSolid)faces->tx->model,surface,&face.item))||
       (e=ProGeomitemFeatureGet(&face.item,&owner)))return e;
    if(owner.id!=faces->source)return PRO_TK_NO_ERROR;
    face.owner_feature=owner.id;
    if((e=ProSelectionAlloc(NULL,(ProModelitem *)&face.item,&face.selection)))return e;
    e=rf_surface_add(faces->tx,faces->collection,&face);
    fd_ref_free(&face);
    if(!e)faces->count++;
    return e;
}

static ProError rf_mirror(FdTxn *tx,const PartCommand *cmd,int *adds_material){
    FdRef plane={0};
    ProFeature source;
    ProFeattype type;
    ProVector point,normal;
    ProElement refs=NULL,surfaces=NULL,options=NULL;
    RfMirrorFaces faces={tx,NULL,0,0};
    ProError e;
#define MIRROR_STEP(call) do{e=(call);if(e)goto done;}while(0)
    tx->stage="resolve mirror source feature and planar reference";
    MIRROR_STEP(rf_active_feature(tx->model,cmd->arg[3],&source,&type));
    if(type!=PRO_FEAT_HOLE){
        const ProElemId form[]={PRO_E_FEATURE_FORM};
        if(type!=PRO_FEAT_CUT&&type!=PRO_FEAT_PROTRUSION){e=PRO_TK_UNSUPPORTED;goto done;}
        MIRROR_STEP(mc_tree_value(&source,form,1,PRO_EXTRUDE,1));
    }
    *adds_material=type==PRO_FEAT_PROTRUSION;
    faces.source=source.id;
    MIRROR_STEP(fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&plane));
    MIRROR_STEP(fd_plane_data(&plane.item,point,normal));
    tx->stage="collect source feature's final solid faces";
    MIRROR_STEP(ProSrfcollectionAlloc(&faces.collection));
    MIRROR_STEP(ProSolidSurfaceVisit((ProSolid)tx->model,rf_mirror_surface,NULL,&faces));
    if(!faces.count){e=PRO_TK_E_NOT_FOUND;goto done;}
    tx->stage="build attached native feature-surface mirror";
    MIRROR_STEP(model_element(tx->root,PRO_E_MGF_REFS,0,0,&refs));
    MIRROR_STEP(model_element(refs,PRO_E_STD_SURF_COLLECTION_APPL,0,0,&surfaces));
    MIRROR_STEP(ProElementCollectionSet(surfaces,faces.collection));
    MIRROR_STEP(fd_ref_element(tx,refs,PRO_E_MGF_MIRROR_PLANE,&plane));
    MIRROR_STEP(model_element(tx->root,PRO_E_FLEX_OPTS_CMPND,0,0,&options));
    MIRROR_STEP(model_element(options,PRO_E_FLEX_TRF_SEL_ATT_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));
    MIRROR_STEP(model_element(options,PRO_E_FLEX_ATTACH_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));
    MIRROR_STEP(model_element(options,PRO_E_FLEX_KEEP_ORIG_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));
    MIRROR_STEP(model_element(options,PRO_E_FLEX_CR_RND_GEOM,1,PRO_FLEXMODEL_OPT_NO,NULL));
    MIRROR_STEP(model_element(options,PRO_E_FLEX_PROPAGATE_TANGENCY,1,PRO_FLEXMODEL_OPT_NO,NULL));
    MIRROR_STEP(fd_create(tx,0));
done:
    if(faces.collection)ProCollectionFree(&faces.collection);
    fd_ref_free(&plane);
    return e;
#undef MIRROR_STEP
}
