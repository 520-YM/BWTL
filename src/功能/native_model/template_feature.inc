static ProError template_feature(ProFeature *feature, ProError status, ProAppData data) {
    ProFeattype type;
    TemplateDatums *d = (TemplateDatums *)data;
    if (status != PRO_TK_NO_ERROR) return status;
    if (ProFeatureTypeGet(feature, &type) != PRO_TK_NO_ERROR) return PRO_TK_GENERAL_ERROR;
    d->total++; if(type==PRO_FEAT_COMPONENT)d->components++;
    if (type == PRO_FEAT_CSYS) d->csys++;
    if (type == PRO_FEAT_DATUM) return ProFeatureGeomitemVisit(feature, PRO_SURFACE, template_surface, NULL, data);
    return PRO_TK_NO_ERROR;
}
