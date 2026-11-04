static ProError part_feature_visit(ProFeature *f,ProError status,ProAppData data){
    PartSnapshot *s=data;PartFeature *p;ProError e;
    if(status)return status;
    if(s->features==PART_FEATURE_MAX){s->truncated=1;return 0;}
    p=&s->feature[s->features];p->id=f->id;
    if(ProFeatureTypeGet(f,&p->type)||ProFeatureStatusGet(f,&p->status)||ProFeatureStatusflagsGet(f,&p->flags)){s->read_errors++;return 0;}
    ProModelitemNameGet((ProModelitem *)f,p->name);s->features++;
    if(p->flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED))s->unhealthy=1;
    if(p->status==PRO_FEAT_ACTIVE){
        e=ProFeatureGeomitemVisit(f,PRO_EDGE,part_geometry_visit,NULL,s);if(e && e!=PRO_TK_E_NOT_FOUND)s->read_errors++;
        if(p->type!=PRO_FEAT_DATUM){e=ProFeatureGeomitemVisit(f,PRO_SURFACE,part_geometry_visit,NULL,s);if(e&&e!=PRO_TK_E_NOT_FOUND)s->read_errors++;}
    }return 0;
}
