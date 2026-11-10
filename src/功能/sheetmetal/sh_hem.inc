typedef struct {ProVector normal;ProName name;} ShOrient;
static ProError sh_orient_plane(ProFeature *feature,ProError status,ProAppData data){
    ShOrient *s=data;ProFeattype type;ProGeomitem item;ProVector p,n;
    if(status)return status;
    if(ProFeatureTypeGet(feature,&type)||type!=PRO_FEAT_DATUM||fd_feature_geom(feature,PRO_SURFACE,&item)||fd_plane_data(&item,p,n)||fabs(ad_dot(n,s->normal))>.9)return 0;
    if(ProModelitemNameGet((ProModelitem *)feature,s->name))return 0;return PRO_TK_E_FOUND;
}
static ProError sh_edge_line(FdTxn *outer,const wchar_t *face_token,const ProVector a,const ProVector b,ProName name){
    FdTxn tx={0};FdSketch sketch={0};ShOrient orient={0};FdRef face={0};ProVector p,delta;ProMatrix frame;wchar_t plane[64];ProError e;int id;
    swprintf_s(name,PRO_NAME_SIZE,L"%.23ls_FOLD",outer->name);
#define EL(call) do{if((e=(call)))goto done;}while(0)
    EL(fd_ref_resolve(outer->model,face_token,PRO_SURFACE,&face));EL(fd_plane_data(&face.item,p,orient.normal));
    e=ProSolidFeatVisit((ProSolid)outer->model,sh_orient_plane,NULL,&orient);if(e!=PRO_TK_E_FOUND){if(!e)e=PRO_TK_E_NOT_FOUND;goto done;}
    swprintf_s(plane,_countof(plane),L"PLANE:%ls",orient.name);
    EL(sh_begin(&tx,outer->model,name,PRO_FEAT_CURVE));EL(fd_sketch_setup(&tx,face_token,plane));EL(fd_create(&tx,1));sketch.feature=tx.feature;
    EL(ProFeatureElemtreeExtract(&sketch.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree));EL(fd_section_from_tree(sketch.tree,&sketch.section));EL(ProSectionLocationGet(sketch.section,frame));
    AsPoint start,end;ad_sub(a,frame[3],delta);start=as_point(ad_dot(delta,frame[0]),ad_dot(delta,frame[1]));ad_sub(b,frame[3],delta);end=as_point(ad_dot(delta,frame[0]),ad_dot(delta,frame[1]));
    EL(as_add_line(sketch.section,start,end,&id));EL(fd_section_solve(sketch.section,outer->scale));tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    EL(ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,tx.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors));EL(fd_verify(&tx));
done:fd_ref_free(&face);fd_sketch_free(&sketch);fd_cleanup(&tx,0);return e;
#undef EL
}
static ProError sh_hem(FdTxn *tx,const PartCommand *cmd){
    FdRef edge={0};ProEdge native;ProSurface sides[2],base=NULL;ProSmtSurfType type;ProGeomitem item;ProVector a,b,d1,d2,dir,n,p,along,inward,mid,candidate;
    ProUvParam uv;ProUvStatus location;ProName sketch;wchar_t face_token[40];PartCommand bend=*cmd;ProError e;ProEnttype edge_type;int surface_id,side_count=0;
    double width=as_arg(cmd,5)*tx->scale;
#define HM(call) do{e=(call);trace(#call,e);if(e)goto done;}while(0)
    HM(fd_ref_resolve(tx->model,cmd->arg[3],PRO_EDGE,&edge));HM(ProGeomitemToEdge(&edge.item,&native));
    HM(ProEdgeTypeGet(native,&edge_type));if(edge_type!=PRO_ENT_LINE){e=PRO_TK_BAD_INPUTS;goto done;}
    HM(ProEdgeNeighborsGet(native,NULL,NULL,&sides[0],&sides[1]));
    for(int i=0;i<2;i++)if(!ProSmtSurfaceTypeGet((ProPart)tx->model,sides[i],&type)){if(type==PRO_SMT_SURF_FACE)base=sides[i];if(type==PRO_SMT_SURF_SIDE)side_count++;}
    if(!base||side_count!=1){e=PRO_TK_BAD_INPUTS;goto done;}HM(ProSurfaceToGeomitem((ProSolid)tx->model,base,&item));HM(fd_plane_data(&item,p,n));HM(ProSurfaceIdGet(base,&surface_id));
    if(!wcscmp(cmd->arg[4],L"ROUND")&&as_arg(cmd,5)/as_arg(cmd,6)>1.9*AS_PI){e=PRO_TK_BAD_INPUTS;goto done;}
    HM(ProEdgeXyzdataEval(native,0,a,d1,d2,dir));HM(ProEdgeXyzdataEval(native,1,b,d1,d2,dir));ad_sub(b,a,along);if(!ad_unit(along)){e=PRO_TK_BAD_INPUTS;goto done;}ad_cross(n,along,inward);
    for(int j=0;j<3;j++){mid[j]=(a[j]+b[j])/2;candidate[j]=mid[j]+inward[j]*.01*tx->scale;}
    HM(ProSurfaceParamEval((ProSolid)tx->model,base,candidate,uv));HM(ProSurfaceUvpntVerify((ProSolid)tx->model,base,uv,&location));
    if(location!=PRO_UV_INSIDE){for(int j=0;j<3;j++){double swap=a[j];a[j]=b[j];b[j]=swap;inward[j]=-inward[j];}}
    for(int j=0;j<3;j++){a[j]+=inward[j]*width;b[j]+=inward[j]*width;candidate[j]=mid[j]+inward[j]*(width+.01*tx->scale);}
    HM(ProSurfaceParamEval((ProSolid)tx->model,base,candidate,uv));HM(ProSurfaceUvpntVerify((ProSolid)tx->model,base,uv,&location));if(location!=PRO_UV_INSIDE){e=PRO_TK_BAD_INPUTS;goto done;}
    swprintf_s(face_token,_countof(face_token),L"SURFACE:%d",surface_id);HM(sh_edge_line(tx,face_token,a,b,sketch));
    {ProElement el;ProElemId ids[]={PRO_E_FEATURE_TYPE};HM(mc_tree_element(tx->root,ids,1,&el));HM(ProElementIntegerSet(el,PRO_FEAT_BEND));}
    swprintf_s(bend.arg[3],PART_TOKEN_MAX,L"CURVE:%ls",sketch);wcscpy_s(bend.arg[4],PART_TOKEN_MAX,L"180");wcscpy_s(bend.arg[5],PART_TOKEN_MAX,cmd->arg[6]);
    wcscpy_s(bend.arg[6],PART_TOKEN_MAX,cmd->arg[7]);wcscpy_s(bend.arg[7],PART_TOKEN_MAX,L"FIX1");HM(sh_bend_native(tx,&bend,!wcscmp(cmd->arg[4],L"ROUND")));
done:fd_ref_free(&edge);return e;
#undef HM
}
