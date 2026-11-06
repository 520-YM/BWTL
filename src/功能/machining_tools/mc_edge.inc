static ProError mc_edge(FdTxn *tx,const PartCommand *cmd){
    ProElement sets=NULL,set=NULL,refs=NULL,el=NULL,conic=NULL,radii=NULL,radius=NULL,leg=NULL;
    ProCollection collection=NULL;ProCrvcollinstr instr=NULL;ProReference reference=NULL;FdRef edge={0};
    double size;ProError e;int i,j,round=!wcscmp(cmd->arg[0],L"EDGE_ROUND");
#define ME(call) do{e=(call);if(e)goto done;}while(0)
    part_number(cmd->arg[3],&size);size*=tx->scale;
    tx->stage="validate edge size against model bounds";
    {Pro3dPnt box[2];double diagonal=0;
        ME(ProSolidOutlineGet((ProSolid)tx->model,box));
        for(i=0;i<3;i++)diagonal+=(box[1][i]-box[0][i])*(box[1][i]-box[0][i]);
        if(size>=sqrt(diagonal)){e=PRO_TK_BAD_INPUTS;goto done;}
    }
    tx->stage="build explicit-edge round or equal-distance chamfer";
    ME(model_element(tx->root,PRO_E_RNDCH_SETS,0,0,&sets));ME(model_element(sets,PRO_E_RNDCH_SET,0,0,&set));
    if(round){
        ME(model_element(set,PRO_E_RNDCH_SHAPE_OPTIONS,1,PRO_ROUND_TYPE_CONSTANT,NULL));
        ME(model_element(set,PRO_E_RNDCH_COMPOUND_CONIC,0,0,&conic));
        ME(model_element(conic,PRO_E_RNDCH_CONIC_TYPE,1,PRO_ROUND_CONIC_DISABLE,NULL));
    }else{
        ME(model_element(set,PRO_E_RNDCH_DIMENSIONAL_SCHEMA,1,PRO_CHM_D_X_D,NULL));
        ME(model_element(set,PRO_E_RNDCH_CHAMFER_SHAPE,1,PRO_CHM_OFFSET_SURFACE,NULL));
    }
    ME(model_element(set,PRO_E_RNDCH_REFERENCES,0,0,&refs));
    if(round)ME(model_element(refs,PRO_E_RNDCH_REFERENCE_TYPE,1,PRO_ROUND_REF_EDGE,NULL));
    ME(model_element(refs,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&el));
    ME(ProCrvcollectionAlloc(&collection));
    for(i=4;i<cmd->count;i++){
        ProEdge handle;ProSurface neighbors[2];ProEdge neighbors_edge[2];ProEnttype edge_type;ProGeomitem planes[2];ProVector p[2],n[2];double dot;
        tx->stage="resolve solid edge and two adjacent solid faces";
        ME(fd_ref_resolve(tx->model,cmd->arg[i],PRO_EDGE,&edge));ME(ProGeomitemToEdge(&edge.item,&handle));
        ME(ProEdgeNeighborsGet(handle,&neighbors_edge[0],&neighbors_edge[1],&neighbors[0],&neighbors[1]));
        if(!neighbors[0]||!neighbors[1]||neighbors[0]==neighbors[1]){e=PRO_TK_BAD_INPUTS;goto done;}
        ME(ProEdgeTypeGet(handle,&edge_type));if(edge_type!=PRO_ENT_LINE){e=PRO_TK_BAD_INPUTS;goto done;}
        for(j=0;j<2;j++){ME(ProSurfaceToGeomitem((ProSolid)tx->model,neighbors[j],&planes[j]));ME(fd_plane_data(&planes[j],p[j],n[j]));}
        dot=n[0][0]*n[1][0]+n[0][1]*n[1][1]+n[0][2]*n[1][2];
        if(fabs(dot)>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}
        ME(ProSelectionToReference(edge.selection,&reference));
        ME(ProCrvcollinstrAlloc(PRO_CURVCOLL_ADD_ONE_INSTR,&instr));ME(ProCrvcollinstrReferenceAdd(instr,reference));
        ME(ProCrvcollectionInstructionAdd(collection,instr));
        ProCrvcollinstrFree(instr);instr=NULL;ProReferenceFree(reference);reference=NULL;
        for(j=0;j<tx->parent_count;j++)if(tx->parents[j]==edge.owner_feature)break;
        if(j==tx->parent_count)tx->parents[tx->parent_count++]=edge.owner_feature;
        fd_ref_free(&edge);
    }
    ME(ProElementCollectionSet(el,collection));
    ME(model_element(set,PRO_E_RNDCH_RADII,0,0,&radii));ME(model_element(radii,PRO_E_RNDCH_RADIUS,0,0,&radius));
    ME(model_element(radius,PRO_E_RNDCH_LEG1,0,0,&leg));
    ME(model_element(leg,PRO_E_RNDCH_LEG_TYPE,1,PRO_ROUND_RADIUS_TYPE_VALUE,NULL));ME(model_element(leg,PRO_E_RNDCH_LEG_VALUE,2,size,NULL));
    ME(model_element(tx->root,PRO_E_RNDCH_ATTACH_TYPE,1,PRO_ROUND_ATTACHED,NULL));
    ME(fd_create(tx,0));
done:fd_ref_free(&edge);if(reference)ProReferenceFree(reference);if(instr)ProCrvcollinstrFree(instr);if(collection)ProCollectionFree(&collection);return e;
#undef ME
}
