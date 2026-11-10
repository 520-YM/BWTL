static ProError am_edge_ref(FdTxn *tx,const wchar_t *token,FdRef *ref,ProSurface adjacent[2]){
    ProEdge edge,next[2];ProQuilt quilt;ProError e;
    if((e=fd_ref_resolve(tx->model,token,PRO_EDGE,ref))||(e=ProGeomitemToEdge(&ref->item,&edge))||
       (e=ProEdgeNeighborsGet(edge,&next[0],&next[1],&adjacent[0],&adjacent[1])))return e;
    if(!adjacent[0]||!adjacent[1]||adjacent[0]==adjacent[1])return PRO_TK_BAD_INPUTS;
    for(int i=0;i<2;i++){
        e=ProSurfaceQuiltGet((ProSolid)tx->model,adjacent[i],&quilt);
        if(e!=PRO_TK_E_NOT_FOUND)return e?e:PRO_TK_BAD_INPUTS;
    }
    return 0;
}
static ProError am_edge_chain(ProElement element,const FdRef *edge,int tangent){
    ProCrvcollinstr instruction=NULL;ProCollection collection=NULL;ProReference reference=NULL;ProError e;
    if((e=ProSelectionToReference(edge->selection,&reference))||
       (e=ProCrvcollinstrAlloc(tangent?PRO_CURVCOLL_TAN_INSTR:PRO_CURVCOLL_ADD_ONE_INSTR,&instruction))||
       (e=ProCrvcollinstrReferenceAdd(instruction,reference))||
       (tangent&&(e=ProCrvcollinstrAttributeSet(instruction,PRO_CURVCOLL_ALL)))||
       (e=ProCrvcollectionAlloc(&collection))||
       (e=ProCrvcollectionInstructionAdd(collection,instruction))||
       (e=ProElementCollectionSet(element,collection)))goto done;
    if(tangent){
        ProSelection *edges=NULL;int count=0;
        e=ProCrvcollectionRegenerate(collection,&edges,&count);
        if(!e&&(count<1||count>64))e=PRO_TK_OUT_OF_RANGE;
        trace("actual tangent chain edges",count);
        if(edges)ProSelectionarrayFree(edges);
    }
done:if(collection)ProCollectionFree(&collection);if(instruction)ProCrvcollinstrFree(instruction);if(reference)ProReferenceFree(reference);return e;
}
static ProError am_radius(FdTxn *tx,ProElement radii,double first,double second,int chamfer,int endpoint,const FdRef *edge){
    ProElement radius,leg,point;ProReference reference=NULL;ProSelection selection=NULL;ProModelitem vertex;
    ProVector xyz;ProUvParam parameters={endpoint?1:0,0};ProEdge geometry;ProError e;
    if((e=model_element(radii,PRO_E_RNDCH_RADIUS,0,0,&radius)))return e;
    if(endpoint>=0){
        vertex=*(ProModelitem *)&edge->item;vertex.type=endpoint?PRO_EDGE_END:PRO_EDGE_START;
        if((e=ProSelectionAlloc(NULL,&vertex,&selection))||
           (e=ProGeomitemToEdge((ProGeomitem *)&edge->item,&geometry))||
           (e=ProEdgeXyzdataEval(geometry,endpoint?1:0,xyz,NULL,NULL,NULL))||
           (e=ProSelectionToReference(selection,&reference))||
           (e=ProReferenceParamsSet(reference,parameters))||(e=ProReferencePointSet(reference,xyz))||
           (e=model_element(radius,PRO_E_STD_POINT_COLLECTION_APPL,0,0,&point))||
           (e=ProElementReferenceSet(point,reference)))goto done;
    }
    if((e=model_element(radius,PRO_E_RNDCH_LEG1,0,0,&leg))||
       (e=model_element(leg,PRO_E_RNDCH_LEG_TYPE,1,PRO_ROUND_RADIUS_TYPE_VALUE,NULL))||
       (e=model_element(leg,PRO_E_RNDCH_LEG_VALUE,2,first,NULL)))goto done;
    if(chamfer){
        if((e=model_element(radius,PRO_E_RNDCH_LEG2,0,0,&leg))||
           (e=model_element(leg,PRO_E_RNDCH_LEG_TYPE,1,PRO_ROUND_RADIUS_TYPE_VALUE,NULL)))goto done;
        e=model_element(leg,PRO_E_RNDCH_LEG_VALUE,2,second,NULL);
    }
done:if(selection)ProSelectionFree(&selection);if(reference)ProReferenceFree(reference);return e;
}
static ProError am_edge(FdTxn *tx,const PartCommand *cmd,int kind){
    FdRef edge={0},face={0};ProSurface adjacent[2];ProElement sets,set,refs,collection,radii,conic;
    int chamfer=kind==AM_CHAMFER;ProError e;double first,second;
#define AE(call) do{if((e=(call)))goto done;}while(0)
    {Pro3dPnt bounds[2];double diagonal=0,size=0;
     AE(ProSolidOutlineGet((ProSolid)tx->model,bounds));
     for(int i=0;i<3;i++)diagonal+=pow(bounds[1][i]-bounds[0][i],2);
     size=chamfer?(!wcscmp(cmd->arg[5],L"AD")?as_arg(cmd,7):fmax(as_arg(cmd,6),as_arg(cmd,7))):
         kind==AM_VARIABLE?fmax(as_arg(cmd,4),as_arg(cmd,5)):as_arg(cmd,4);
     if(size*tx->scale>=sqrt(diagonal)/2){e=PRO_TK_BAD_INPUTS;goto done;}}
    AE(am_edge_ref(tx,cmd->arg[3],&edge,adjacent));
    tx->parents[tx->parent_count++]=edge.owner_feature;
    AE(model_element(tx->root,PRO_E_RNDCH_SETS,0,0,&sets));AE(model_element(sets,PRO_E_RNDCH_SET,0,0,&set));
    if(chamfer){
        ProSurface selected;
        AE(fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&face));AE(ProGeomitemToSurface(&face.item,&selected));
        if(selected!=adjacent[0]&&selected!=adjacent[1]){e=PRO_TK_BAD_INPUTS;goto done;}
        AE(model_element(set,PRO_E_RNDCH_DIMENSIONAL_SCHEMA,1,!wcscmp(cmd->arg[5],L"DD")?PRO_CHM_D1_X_D2:PRO_CHM_ANG_X_D,NULL));
        AE(model_element(set,PRO_E_RNDCH_CHAMFER_SHAPE,1,PRO_CHM_OFFSET_SURFACE,NULL));
        /* Creo's angle-distance schema stores distance in LEG1, angle in LEG2. */
        first=as_arg(cmd,!wcscmp(cmd->arg[5],L"AD")?7:6)*tx->scale;
        second=as_arg(cmd,!wcscmp(cmd->arg[5],L"AD")?6:7)*(!wcscmp(cmd->arg[5],L"AD")?1:tx->scale);
    }else{
        first=as_arg(cmd,4)*tx->scale;second=as_arg(cmd,kind==AM_VARIABLE?5:4)*tx->scale;
        AE(model_element(set,PRO_E_RNDCH_SHAPE_OPTIONS,1,kind==AM_VARIABLE?PRO_ROUND_TYPE_VARIABLE:PRO_ROUND_TYPE_CONSTANT,NULL));
        AE(model_element(set,PRO_E_RNDCH_COMPOUND_CONIC,0,0,&conic));
        AE(model_element(conic,PRO_E_RNDCH_CONIC_TYPE,1,PRO_ROUND_CONIC_DISABLE,NULL));
    }
    AE(model_element(set,PRO_E_RNDCH_REFERENCES,0,0,&refs));
    AE(model_element(refs,PRO_E_RNDCH_REFERENCE_TYPE,1,PRO_ROUND_REF_EDGE,NULL));
    if(chamfer)AE(fd_ref_element(tx,refs,PRO_E_RNDCH_REFERENCE_SURFACE,&face));
    AE(model_element(refs,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&collection));AE(am_edge_chain(collection,&edge,kind==AM_TANGENT));
    AE(model_element(set,PRO_E_RNDCH_RADII,0,0,&radii));
    AE(am_radius(tx,radii,first,second,chamfer,kind==AM_VARIABLE?0:-1,&edge));
    if(kind==AM_VARIABLE)AE(am_radius(tx,radii,second,0,0,1,&edge));
    AE(model_element(tx->root,PRO_E_RNDCH_ATTACH_TYPE,1,PRO_ROUND_ATTACHED,NULL));
    AE(fd_create(tx,0));
done:fd_ref_free(&edge);fd_ref_free(&face);return e;
#undef AE
}
