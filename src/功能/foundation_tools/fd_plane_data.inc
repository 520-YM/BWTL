static ProError fd_plane_data(ProGeomitem *item,ProVector point,ProVector normal){
    ProSurface surface;ProSrftype type;ProUvParam uv={0,0};ProVector first[2],second[3];ProError e;
    if((e=ProGeomitemToSurface(item,&surface))||(e=ProSurfaceTypeGet(surface,&type)))return e;
    if(type!=PRO_SRF_PLANE)return PRO_TK_INVALID_TYPE;
    return ProSurfaceXyzdataEval(surface,uv,point,first,second,normal);
}
