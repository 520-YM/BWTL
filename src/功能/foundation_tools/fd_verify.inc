static ProError fd_verify(FdTxn *tx){
    ProError e;ProFeatStatus status;ProName name;ProMassProperty mass;int *parents=NULL,count=0,i,j;
    tx->stage="regenerate and verify native feature";
    if((e=part_regenerate(tx->model))||(e=ProFeatureStatusGet(&tx->feature,&status))||(e=ProModelitemNameGet((ProModelitem *)&tx->feature,name)))return e;
    if(status!=PRO_FEAT_ACTIVE||wcscmp(name,tx->name))return PRO_TK_GENERAL_ERROR;
    if((e=ProFeatureParentsGet(&tx->feature,&parents,&count)))return e;
    for(i=0;i<tx->parent_count;i++){
        for(j=0;j<count;j++)if(parents[j]==tx->parents[i])break;
        if(j==count){if(parents)ProArrayFree((ProArray *)&parents);return PRO_TK_GENERAL_ERROR;}
    }if(parents)ProArrayFree((ProArray *)&parents);
    if(tx->was_empty){if(template_empty((ProSolid)tx->model))return PRO_TK_GENERAL_ERROR;}
    else if(tx->has_volume){
        if((e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)))return e;
        if(!part_close(mass.volume,tx->volume))return PRO_TK_GENERAL_ERROR;
    }return 0;
}
