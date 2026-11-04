static ProError fd_feature_geom(ProFeature *feature,ProType type,ProGeomitem *item){
    FdCollect c={0};ProError e=ProFeatureGeomitemVisit(feature,type,fd_collect_geom,NULL,&c);
    if(e)return e;if(c.count!=1)return c.count?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;*item=c.item;return 0;
}
