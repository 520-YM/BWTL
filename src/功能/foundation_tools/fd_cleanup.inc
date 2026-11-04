static void fd_cleanup(FdTxn *tx,ProError error){
    int i;
    if(error&&(tx->created||tx->root)&&tx->before_count){
        FdTxn after={0};
        if(tx->created&&(model_remove_feature((ProSolid)tx->model,tx->feature.id)||part_regenerate(tx->model)))tx->rollback_ok=0;
        if(ProSolidFeatVisit((ProSolid)tx->model,fd_feature_ids,NULL,&after)||after.before_count!=tx->before_count)tx->rollback_ok=0;
        else for(i=0;i<after.before_count;i++)if(!fd_was_feature(tx,after.before_ids[i]))tx->rollback_ok=0;
    }
    if(tx->root)ProElementFree(&tx->root);
    for(i=0;i<tx->ref_count;i++)ProReferenceFree(tx->refs[i]);
    if(tx->model_selection)ProSelectionFree(&tx->model_selection);
    if(tx->options)ProArrayFree((ProArray *)&tx->options);
}
