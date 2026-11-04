static ProError template_surface(ProGeomitem *item, ProError status, ProAppData data) {
    TemplateDatums *d = (TemplateDatums *)data;
    ProSurface surface; ProSrftype type;
    ProUvParam uv = {0,0}; ProVector point, first[2], second[3], normal;
    if (status != PRO_TK_NO_ERROR) return status;
    if (d->planes >= 32) return PRO_TK_NO_ERROR;
    if (ProGeomitemToSurface(item, &surface) != PRO_TK_NO_ERROR) return PRO_TK_GENERAL_ERROR;
    if (ProSurfaceTypeGet(surface, &type) != PRO_TK_NO_ERROR || type != PRO_SRF_PLANE) return PRO_TK_NO_ERROR;
    if (ProSurfaceXyzdataEval(surface, uv, point, first, second, normal) != PRO_TK_NO_ERROR) return PRO_TK_GENERAL_ERROR;
    d->plane[d->planes] = *item;
    memcpy(d->normal[d->planes], normal, sizeof(ProVector)); d->planes++;
    return PRO_TK_NO_ERROR;
}
