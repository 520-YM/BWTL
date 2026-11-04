static ProError part_geometry_visit(ProGeomitem *item,ProError status,ProAppData data){
    PartSnapshot *s=data;PartGeometry *g;int i,n;
    if(status)return status;
    if(item->type==PRO_EDGE){
        ProEdge edge;ProEnttype type;
        n=s->edges;for(i=0;i<n;i++)if(s->edge[i].id==item->id)return 0;
        if(n==PART_GEOM_MAX){s->truncated=1;return 0;}g=&s->edge[n];
        if(ProGeomitemToEdge(item,&edge)||ProEdgeTypeGet(edge,&type)||ProEdgeLengthEval(edge,&g->size)){s->read_errors++;return 0;}
        g->id=item->id;g->type=type;s->edges++;
    }else if(item->type==PRO_SURFACE){
        ProSurface surface;ProSrftype type;
        n=s->surfaces;for(i=0;i<n;i++)if(s->surface[i].id==item->id)return 0;
        if(n==PART_GEOM_MAX){s->truncated=1;return 0;}g=&s->surface[n];
        if(ProGeomitemToSurface(item,&surface)||ProSurfaceTypeGet(surface,&type)||ProSurfaceAreaEval(surface,&g->size)){s->read_errors++;return 0;}
        g->id=item->id;g->type=type;s->surfaces++;
    }return 0;
}
