typedef struct {ProMdl model;double mm;} OrganizeView;

static ProError org_feature(ProMdl model,const wchar_t *reference,ProFeature *feature){
    int id;
    if(qa_ref(reference,L"ID:")&&part_integer(reference+3,&id))
        return ProFeatureInit((ProSolid)model,id,feature);
    return ux_named_feature(model,reference,feature);
}

static ProError org_bores(const OrganizeView *view,DeliveryBores **out){
    DeliveryBores *scan=calloc(1,sizeof(*scan));
    if(!scan)return PRO_TK_OUT_OF_MEMORY;
    ProError error=dv_bores_scan(view->model,0,view->mm,scan);
    if(error){free(scan);return error;}
    *out=scan;
    return PRO_TK_NO_ERROR;
}

static const wchar_t *org_bore_class(int classification){
    if(classification==2)return L"直通";
    if(classification==3)return L"平底盲孔";
    return L"端型待核对";
}

static ProError org_read_status(ProFeature *feature,ProFeatStatus *status,unsigned *flags){
    ProError error=ProFeatureStatusGet(feature,status);
    return error?error:ProFeatureStatusflagsGet(feature,flags);
}
