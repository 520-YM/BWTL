typedef struct {ProMdl model;int id,count;ProGeomitem item;} SfQuiltFind;
static ProError sf_find_quilt(ProQuilt quilt,ProError status,ProAppData data){
    SfQuiltFind *found=data;
    ProGeomitem item;
    ProError e;
    if(status)return status;
    if((e=ProQuiltToGeomitem((ProSolid)found->model,quilt,&item)))return e;
    if(item.id==found->id){found->item=item;found->count++;}
    return PRO_TK_NO_ERROR;
}
static ProError sf_ref(ProMdl model,const wchar_t *token,FdRef *out){
    ProModelitem feature;
    ProFeature owner;
    SfQuiltFind found={0};
    ProError e;
    int id;
    memset(out,0,sizeof(*out));
    if(wcsncmp(token,L"QUILT:",6))return PRO_TK_BAD_INPUTS;
    if(part_integer(token+6,&id)){
        found.model=model;found.id=id;
        if((e=ProSolidQuiltVisit((ProSolid)model,sf_find_quilt,NULL,&found)))return e;
        if(found.count!=1)return PRO_TK_E_NOT_FOUND;
        out->item=found.item;
    }else{
        if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)token+6,&feature))||
           (e=fd_feature_geom((ProFeature *)&feature,PRO_QUILT,&out->item)))return e;
        found.model=model;found.id=out->item.id;
        if((e=ProSolidQuiltVisit((ProSolid)model,sf_find_quilt,NULL,&found)))return e;
        if(found.count!=1)return PRO_TK_E_NOT_FOUND;
    }
    if((e=ProGeomitemFeatureGet(&out->item,&owner)))return e;
    out->owner_feature=owner.id;
    return ProSelectionAlloc(NULL,(ProModelitem *)&out->item,&out->selection);
}
typedef struct {ProMdl model;double area;int count;ProSurface single;} SfArea;
static ProError sf_surface_area(ProSurface surface,ProError status,ProAppData data){
    SfArea *area=data;
    double measured;
    ProError e;
    if(status)return status;
    if((e=ProSurfaceAreaEval(surface,&measured)))return e;
    if(!isfinite(measured)||measured<=0)return PRO_TK_GENERAL_ERROR;
    area->area+=measured;area->count++;area->single=surface;
    return PRO_TK_NO_ERROR;
}
static ProError sf_area(const FdRef *ref,SfArea *area){
    ProQuilt quilt;
    ProError e;
    memset(area,0,sizeof(*area));area->model=ref->item.owner;
    if((e=ProGeomitemToQuilt((ProGeomitem *)&ref->item,&quilt)))return e;
    return ProQuiltSurfaceVisit(quilt,sf_surface_area,NULL,area);
}
