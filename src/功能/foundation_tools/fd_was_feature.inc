static int fd_was_feature(const FdTxn *tx,int id){int i;for(i=0;i<tx->before_count;i++)if(tx->before_ids[i]==id)return 1;return 0;}
