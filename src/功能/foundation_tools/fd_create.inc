static ProError fd_create(FdTxn *tx,int incomplete){
    ProError e;tx->stage="create native feature";tx->options[0]=incomplete?PRO_FEAT_CR_INCOMPLETE_FEAT:PRO_FEAT_CR_NO_OPTS;
    e=ProFeatureWithoptionsCreate(tx->model_selection,tx->root,tx->options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx->feature,&tx->errors);
    if(tx->feature.owner==tx->model&&tx->feature.id>0&&!fd_was_feature(tx,tx->feature.id))tx->created=1;
    return e;
}
