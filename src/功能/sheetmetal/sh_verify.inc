static ProError sh_verify(FdTxn *tx,const PartCommand *cmd,int k){
    ProError e;ProFeatStatus state;ProFeattype actual,expected;ProName name;ProMassProperty mass;ProDimension thickness;double t;int bodies,*parents=NULL,n=0;
    expected=k==SH_SKETCH?PRO_FEAT_CURVE:k==SH_FLANGE?PRO_FEAT_WALL:k==SH_BEND||k==SH_HEM?PRO_FEAT_BEND:k==SH_CUT?PRO_FEAT_CUT_SMT:
        k==SH_UNBEND?PRO_FEAT_UNBEND:k==SH_REBEND?PRO_FEAT_BEND_BACK:k==SH_FORM?PRO_FEAT_SMT_SKETCH_FORM:PRO_FEAT_DATUM_SURF;
    e=ProFeatureStatusGet(&tx->feature,&state);
    if(e){trace("sheet verification read feature status",e);return e;}
    e=ProFeatureTypeGet(&tx->feature,&actual);
    if(e){trace("sheet verification read feature type",e);return e;}
    if(state!=PRO_FEAT_ACTIVE||actual!=expected)return PRO_TK_GENERAL_ERROR;
    e=ProModelitemNameGet((ProModelitem *)&tx->feature,name);
    if(e==PRO_TK_E_NOT_FOUND&&k==SH_FLANGE&&tx->created){
        /* Redefining a stock wall profile can discard its initial name. */
        e=ProModelitemNameSet((ProModelitem *)&tx->feature,tx->name);
        if(!e)e=ProModelitemNameGet((ProModelitem *)&tx->feature,name);
    }
    if(e){trace("sheet verification read feature name",e);return e;}
    if(_wcsicmp(name,tx->name))return PRO_TK_GENERAL_ERROR;
    if((e=ProFeatureParentsGet(&tx->feature,&parents,&n))){trace("sheet verification read feature parents",e);return e;}
    for(int i=0;i<tx->parent_count;i++){int j;for(j=0;j<n&&parents[j]!=tx->parents[i];j++){}if(j==n){if(parents)ProArrayFree((ProArray *)&parents);return PRO_TK_GENERAL_ERROR;}}
    if(parents)ProArrayFree((ProArray *)&parents);
    if(k==SH_SKETCH)return fd_verify(tx);
    e=sd_body_count(tx->model,&bodies);
    if(e){trace("sheet verification read body count",e);return e;}
    e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);
    if(e){trace("sheet verification read volume",e);return e;}
    e=ProSmtPartThicknessGet((ProPart)tx->model,&thickness);
    if(e){trace("sheet verification locate part thickness",e);return e;}
    e=ProDimensionValueGet(&thickness,&t);
    if(e){trace("sheet verification read thickness value",e);return e;}
    if(bodies!=1||!isfinite(mass.volume)||mass.volume<=0||!isfinite(t)||t<=0)return PRO_TK_BAD_CONTEXT;
    if((k==SH_FIRST||k==SH_CONVERT)&&!part_close(t,as_arg(cmd,4)*tx->scale))return PRO_TK_GENERAL_ERROR;
    if(k==SH_CONVERT&&(!tx->has_volume||!part_close(mass.volume,tx->volume)))return PRO_TK_GENERAL_ERROR;
    if(k==SH_FLANGE&&mass.volume<=tx->volume)return PRO_TK_GENERAL_ERROR;
    if(k==SH_CUT&&mass.volume>=tx->volume)return PRO_TK_GENERAL_ERROR;
    if(k==SH_FORM){ProElemId form[]={PRO_E_FEATURE_FORM},depth[]={PRO_E_SKETCH_FORM_DEPTH_PUNCH};
        if((e=mc_tree_value(&tx->feature,form,1,PRO_EXTRUDE,1))||(e=mc_tree_value(&tx->feature,depth,1,as_arg(cmd,4)*tx->scale,0)))return e;}
    return 0;
}
