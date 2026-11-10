static ProError sh_verify(FdTxn *tx,const PartCommand *cmd,int k){
    ProError e;ProFeatStatus state;ProFeattype actual,expected;ProName name;ProMassProperty mass;ProDimension thickness;double t;int bodies,*parents=NULL,n=0;
    expected=k==SH_SKETCH?PRO_FEAT_CURVE:k==SH_FLANGE?PRO_FEAT_WALL:k==SH_BEND||k==SH_HEM?PRO_FEAT_BEND:k==SH_CUT?PRO_FEAT_CUT_SMT:
        k==SH_UNBEND?PRO_FEAT_UNBEND:k==SH_REBEND?PRO_FEAT_BEND_BACK:k==SH_FORM?PRO_FEAT_SMT_SKETCH_FORM:PRO_FEAT_DATUM_SURF;
    if((e=ProFeatureStatusGet(&tx->feature,&state))||(e=ProFeatureTypeGet(&tx->feature,&actual))||(e=ProModelitemNameGet((ProModelitem *)&tx->feature,name)))return e;
    if(state!=PRO_FEAT_ACTIVE||actual!=expected||_wcsicmp(name,tx->name))return PRO_TK_GENERAL_ERROR;
    if((e=ProFeatureParentsGet(&tx->feature,&parents,&n)))return e;
    for(int i=0;i<tx->parent_count;i++){int j;for(j=0;j<n&&parents[j]!=tx->parents[i];j++){}if(j==n){if(parents)ProArrayFree((ProArray *)&parents);return PRO_TK_GENERAL_ERROR;}}
    if(parents)ProArrayFree((ProArray *)&parents);
    if(k==SH_SKETCH)return fd_verify(tx);
    if((e=sd_body_count(tx->model,&bodies))||(e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass))||
       (e=ProSmtPartThicknessGet((ProPart)tx->model,&thickness))||(e=ProDimensionValueGet(&thickness,&t)))return e;
    if(bodies!=1||!isfinite(mass.volume)||mass.volume<=0||!isfinite(t)||t<=0)return PRO_TK_BAD_CONTEXT;
    if((k==SH_FIRST||k==SH_CONVERT)&&!part_close(t,as_arg(cmd,4)*tx->scale))return PRO_TK_GENERAL_ERROR;
    if(k==SH_CONVERT&&(!tx->has_volume||!part_close(mass.volume,tx->volume)))return PRO_TK_GENERAL_ERROR;
    if(k==SH_FLANGE&&mass.volume<=tx->volume)return PRO_TK_GENERAL_ERROR;
    if(k==SH_CUT&&mass.volume>=tx->volume)return PRO_TK_GENERAL_ERROR;
    if(k==SH_FORM){ProElemId form[]={PRO_E_FEATURE_FORM},depth[]={PRO_E_SKETCH_FORM_DEPTH_PUNCH};
        if((e=mc_tree_value(&tx->feature,form,1,PRO_EXTRUDE,1))||(e=mc_tree_value(&tx->feature,depth,1,as_arg(cmd,4)*tx->scale,0)))return e;}
    return 0;
}
