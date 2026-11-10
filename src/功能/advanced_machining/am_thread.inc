static ProError am_thread(FdTxn *tx,const PartCommand *cmd,McProfiledHole *hole){
    ProElement com,el;ProError e;int entrance;wchar_t alternate[PART_TOKEN_MAX];
    const wchar_t *size=cmd->arg[9],*x=wcschr(size,L'x');
    hole->kind=MC_THREAD;hole->thread_depth=as_arg(cmd,11)*tx->scale;hole->depth=as_arg(cmd,12)*tx->scale;
#define AT(call) do{if((e=(call)))return e;}while(0)
    AT(model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_HLE_TYPE_SKETCHED,NULL));
    AT(model_element(tx->root,PRO_E_HLE_COM,0,0,&com));
    AT(model_element(com,PRO_E_HLE_TYPE_NEW,1,PRO_HLE_NEW_TYPE_STANDARD,NULL));
    AT(mc_thread_fields(com,hole->thread_depth>0?hole->thread_depth:tx->scale));
    if(!hole->depth){ProElemId path[]={PRO_E_HLE_THRD_DEPTH};AT(mc_tree_element(com,path,1,&el));AT(ProElementIntegerSet(el,PRO_HLE_THRU_THREAD));}
    AT(model_element(com,PRO_E_HLE_ADD_CBORE,1,PRO_HLE_NO_CBORE,NULL));
    AT(model_element(com,PRO_E_HLE_ADD_CSINK,1,PRO_HLE_NO_CSINK,NULL));
    AT(model_element(com,PRO_E_HLE_MAKE_LIGHTWT,1,PRO_HLE_REGULAR,NULL));
    AT(model_element(com,PRO_E_HLE_DEPTH,1,hole->depth?PRO_HLE_STD_VAR_DEPTH:PRO_HLE_STD_THRU_ALL_DEPTH,NULL));
    AT(model_element(com,PRO_E_HLE_DRILLDEPTH,2,hole->depth,NULL));
    AT(model_element(com,PRO_E_HLE_DRILLANGLE,2,118,NULL));
    AT(model_element(com,PRO_E_HLE_DEPTH_DIM_TYPE,1,PRO_HLE_DEP_SHOULDER_DIM_SCHEME,NULL));
    AT(model_element(com,PRO_E_HLE_CRDIR_FLIP,1,PRO_HLE_CR_IN_SIDE_ONE,NULL));
    AT(model_element(com,PRO_E_HLE_ADD_PARAMETERS,1,PRO_HOLE_ADD_PARAMETERS_FLAG,NULL));
    AT(model_element(com,PRO_E_HLE_ADD_NOTE,1,PRO_HOLE_ADD_NOTE_FLAG,NULL));
    AT(model_element(com,PRO_E_HLE_TOP_CLEARANCE,1,PRO_HOLE_GEN_CLRNCE,NULL));
    AT(mc_hole_place(tx,cmd,&hole->geometry,&entrance));
    AT(ProElementHoleThreadSeriesSet(tx->root,tx->model,(wchar_t *)cmd->arg[8]));
    e=ProElementHoleScrewSizeSet(tx->root,tx->model,(wchar_t *)size);
    if(e==PRO_TK_INVALID_NAME&&x&&x[1]==L'0'&&x[2]==L'.'){
        swprintf_s(alternate,_countof(alternate),L"%.*ls%ls",(int)(x+1-size),size,x+2);
        e=ProElementHoleScrewSizeSet(tx->root,tx->model,alternate);
    }
    if(e)return e;
    AT(mc_hole_stock(tx->model,&hole->geometry,entrance,&hole->stock));
    if(hole->depth>=hole->stock)return PRO_TK_BAD_INPUTS;
    AT(fd_create(tx,0));return 0;
#undef AT
}
static ProError am_verify_thread(FdTxn *tx,const PartCommand *cmd,McProfiledHole *hole,double volume){
    ProElement tree=NULL,el;wchar_t *series=NULL,*size=NULL;ProError e;
    ProElemId dia[]={PRO_E_HLE_COM,PRO_E_HLE_HOLEDIAM};
    ProElemId type[]={PRO_E_HLE_COM,PRO_E_HLE_THRD_DEPTH};
    if((e=mc_tree_value(&tx->feature,type,2,hole->depth?PRO_HLE_VARIABLE_THREAD:PRO_HLE_THRU_THREAD,1))||
       (e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||
       (e=ProElementHoleThreadSeriesGet(tree,tx->model,&series))||
       (e=ProElementHoleScrewSizeGet(tree,tx->model,&size))||
       (e=mc_tree_element(tree,dia,2,&el))||(e=ProElementDoubleGet(el,NULL,&hole->diameter)))goto done;
    if(!series||!size||_wcsicmp(series,cmd->arg[8])){e=PRO_TK_GENERAL_ERROR;goto done;}
    if(_wcsicmp(size,cmd->arg[9])){
        const wchar_t *a=wcschr(size,L'x'),*b=wcschr(cmd->arg[9],L'x');double p,q;
        if(wcscmp(series,L"ISO")||!a||!b||a-size!=b-cmd->arg[9]||wcsncmp(size,cmd->arg[9],a-size)||
           !part_number(a+1,&p)||!part_number(b+1,&q)||p!=q){e=PRO_TK_GENERAL_ERROR;goto done;}
    }
    {ProElemId added[]={PRO_E_HLE_COM,PRO_E_HLE_ADD_THREAD};
     ProElemId drill[]={PRO_E_HLE_COM,PRO_E_HLE_DRILLDEPTH},thread_depth[]={PRO_E_HLE_COM,PRO_E_HLE_THRDDEPTH};
     if((e=mc_tree_value(&tx->feature,added,2,PRO_HLE_ADD_THREAD,1)))goto done;
     if(hole->depth&&((e=mc_tree_value(&tx->feature,drill,2,hole->depth,0))||
        (e=mc_tree_value(&tx->feature,thread_depth,2,hole->thread_depth,0))))goto done;}
    {double r=hole->diameter/2,tip=hole->depth?r/tan(118*AS_PI/360):0;
     double expected=AS_PI*r*r*(hole->depth?hole->depth+tip/3:hole->stock);
     if(hole->depth+tip>=hole->stock||fabs(tx->volume-volume-expected)>fmax(1e-7,expected*1e-5)){e=PRO_TK_GENERAL_ERROR;goto done;}}
    hole->geometry.radius=hole->diameter/2;hole->geometry.profiled=1;
    {McProfileWall wall={&tx->feature,&hole->geometry};e=ProSolidSurfaceVisit((ProSolid)tx->model,mc_profile_wall,NULL,&wall);}
    if(!e&&!hole->geometry.cylinders)e=PRO_TK_E_NOT_FOUND;
done:if(series)ProWstringFree(series);if(size)ProWstringFree(size);if(tree)ProFeatureElemtreeFree(&tx->feature,tree);return e;
}
