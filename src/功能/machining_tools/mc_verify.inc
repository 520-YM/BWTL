static ProError mc_tree_value(ProFeature *f,ProElemId *ids,int n,double expected,int integer){
    ProElement tree=NULL,el=NULL;ProElempath path=NULL;ProElempathItem parts[8];ProError e;int i,v;double d;
    if(n>8)return PRO_TK_BAD_INPUTS;
    if((e=ProFeatureElemtreeExtract(f,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||(e=ProElempathAlloc(&path)))goto done;
    for(i=0;i<n;i++){parts[i].type=PRO_ELEM_PATH_ITEM_TYPE_ID;parts[i].path_item.elem_id=ids[i];}
    if((e=ProElempathDataSet(path,parts,n))||(e=ProElemtreeElementGet(tree,path,&el)))goto done;
    if(integer){e=ProElementIntegerGet(el,NULL,&v);if(!e&&v!=(int)expected)e=PRO_TK_GENERAL_ERROR;}
    else {e=ProElementDoubleGet(el,NULL,&d);if(!e&&!part_close(d,expected))e=PRO_TK_GENERAL_ERROR;}
done:if(path)ProElempathFree(&path);if(tree)ProFeatureElemtreeFree(f,tree);return e;
}
static ProError mc_edge_size(ProFeature *feature,double expected){
    ProElement tree=NULL,el=NULL;ProElempath path=NULL;ProElempathItem p[6];ProError e;double value;
    int ids[]={PRO_E_RNDCH_SETS,0,PRO_E_RNDCH_RADII,0,PRO_E_RNDCH_LEG1,PRO_E_RNDCH_LEG_VALUE},i;
    if((e=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||(e=ProElempathAlloc(&path)))goto done;
    for(i=0;i<6;i++){p[i].type=(i==1||i==3)?PRO_ELEM_PATH_ITEM_TYPE_INDEX:PRO_ELEM_PATH_ITEM_TYPE_ID;p[i].path_item.elem_id=ids[i];}
    if((e=ProElempathDataSet(path,p,6))||(e=ProElemtreeElementGet(tree,path,&el))||(e=ProElementDoubleGet(el,NULL,&value)))goto done;
    if(!part_close(value,expected))e=PRO_TK_GENERAL_ERROR;
done:if(path)ProElempathFree(&path);if(tree)ProFeatureElemtreeFree(feature,tree);return e;
}
static ProError mc_hole_wall(ProGeomitem *item,ProError status,ProAppData data){
    McHoleGeometry *g=data;ProSurface surface;ProSrftype type;ProGeomitemdata *raw=NULL;ProError e;double dot=0,offset=0,area,length,delta;int i;
    ProSurfacedata *s;ProCylinderdata *c;
    if(status)return status;
    if((e=ProGeomitemToSurface(item,&surface))||(e=ProSurfaceTypeGet(surface,&type)))return e;
    if(type!=PRO_SRF_CYL)return 0;
    if((e=ProSurfaceDataGet(surface,&raw)))return e;s=raw->data.p_surface_data;c=&s->srf_shape.cylinder;
    for(i=0;i<3;i++){dot+=c->e3[i]*g->normal[i];offset+=(c->origin[i]-g->center[i])*g->normal[i];}
    if(fabs(fabs(dot)-1)>1e-6||!part_close(c->radius,g->radius)){e=PRO_TK_GENERAL_ERROR;goto done;}
    for(i=0;i<3;i++){delta=c->origin[i]-g->center[i]-offset*g->normal[i];if(fabs(delta)>1e-5){e=PRO_TK_GENERAL_ERROR;goto done;}}
    trace("hole cylinder u span x1000",(int)lround(fabs(s->uv_max[0]-s->uv_min[0])*1000));
    trace("hole cylinder v span x1000",(int)lround(fabs(s->uv_max[1]-s->uv_min[1])*1000));
    length=fabs(s->uv_max[1]-s->uv_min[1]);
    if(length<=0||fabs(s->uv_max[0]-s->uv_min[0])<=0||fabs(s->uv_max[0]-s->uv_min[0])>2*3.141592653589793+1e-5){e=PRO_TK_BAD_CONTEXT;goto done;}
    if((e=ProSurfaceAreaEval(surface,&area)))goto done;
    if(fabs(area-fabs(s->uv_max[0]-s->uv_min[0])*c->radius*length)>fmax(1e-7,area*1e-5)){e=PRO_TK_BAD_CONTEXT;goto done;}
    if(g->cylinders&&!part_close(g->length,length)){e=PRO_TK_BAD_CONTEXT;goto done;}
    g->length=length;g->angle+=fabs(s->uv_max[0]-s->uv_min[0]);g->cylinders++;
done:if(raw)ProGeomitemdataFree(&raw);return e;
}
static ProError mc_verify(FdTxn *tx,ProFeattype type,double *volume){
    ProError e;ProFeatStatus status;ProFeattype actual;ProName name;ProMassProperty mass;
    int *parents=NULL,n=0,i,j,bodies=0;
    if((e=part_regenerate(tx->model))||(e=ProFeatureStatusGet(&tx->feature,&status))||
       (e=ProFeatureTypeGet(&tx->feature,&actual))||(e=ProModelitemNameGet((ProModelitem *)&tx->feature,name)))return e;
    tx->stage="verify active feature type and name";
    if(status!=PRO_FEAT_ACTIVE||actual!=type||_wcsicmp(name,tx->name))return PRO_TK_GENERAL_ERROR;
    tx->stage="verify feature parents";
    if((e=ProFeatureParentsGet(&tx->feature,&parents,&n)))return e;
    for(i=0;i<tx->parent_count;i++){
        for(j=0;j<n;j++)if(tx->parents[i]==parents[j])break;
        if(j==n){if(parents)ProArrayFree((ProArray *)&parents);return PRO_TK_GENERAL_ERROR;}
    }if(parents)ProArrayFree((ProArray *)&parents);
    tx->stage="verify single body and material change";
    if((e=sd_body_count(tx->model,&bodies))||(e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)))return e;
    if(bodies!=1||!isfinite(mass.volume)||mass.volume<=0||!tx->has_volume||!isfinite(tx->volume)||
       fabs(mass.volume-tx->volume)<=fmax(1e-9,tx->volume*1e-10))return PRO_TK_GENERAL_ERROR;
    *volume=mass.volume;return 0;
}
static void mc_cleanup(FdTxn *tx,ProError e){
    ProMassProperty mass;int bodies;
    fd_cleanup(tx,e);
    if(e&&tx->before_count&&tx->has_volume){
        if(part_health(tx->model)||sd_body_count(tx->model,&bodies)||bodies!=1||
           ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)||!part_close(mass.volume,tx->volume))tx->rollback_ok=0;
    }
}
