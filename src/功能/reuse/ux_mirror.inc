typedef struct {FdTxn *tx;ProCollection collection;int ids[8],counts[8],n;} UxMirror;
static ProError ux_mirror_face(ProSurface surface,ProError status,ProAppData data){UxMirror *s=data;ProFeature owner;FdRef ref={0};ProError e;if(status)return status;if((e=ProSurfaceToGeomitem((ProSolid)s->tx->model,surface,&ref.item))||(e=ProGeomitemFeatureGet(&ref.item,&owner)))return e;for(int i=0;i<s->n;i++)if(owner.id==s->ids[i]){ref.owner_feature=owner.id;if((e=ProSelectionAlloc(NULL,(ProModelitem *)&ref.item,&ref.selection)))return e;e=rf_surface_add(s->tx,s->collection,&ref);fd_ref_free(&ref);if(!e)s->counts[i]++;return e;}return 0;}
static ProError ux_mirror(FdTxn *tx,const PartCommand *cmd){UxMirror s={0};FdRef plane={0};ProElement refs,el,opts;ProError e;ProVector p,n;s.tx=tx;s.n=cmd->count-4;
#define UM(call) do{if((e=(call))){trace("reuse mirror " #call,e);goto done;}}while(0)
 for(int i=0;i<s.n;i++){ProFeature f;ProFeattype type;UM(rf_active_feature(tx->model,cmd->arg[i+4],&f,&type));if(type!=PRO_FEAT_HOLE&&type!=PRO_FEAT_CUT&&type!=PRO_FEAT_PROTRUSION&&type!=PRO_FEAT_ROUND&&type!=PRO_FEAT_CHAMFER){e=PRO_TK_UNSUPPORTED;goto done;}s.ids[i]=f.id;}
 UM(fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&plane));UM(fd_plane_data(&plane.item,p,n));UM(ProSrfcollectionAlloc(&s.collection));UM(ProSolidSurfaceVisit((ProSolid)tx->model,ux_mirror_face,NULL,&s));
 for(int i=0;i<s.n;i++)if(!s.counts[i]){e=PRO_TK_E_NOT_FOUND;goto done;}
 UM(model_element(tx->root,PRO_E_MGF_REFS,0,0,&refs));UM(model_element(refs,PRO_E_STD_SURF_COLLECTION_APPL,0,0,&el));UM(ProElementCollectionSet(el,s.collection));UM(fd_ref_element(tx,refs,PRO_E_MGF_MIRROR_PLANE,&plane));
 UM(model_element(tx->root,PRO_E_FLEX_OPTS_CMPND,0,0,&opts));UM(model_element(opts,PRO_E_FLEX_TRF_SEL_ATT_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));UM(model_element(opts,PRO_E_FLEX_ATTACH_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));UM(model_element(opts,PRO_E_FLEX_KEEP_ORIG_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));UM(model_element(opts,PRO_E_FLEX_CR_RND_GEOM,1,PRO_FLEXMODEL_OPT_NO,NULL));UM(model_element(opts,PRO_E_FLEX_PROPAGATE_TANGENCY,1,PRO_FLEXMODEL_OPT_NO,NULL));UM(fd_create(tx,0));
done:if(s.collection)ProCollectionFree(&s.collection);fd_ref_free(&plane);return e;
#undef UM
}
