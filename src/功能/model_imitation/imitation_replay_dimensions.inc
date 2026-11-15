typedef struct {
    ProDimensiontype type;
    double value;
    ProDimension target;
    int count;
} ReplayDimensionMatch;

static ProError replay_match_dimension(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    if(dimension->type!=PRO_DIMENSION)return PRO_TK_NO_ERROR;
    ReplayDimensionMatch *match=data;
    ProDimensiontype type;
    double value;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error&&type==match->type&&fabs(value-match->value)<=fmax(1e-8,fabs(value)*1e-8)){
        match->target=*dimension;
        match->count++;
    }
    return error;
}

static ProError replay_dimension_item(ImitationReplay *replay,ProMdl owner,int id,ProModelitem *target)
{
    ProDimension original;
    ProFeature source;
    ReplayDimensionMatch match={0};
    ProError error=ProModelitemInit(owner,id,PRO_DIMENSION,&original);
    if(!error)error=ProDimensionOwnerfeatureGet(&original,&source);
    if(!error)error=ProDimensionTypeGet(&original,&match.type);
    if(!error)error=ProDimensionValueGet(&original,&match.value);
    if(error)return error;
    int mapped=0;
    for(int i=0;i<replay->count;i++)if(replay->mapped[i].source==source.id){
        mapped=replay->mapped[i].target;
        break;
    }
    if(!mapped)return PRO_TK_E_NOT_FOUND;
    ProFeature feature;
    error=ProFeatureInit((ProSolid)replay->target,mapped,&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&match);
    if(error)return error;
    if(match.count!=1)return match.count?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;
    *target=match.target;
    return PRO_TK_NO_ERROR;
}
