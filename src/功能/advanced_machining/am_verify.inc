static ProError am_set_element(ProElement tree,int radius,int leg,ProElemId leaf,ProElement *element){
    ProElempath path=NULL;ProElempathItem p[6];ProError e;
    int ids[]={PRO_E_RNDCH_SETS,0,PRO_E_RNDCH_RADII,radius,leg,leaf};
    if((e=ProElempathAlloc(&path)))return e;
    for(int i=0;i<6;i++){p[i].type=(i==1||i==3)?PRO_ELEM_PATH_ITEM_TYPE_INDEX:PRO_ELEM_PATH_ITEM_TYPE_ID;p[i].path_item.elem_id=ids[i];}
    e=ProElempathDataSet(path,p,6);if(!e)e=ProElemtreeElementGet(tree,path,element);ProElempathFree(&path);return e;
}
static ProError am_verify_edge(FdTxn *tx,const PartCommand *cmd,int kind){
    ProElement tree=NULL,element;ProError e;double actual,expected;
    if((e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)))return e;
    for(int i=0;i<(kind==AM_TANGENT?1:2);i++){
        int radius=kind==AM_VARIABLE?i:0;
        ProElemId leg=kind==AM_CHAMFER&&i?PRO_E_RNDCH_LEG2:PRO_E_RNDCH_LEG1;
        expected=as_arg(cmd,kind==AM_CHAMFER?(!wcscmp(cmd->arg[5],L"AD")?7-i:6+i):4+i)*
            (kind==AM_CHAMFER&&i&&!wcscmp(cmd->arg[5],L"AD")?1:tx->scale);
        if((e=am_set_element(tree,radius,leg,PRO_E_RNDCH_LEG_VALUE,&element))||
           (e=ProElementDoubleGet(element,NULL,&actual)))goto done;
        if(!part_close(actual,expected)){e=PRO_TK_GENERAL_ERROR;goto done;}
    }
    if(kind==AM_VARIABLE){
        ProElemId ids[]={PRO_E_RNDCH_SETS,0,PRO_E_RNDCH_SHAPE_OPTIONS};
        ProElempath path=NULL;ProElempathItem p[3];int type;
        if((e=ProElempathAlloc(&path)))goto done;
        for(int i=0;i<3;i++){p[i].type=i==1?PRO_ELEM_PATH_ITEM_TYPE_INDEX:PRO_ELEM_PATH_ITEM_TYPE_ID;p[i].path_item.elem_id=ids[i];}
        e=ProElempathDataSet(path,p,3);if(!e)e=ProElemtreeElementGet(tree,path,&element);ProElempathFree(&path);
        if(e||(e=ProElementIntegerGet(element,NULL,&type)))goto done;
        if(type!=PRO_ROUND_TYPE_VARIABLE)e=PRO_TK_GENERAL_ERROR;
    }
done:if(tree)ProFeatureElemtreeFree(&tx->feature,tree);return e;
}
typedef struct {AmHole *hole;double depth,sign,scale;int floor,locate;ProMdl model;int feature;} AmFloor;
static ProError am_floor(ProGeomitem *item,ProError status,ProAppData data){
    AmFloor *f=data;ProVector point,normal,delta;ProSurface surface;double area;ProError e;
    if(status)return status;
    if(fd_plane_data(item,point,normal))return 0;
    if(fabs(fabs(ad_dot(normal,f->hole->axis))-1)>1e-6)return 0;
    ad_sub(point,f->hole->center,delta);
    if(f->locate&&fabs(ad_dot(delta,f->hole->axis)-f->sign*f->depth)>0.001*f->scale)return 0;
    if((e=ProGeomitemToSurface(item,&surface))||(e=ProSurfaceAreaEval(surface,&area)))return e;
    if(fabs(area-AS_PI*pow(f->hole->radius,2))<=fmax(1e-7,area*1e-5))f->floor++;
    return 0;
}
static ProError am_floor_final(ProSurface surface,ProError status,ProAppData data){
    AmFloor *f=data;ProGeomitem item;ProFeature feature;ProError e;
    if(status)return status;
    if((e=ProSurfaceToGeomitem((ProSolid)f->model,surface,&item))||(e=ProGeomitemFeatureGet(&item,&feature)))return e;
    return feature.id==f->feature?am_floor(&item,0,data):0;
}
static ProError am_verify_floor(FdTxn *tx,const PartCommand *cmd,AmHole *hole,int kind){
    AmFloor f={hole,as_arg(cmd,kind==AM_SPOTFACE?6:kind==AM_AXIS?7:10)*tx->scale,1,tx->scale,0,kind!=AM_RADIAL};
    ProError e;
    if(!f.depth)return 0;
    f.model=tx->model;f.feature=tx->feature.id;
    if(kind!=AM_RADIAL&&!wcscmp(cmd->arg[kind==AM_SPOTFACE?7:8],L"NEG"))f.sign=-1;
    e=ProSolidSurfaceVisit((ProSolid)tx->model,am_floor_final,NULL,&f);
    return e?e:f.floor?0:PRO_TK_GENERAL_ERROR;
}
