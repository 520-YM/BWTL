static ProError mc_thread_fields(ProElement com,double thread_depth){
    ProError e;
    if((e=model_element(com,PRO_E_HLE_STAN_TYPE,1,PRO_HLE_TAPPED_TYPE,NULL))||
       (e=model_element(com,PRO_E_HLE_THRDSERIS,1,0,NULL))||
       (e=model_element(com,PRO_E_HLE_FITTYPE,1,PRO_HLE_CLOSE_FIT,NULL))||
       (e=model_element(com,PRO_E_HLE_SCREWSIZE,1,0,NULL))||
       (e=model_element(com,PRO_E_HLE_ADD_THREAD,1,PRO_HLE_ADD_THREAD,NULL))||
       (e=model_element(com,PRO_E_HLE_THRD_DEPTH,1,PRO_HLE_VARIABLE_THREAD,NULL))||
       (e=model_element(com,PRO_E_HLE_THRDDEPTH,2,thread_depth,NULL))||
       (e=model_element(com,PRO_E_HLE_TAPERED_STRT_DEPTH_OPT,1,PRO_HOLE_NONE_DEPTH_TYPE,NULL)))return e;
    return model_element(com,PRO_E_HLE_ADD_TAPERED_TIP_ANGLE,1,0,NULL);
}

static ProError mc_thread_size(FdTxn *tx,const PartCommand *cmd){
    const wchar_t *size=cmd->arg[9],*pitch=wcschr(size,L'x')+1;
    wchar_t alternate[PART_TOKEN_MAX];
    ProError e;
    tx->stage="resolve ISO fastener from Creo hole table";
    if((e=ProElementHoleThreadSeriesSet(tx->root,tx->model,(wchar_t *)cmd->arg[8])))return e;
    e=ProElementHoleScrewSizeSet(tx->root,tx->model,(wchar_t *)size);
    /* Factory ISO tables omit the zero before fractional pitches. */
    if(e==PRO_TK_INVALID_NAME&&pitch[0]==L'0'&&pitch[1]==L'.'){
        swprintf_s(alternate,_countof(alternate),L"%.*ls%ls",(int)(pitch-size),size,pitch+1);
        e=ProElementHoleScrewSizeSet(tx->root,tx->model,alternate);
    }
    return e;
}

static ProError mc_verify_thread(FdTxn *tx,const PartCommand *cmd,double thread_depth,double *diameter){
    const ProElemId dia[]={PRO_E_HLE_COM,PRO_E_HLE_HOLEDIAM};
    const ProElemId threaded[]={PRO_E_HLE_COM,PRO_E_HLE_ADD_THREAD};
    const ProElemId depth_type[]={PRO_E_HLE_COM,PRO_E_HLE_THRD_DEPTH};
    const ProElemId depth[]={PRO_E_HLE_COM,PRO_E_HLE_THRDDEPTH};
    ProElement tree=NULL,element=NULL;
    wchar_t *series=NULL,*size=NULL;
    ProError e;
    tx->stage="verify native ISO thread series, size and depth";
    if((e=mc_tree_value(&tx->feature,threaded,2,PRO_HLE_ADD_THREAD,1))||
       (e=mc_tree_value(&tx->feature,depth_type,2,PRO_HLE_VARIABLE_THREAD,1))||
       (e=mc_tree_value(&tx->feature,depth,2,thread_depth,0))||
       (e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||
       (e=ProElementHoleThreadSeriesGet(tree,tx->model,&series))||
       (e=ProElementHoleScrewSizeGet(tree,tx->model,&size))||
       (e=mc_tree_element(tree,dia,2,&element))||
       (e=ProElementDoubleGet(element,NULL,diameter)))goto done;
    if(!series||!size||_wcsicmp(series,cmd->arg[8]))e=PRO_TK_GENERAL_ERROR;
    else if(_wcsicmp(size,cmd->arg[9])){
        const wchar_t *requested=wcschr(cmd->arg[9],L'x')+1,*actual=wcschr(size,L'x');
        if(!actual||actual-size!=requested-cmd->arg[9]-1||
           wcsncmp(size,cmd->arg[9],(size_t)(requested-cmd->arg[9]))||
           requested[0]!=L'0'||requested[1]!=L'.'||wcscmp(actual+1,requested+1))e=PRO_TK_GENERAL_ERROR;
    }
done:
    if(series)ProWstringFree(series);
    if(size)ProWstringFree(size);
    if(tree)ProFeatureElemtreeFree(&tx->feature,tree);
    return e;
}
