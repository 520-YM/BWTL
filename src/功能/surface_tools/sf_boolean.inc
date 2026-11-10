static ProError sf_body_ref(ProMdl model,const wchar_t *token,FdRef *ref){
    ProSolidBody *bodies=NULL;
    ProFeature *features=NULL;
    int n=0,id,count=0;
    ProError e;
    if(!part_integer(token+5,&id))return PRO_TK_BAD_INPUTS;
    if((e=ProSolidBodiesCollect((ProSolid)model,&bodies))||(e=ProArraySizeGet((ProArray)bodies,&n)))goto done;
    e=PRO_TK_E_NOT_FOUND;
    for(int i=0;i<n;i++){
        ProSolidBodyState state;
        if(bodies[i].id!=id)continue;
        if((e=ProSolidBodyStateGet(&bodies[i],&state)))goto done;
        if(state!=PRO_BODY_STATE_ACTIVE){e=PRO_TK_BAD_CONTEXT;goto done;}
        if((e=ProSolidBodyFeaturesGet(&bodies[i],&features))||
           (e=ProArraySizeGet((ProArray)features,&count)))goto done;
        if(count<1){e=PRO_TK_E_NOT_FOUND;goto done;}
        ref->item=*(ProGeomitem *)&bodies[i];ref->owner_feature=features[count-1].id;
        e=ProSelectionAlloc(NULL,(ProModelitem *)&bodies[i],&ref->selection);
        break;
    }
done:
    if(features)ProArrayFree((ProArray *)&features);
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return e;
}
static ProError sf_boolean(FdTxn *tx,const PartCommand *cmd){
    FdRef target={0},tool={0};
    int operation=!wcscmp(cmd->arg[3],L"MERGE")?PRO_MERGE_BOOL_TYPE:!wcscmp(cmd->arg[3],L"SUBTRACT")?PRO_SUBTRACT_BOOL_TYPE:PRO_INTERSECT_BOOL_TYPE;
    ProError e;
    if((e=sf_body_ref(tx->model,cmd->arg[4],&target))||
       (e=sf_body_ref(tx->model,cmd->arg[5],&tool)))goto done;
    if(target.item.id==tool.item.id){e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=model_element(tx->root,PRO_E_BOOLEAN_TYPE,1,operation,NULL))||
       (e=fd_ref_element(tx,tx->root,PRO_E_TARGET_BODY,&target))||
       (e=fd_ref_element(tx,tx->root,PRO_E_TOOL_BODIES,&tool)))goto done;
    if(operation==PRO_SUBTRACT_BOOL_TYPE)e=model_element(tx->root,PRO_E_KEEP_TOOLS,1,PRO_KEEP_TOOL_NO,NULL);
    /* Boolean dependencies are BODY references. Feature parents omit some
       body-contributing features, so verify the native BODY references below. */
    if(!e)tx->parent_count=0;
done:
    fd_ref_free(&target);fd_ref_free(&tool);
    return e;
}

static ProError sf_boolean_verify(FdTxn *tx,const PartCommand *cmd){
    for(int i=4;i<=5;i++){
        FdRef expected={0};
        int id;
        ProError e;
        if(!part_integer(cmd->arg[i]+5,&id))return PRO_TK_BAD_INPUTS;
        if((e=ProModelitemInit(tx->model,id,PRO_BODY,&expected.item)))return e;
        e=sd_projection_reference(tx->model,&tx->feature,&expected);
        if(e)return e;
    }
    return PRO_TK_NO_ERROR;
}
