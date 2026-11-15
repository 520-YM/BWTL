static ProError replay_corner_reference(ImitationReplay *replay,ProReference source,ProReference *target)
{
    ProType type;
    ProError error=ProReferenceTypeGet(source,&type);
    if(error)return error;
    double parameter=0;
    switch(type){
    case PRO_EDGE_START:break;
    case PRO_EDGE_END:parameter=1;break;
    case PRO_EDGE:case PRO_EDGE_PNT:{
        ProUvParam value;
        error=ProReferenceParamsGet(source,value);
        if(error)return error;
        if(!isfinite(value[0])||(fabs(value[0])>1e-8&&fabs(value[0]-1)>1e-8))
            return PRO_TK_UNSUPPORTED;
        parameter=value[0]>0.5?1:0;
        break;
    }
    default:return PRO_TK_UNSUPPORTED;
    }

    ProMdl owner=NULL;
    error=ProReferenceOwnerGet(source,&owner);
    if(error||owner!=replay->history_owner)return error?error:PRO_TK_BAD_CONTEXT;
    int id;
    ProEdge old_edge,new_edge;
    ProModelitem mapped;
    ProVector old_point,endpoints[2];
    error=ProReferenceIdGet(source,&id);
    if(!error)error=ProEdgeInit((ProSolid)owner,id,&old_edge);
    if(!error)error=ProEdgeXyzdataEval(old_edge,parameter,old_point,NULL,NULL,NULL);
    if(!error)error=replay_reference_item(replay,source,&mapped,1);
    if(!error)error=ProGeomitemToEdge(&mapped,&new_edge);
    for(int i=0;!error&&i<2;i++)error=ProEdgeXyzdataEval(new_edge,i,endpoints[i],NULL,NULL,NULL);
    if(error)return error;
    const int start=ad_near(old_point,endpoints[0],0.0002);
    const int end=ad_near(old_point,endpoints[1],0.0002);
    if(start==end)return start?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;

    ProSelection selection=NULL;
    ProUvParam location={end,0};
    mapped.type=end?PRO_EDGE_END:PRO_EDGE_START;
    error=ProSelectionAlloc(NULL,&mapped,&selection);
    if(!error)error=ProSelectionToReference(selection,target);
    if(!error)error=ProReferenceParamsSet(*target,location);
    if(!error)error=ProReferencePointSet(*target,endpoints[end]);
    if(selection)ProSelectionFree(&selection);
    if(error&&*target){ProReferenceFree(*target);*target=NULL;}
    return error;
}
