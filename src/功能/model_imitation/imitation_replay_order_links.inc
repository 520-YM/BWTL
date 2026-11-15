static int replay_order_position(const ReplayOrder *order,int feature)
{
    for(int i=0;i<order->count;i++)if(order->step[i].id==feature)return i+1;
    return 0;
}

static ProError replay_order_group(ProFeature *feature,const ReplayOrder *order,int *group)
{
    ProGroupStatus status;
    ProError error=ProFeatureGroupStatusGet(feature,&status);
    if(error||status!=PRO_GROUP_MEMBER)return error;
    ProGroup owner;
    error=ProFeatureGroupGet(feature,&owner);
    if(error||owner.id==feature->id)return error;
    *group=replay_order_position(order,owner.id);
    return *group?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError replay_order_section_link(ProFeature *feature,ProFeattype type,int sections,
                                          const ReplayOrder *order,int *linked)
{
    if(!sections||!(type==PRO_FEAT_FIRST_FEAT||type==PRO_FEAT_PROTRUSION||type==PRO_FEAT_CUT||
                    type==PRO_FEAT_CUT_SMT||type==PRO_FEAT_DATUM_SURF||type==PRO_FEAT_WALL||type==PRO_FEAT_DEFORM_AREA))
        return PRO_TK_NO_ERROR;
    ProElement tree=NULL,reference=NULL;
    ProReference link=NULL;
    ProError error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(error==PRO_TK_INVALID_TYPE&&type==PRO_FEAT_PROTRUSION&&replay_blend_subtype(feature))return PRO_TK_NO_ERROR;
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_STD_SECTION,PRO_E_SEC_USE_SKETCH},2,&reference);
    if(!error)error=ProElementReferenceGet(reference,NULL,&link);
    if(error==PRO_TK_E_NOT_FOUND||error==PRO_TK_EMPTY)error=PRO_TK_NO_ERROR;
    if(!error&&link){
        ProMdl owner=NULL;
        int id=0;
        error=ProReferenceIdGet(link,&id);
        if(!error&&id>0)error=ProReferenceOwnerGet(link,&owner);
        if(!error&&id>0&&owner==feature->owner){
            *linked=replay_order_position(order,id);
            if(!*linked)error=PRO_TK_INCOMPLETE;
        }else if(!error&&id>0)*linked=-1;
    }
    if(link)ProReferenceFree(link);
    if(tree)ProFeatureElemtreeFree(feature,tree);
    return error;
}
