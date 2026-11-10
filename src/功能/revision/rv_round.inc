typedef struct {double radius;int matches;} RevisionRound;

static ProError rv_round_surface(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    RevisionRound *check=data;
    ProSurface surface;
    ProGeomitemdata *raw=NULL;
    ProError error=ProGeomitemToSurface(item,&surface);
    if(!error)error=ProSurfaceDataGet(surface,&raw);
    if(!error){
        const ProSurfacedata *s=raw->data.p_surface_data;
        if(s->type==PRO_SRF_CYL&&part_close(s->srf_shape.cylinder.radius,check->radius))check->matches++;
        ProGeomitemdataFree(&raw);
    }
    return error;
}

static ProError rv_round_geometry(ProFeature *feature,double radius){
    RevisionRound check={radius,0};
    ProError error=ProFeatureGeomitemVisit(feature,PRO_SURFACE,rv_round_surface,NULL,&check);
    return error?error:check.matches?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError rv_round(Revision *tx,const PartCommand *cmd,PartText *out){
    const ProElemId path[]={PRO_E_RNDCH_SETS,0,PRO_E_RNDCH_RADII,0,PRO_E_RNDCH_LEG1,PRO_E_RNDCH_LEG_VALUE};
    const ProElemId schema[]={PRO_E_RNDCH_SETS,0,PRO_E_RNDCH_SHAPE_OPTIONS};
    ProFeature features[8];
    int n=(int)as_arg(cmd,3);
    double next=as_arg(cmd,2)/tx->mm;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<n;i++){
        ProFeature *f=&features[i];
        ProElement tree=NULL,element;
        int type;
        double old=as_arg(cmd,5+2*i)/tx->mm;
        ProDimension dim;
        error=rv_feature(tx->model,cmd->arg[4+2*i],PRO_FEAT_ROUND,f);
        if(!error)error=ProFeatureElemtreeExtract(f,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
        if(!error)error=md_array_value(tree,schema,3,1,0,&element);
        if(!error)error=ProElementIntegerGet(element,NULL,&type);
        if(!error&&type!=PRO_ROUND_TYPE_CONSTANT)error=PRO_TK_BAD_CONTEXT;
        if(!error&&!md_array_value(tree,schema,3,1,1,&element))error=PRO_TK_BAD_CONTEXT;
        if(!error)error=rv_elem_dim(f,path,_countof(path),(1<<1)|(1<<3),&dim);
        if(!error)error=rv_round_geometry(f,old);
        if(!error)error=rv_add(tx,dim,f->id,old,next);
        if(tree)ProFeatureElemtreeFree(f,tree);
    }
    if(!error)error=rv_apply(tx);
    for(int i=0;!error&&i<n;i++){
        error=mc_edge_size(&features[i],next);
        if(!error)error=rv_round_geometry(&features[i],next);
    }
    if(!error)part_text_add(out,L"已将 %d 个等半径圆角特征统一为 R%.8g mm，并核对实际圆柱过渡面。",n,as_arg(cmd,2));
    return error;
}
