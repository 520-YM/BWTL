static ProError fd_collect_geom(ProGeomitem *item,ProError status,ProAppData data){
    FdCollect *c=data;if(status)return status;c->item=*item;c->count++;return 0;
}
