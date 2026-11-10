typedef struct {ProDimension dim;double old,next;} RevisionEdit;
typedef struct {
    ProMdl model;
    double mm;
    RevisionEdit edits[24];
    int count,applied;
    ProOutline before;
    ProMassProperty mass;
} Revision;

static int rv_recovery_uncertain;
#ifdef BWTL_REVISION_FAULT_TEST
static int rv_test_after_first,rv_test_fault_hit;
#endif

static ProError rv_geometry(ProMdl model,ProOutline bounds,ProMassProperty *mass){
    ProSolidBody body;
    ProError error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodyOutlineGet(&body,bounds);
    return error?error:qa_geometry_mass(model,mass);
}

static ProError rv_begin(Revision *tx,ProMdl model){
    memset(tx,0,sizeof(*tx));
    tx->model=model;
    ProError error=qa_mm_per_unit(model,&tx->mm);
    if(!error)error=part_health(model);
    return error?error:rv_geometry(model,tx->before,&tx->mass);
}

static ProError rv_feature(ProMdl model,const wchar_t *name,ProFeattype expected,ProFeature *feature){
    ProFeattype type;
    ProFeatStatus state;
    int number;
    ProError error=dv_feature(model,name,feature,&number);
    if(!error)error=ProFeatureTypeGet(feature,&type);
    if(!error)error=ProFeatureStatusGet(feature,&state);
    return error?error:type==expected&&state==PRO_FEAT_ACTIVE?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError rv_add(Revision *tx,ProDimension dim,int feature,double old,double next){
    ProFeature owner;
    ProBoolean related,negative;
    ProDimensiontype type;
    ProDimension parent;
    double value;
    ProError error=ProDimensionOwnerfeatureGet(&dim,&owner);
    if(!error)error=ProDimensionIsReldriven(&dim,&related);
    if(!error)error=ProDimensionIsRegenednegative(&dim,&negative);
    if(!error)error=ProDimensionTypeGet(&dim,&type);
    if(!error)error=ProDimensionValueGet(&dim,&value);
    if(error)return error;
    if(dim.type!=PRO_DIMENSION||owner.owner!=tx->model||owner.id!=feature||related||negative||!part_close(value,old)||
       (type!=PRODIMTYPE_LINEAR&&type!=PRODIMTYPE_DIAMETER&&type!=PRODIMTYPE_RADIUS&&type!=PRODIMTYPE_ANGLE))
        return PRO_TK_BAD_CONTEXT;
    error=ProDimensionParentGet(&dim,&parent);
    if(error!=PRO_TK_E_NOT_FOUND)return error?error:PRO_TK_BAD_CONTEXT;
    if(tx->count==_countof(tx->edits))return PRO_TK_OUT_OF_RANGE;
    for(int i=0;i<tx->count;i++)if(tx->edits[i].dim.id==dim.id)return PRO_TK_BAD_INPUTS;
    tx->edits[tx->count++]=(RevisionEdit){dim,value,next};
    return PRO_TK_NO_ERROR;
}

static ProError rv_elem_dim(ProFeature *feature,const ProElemId *ids,int n,int array_mask,ProDimension *dim){
    ProElempath path=NULL;
    ProElempathItem p[8];
    int id;
    if(n>_countof(p))return PRO_TK_BAD_INPUTS;
    ProError error=ProElempathAlloc(&path);
    if(error)return error;
    for(int i=0;i<n;i++){
        p[i].type=(array_mask&(1<<i))?PRO_ELEM_PATH_ITEM_TYPE_INDEX:PRO_ELEM_PATH_ITEM_TYPE_ID;
        p[i].path_item.elem_id=ids[i];
    }
    error=ProElempathDataSet(path,p,n);
    if(!error)error=ProFeatureElemDimensionIdGet(feature,path,&id);
    ProElempathFree(&path);
    if(!error)*dim=(ProDimension){PRO_DIMENSION,id,feature->owner};
    return error;
}

static ProError rv_read_dims(Revision *tx,int original){
    for(int i=0;i<tx->count;i++){
        RevisionEdit *edit=&tx->edits[i];
        double value;
        ProError error=ProDimensionValueGet(&edit->dim,&value);
        if(error)return error;
        if(!part_close(value,original?edit->old:edit->next))return PRO_TK_GENERAL_ERROR;
    }
    return PRO_TK_NO_ERROR;
}

static ProError rv_apply(Revision *tx){
    for(int i=0;i<tx->count;i++){
        RevisionEdit *edit=&tx->edits[i];
        if(part_close(edit->old,edit->next))continue;
        tx->applied=i+1;
        ProError error=ProDimensionValueSet(&edit->dim,edit->next);
        if(error)return error;
#ifdef BWTL_REVISION_FAULT_TEST
        if(rv_test_after_first){rv_test_fault_hit=1;return PRO_TK_GENERAL_ERROR;}
#endif
    }
    if(!tx->applied)return PRO_TK_NO_CHANGE;
    ProError error=part_regenerate(tx->model);
    return error?error:rv_read_dims(tx,0);
}

static int rv_geometry_restored(Revision *tx){
    ProOutline now;
    ProMassProperty mass;
    if(part_health(tx->model)||rv_geometry(tx->model,now,&mass)||
       !part_close(tx->mass.volume,mass.volume)||!part_close(tx->mass.surface_area,mass.surface_area))return 0;
    for(int i=0;i<2;i++)for(int j=0;j<3;j++)if(!part_close(tx->before[i][j],now[i][j]))return 0;
    return 1;
}

static int rv_restore(Revision *tx){
    int ok=1;
    for(int i=tx->applied-1;i>=0;i--){
        RevisionEdit *edit=&tx->edits[i];
        double current;
        if(!ProDimensionValueGet(&edit->dim,&current)&&part_close(current,edit->old))continue;
        if(ProDimensionValueSet(&edit->dim,edit->old))ok=0;
    }
    if(part_regenerate(tx->model)||rv_read_dims(tx,1)||!rv_geometry_restored(tx))ok=0;
    if(!ok)rv_recovery_uncertain=1;
    return ok;
}

static ProError rv_symbol(Revision *tx,const wchar_t *name,int owner,double old_mm,double new_mm,int circle){
    UxDimension selected;
    ProDimensiontype type;
    ProFeature feature;
    ProFeatStatus status;
    ProError error=ux_dimension(tx->model,name,&selected);
    if(!error)error=ProDimensionOwnerfeatureGet(&selected.dim,&feature);
    if(!error)error=ProFeatureStatusGet(&feature,&status);
    if(!error)error=ProDimensionTypeGet(&selected.dim,&type);
    if(error)return error;
    if(status!=PRO_FEAT_ACTIVE||(owner&&feature.id!=owner))return PRO_TK_BAD_CONTEXT;
    if(circle){
        if(type==PRODIMTYPE_RADIUS){old_mm/=2;new_mm/=2;}
        else if(type!=PRODIMTYPE_DIAMETER)return PRO_TK_BAD_CONTEXT;
    }else if(type!=PRODIMTYPE_LINEAR)return PRO_TK_BAD_CONTEXT;
    return rv_add(tx,selected.dim,feature.id,old_mm/tx->mm,new_mm/tx->mm);
}
