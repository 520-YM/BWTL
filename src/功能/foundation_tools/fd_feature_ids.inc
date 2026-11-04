static ProError fd_feature_ids(ProFeature *f,ProError status,ProAppData data){
    FdTxn *tx=data;if(status)return status;if(tx->before_count>=512)return PRO_TK_BAD_CONTEXT;
    tx->before_ids[tx->before_count++]=f->id;return 0;
}
