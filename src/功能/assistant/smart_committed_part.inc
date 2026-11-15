typedef struct { unsigned long long hash; ProError error; } SmartPartSignature;
static SmartPartSignature smart_committed_part;

static void smart_signature_add(SmartPartSignature *signature,const void *value,size_t size){
    const unsigned char *bytes=value;
    for(size_t i=0;i<size;i++)signature->hash=(signature->hash^bytes[i])*1099511628211ULL;
}

static ProError smart_signature_feature(ProFeature *feature,ProError status,ProAppData data){
    SmartPartSignature *signature=data;
    if(status)return status;
    ProFeatStatus state;
    ProFeattype type;
    ProName name=L"";
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error)error=ProFeatureStatusGet(feature,&state);
    if(error)return error;
    ProModelitemNameGet(feature,name);
    smart_signature_add(signature,&feature->id,sizeof(feature->id));
    smart_signature_add(signature,&type,sizeof(type));
    smart_signature_add(signature,&state,sizeof(state));
    smart_signature_add(signature,name,(wcslen(name)+1)*sizeof(*name));
    return PRO_TK_NO_ERROR;
}

static ProError smart_signature_dimension(ProDimension *dimension,ProError status,ProAppData data){
    SmartPartSignature *signature=data;
    if(status)return status;
    double value=0;
    ProError error=ProDimensionValueGet(dimension,&value);
    if(error)return error;
    smart_signature_add(signature,&dimension->id,sizeof(dimension->id));
    smart_signature_add(signature,&value,sizeof(value));
    return PRO_TK_NO_ERROR;
}

static SmartPartSignature smart_part_signature(ProMdl model){
    SmartPartSignature signature={14695981039346656037ULL,PRO_TK_NO_ERROR};
    signature.error=ProSolidFeatVisit((ProSolid)model,smart_signature_feature,NULL,&signature);
    if(!signature.error)
        signature.error=ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,smart_signature_dimension,NULL,&signature);
    if(signature.error==PRO_TK_E_NOT_FOUND)signature.error=PRO_TK_NO_ERROR;
    return signature;
}