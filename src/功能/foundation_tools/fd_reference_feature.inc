static ProError fd_reference_feature(ProFeature *feature,ProError status,ProAppData data){
    ProType types[6]={PRO_SURFACE,PRO_AXIS,PRO_POINT,PRO_CSYS,PRO_CURVE,PRO_EDGE};int i;ProError e;ProFeatStatus state;
    if(status)return status;if(ProFeatureStatusGet(feature,&state)||state!=PRO_FEAT_ACTIVE)return 0;
    for(i=0;i<6;i++){e=ProFeatureGeomitemVisit(feature,types[i],fd_reference_item,NULL,data);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;}return 0;
}
