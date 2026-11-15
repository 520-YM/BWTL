#define SH_DXF_MAX 2048
typedef struct {ProEdge edge;int id,seen,hole;} ShDxfEdge;
typedef struct {double a[2],b[2],width;int surface,direction;} ShDxfBend;
typedef struct {
    ProMdl model;ProSurface surface;double scale;ProVector origin,u,v,normal;
    ShDxfEdge edges[SH_DXF_MAX];int count,faces,hole,face_edge_count;ProEdge face_edges[64];
    ShDxfBend bends[256];int bend_count;
} ShDxf;
static ProError sh_project(ShDxf *s,const double *world,double *xy){
    ProVector delta;ad_sub(world,s->origin,delta);
    if(fabs(ad_dot(delta,s->normal))>0.002*s->scale)return PRO_TK_BAD_CONTEXT;
    xy[0]=ad_dot(delta,s->u)/s->scale;xy[1]=ad_dot(delta,s->v)/s->scale;return 0;
}
static ProError sh_dxf_edge(ProEdge edge,ProError status,ProAppData data){
    ShDxf *s=data;int id,i;ProError e;if(status)return status;if((e=ProEdgeIdGet(edge,&id)))return e;
    if(s->face_edge_count>=64)return PRO_TK_OUT_OF_RANGE;s->face_edges[s->face_edge_count++]=edge;
    for(i=0;i<s->count&&s->edges[i].id!=id;i++){}
    if(i==s->count){if(s->count>=SH_DXF_MAX)return PRO_TK_OUT_OF_RANGE;s->count++;s->edges[i].edge=edge;s->edges[i].id=id;s->edges[i].hole=s->hole;}
    if(++s->edges[i].seen>2)return PRO_TK_BAD_CONTEXT;return 0;
}
static ProError sh_dxf_contour(ProContour contour,ProError status,ProAppData data){
    ShDxf *s=data;ProContourTraversal traversal;ProError e;if(status)return status;
    if((e=ProContourTraversalGet(contour,&traversal)))return e;s->hole=traversal==PRO_CONTOUR_TRAV_INTERNAL;
    return ProContourEdgeVisit(s->surface,contour,sh_dxf_edge,NULL,s);
}
static ProError sh_dxf_surface(ProSurface surface,ProError status,ProAppData data){
    ShDxf *s=data;ProSmtSurfType side;ProGeomitem item;ProVector p,n;ProError e;double radius,angle,developed;ProBool inside;
    if(status)return status;if((e=ProSmtSurfaceTypeGet((ProPart)s->model,surface,&side)))return e;if(side!=PRO_SMT_SURF_FACE)return 0;
    if((e=ProSurfaceToGeomitem((ProSolid)s->model,surface,&item))||(e=fd_plane_data(&item,p,n)))return e;
    if(fabs(fabs(ad_dot(n,s->normal))-1)>1e-6)return PRO_TK_BAD_CONTEXT;
    {double xy[2];if((e=sh_project(s,p,xy)))return e;}
    s->surface=surface;s->face_edge_count=0;s->faces++;
    if((e=ProSurfaceContourVisit(surface,sh_dxf_contour,NULL,s)))return e;
    if(!ProSmtBendsrfInfoGet((ProPart)s->model,surface,&radius,&inside,&angle,&developed,NULL)&&fabs(angle)>.01){
        /* Rectangular unbent bend regions give an exact centre line, not a
           tangent boundary falsely labelled as the bend line. */
        Pro3dPnt a[4],b[4],mid={0},axis={0};double lengths[4],span=0;int chosen=-1;
        if(s->face_edge_count!=4||s->bend_count>=256)return PRO_TK_BAD_CONTEXT;
        for(int i=0;i<4;i++){
            ProEnttype type;ProVector d1,d2,dir,delta;
            if((e=ProEdgeTypeGet(s->face_edges[i],&type))||type!=PRO_ENT_LINE)return PRO_TK_BAD_CONTEXT;
            if((e=ProEdgeXyzdataEval(s->face_edges[i],0,a[i],d1,d2,dir))||(e=ProEdgeXyzdataEval(s->face_edges[i],1,b[i],d1,d2,dir)))return e;
            ad_sub(b[i],a[i],delta);lengths[i]=sqrt(ad_dot(delta,delta));for(int j=0;j<3;j++)mid[j]+=(a[i][j]+b[i][j])/8;
            if(fabs(lengths[i]-developed)>fmax(0.001*s->scale,developed*1e-5)){chosen=i;span=lengths[i];}
        }
        if(chosen<0)return PRO_TK_BAD_CONTEXT;ad_sub(b[chosen],a[chosen],axis);if(!ad_unit(axis))return PRO_TK_BAD_CONTEXT;
        {ProVector start,end;double area;for(int j=0;j<3;j++){start[j]=mid[j]-axis[j]*span/2;end[j]=mid[j]+axis[j]*span/2;}
         if((e=ProSurfaceAreaEval(surface,&area))||fabs(area-span*developed)>fmax(.01*s->scale*s->scale,area*1e-5))return PRO_TK_BAD_CONTEXT;
         ShDxfBend *bend=&s->bends[s->bend_count];
         if((e=sh_project(s,start,bend->a))||(e=sh_project(s,end,bend->b)))return e;
         bend->width=developed/s->scale;bend->surface=item.id;
         s->bend_count++;}
    }
    return 0;
}
static double sh_degrees(double x){x=fmod(x*180/AS_PI,360);return x<0?x+360:x;}
static void sh_dxf_line(FILE *f,const char *layer,const double *a,const double *b){
    fprintf(f,"0\nLINE\n8\n%s\n10\n%.15g\n20\n%.15g\n30\n0\n11\n%.15g\n21\n%.15g\n31\n0\n",layer,a[0],a[1],b[0],b[1]);
}
static ProError sh_dxf_write_edge(FILE *f,ShDxf *s,ShDxfEdge *entry){
    ProEnttype type;ProGeomitemdata *raw=NULL;ProVector p,q,m,d1,d2,dir;double a[2],b[2],mid[2],c[2],radius,start,end,center[3];ProError e;
    const char *layer=entry->hole?"HOLES":"OUTLINE";
    if((e=ProEdgeTypeGet(entry->edge,&type))||(e=ProEdgeXyzdataEval(entry->edge,0,p,d1,d2,dir))||
       (e=ProEdgeXyzdataEval(entry->edge,1,q,d1,d2,dir))||(e=sh_project(s,p,a))||(e=sh_project(s,q,b)))return e;
    if(type==PRO_ENT_LINE){sh_dxf_line(f,layer,a,b);return 0;}
    if(type!=PRO_ENT_ARC&&type!=PRO_ENT_CIRCLE)return PRO_TK_INVALID_TYPE;
    if((e=ProEdgeDataGet(entry->edge,&raw)))return e;
    if(type==PRO_ENT_ARC){ProArcdata *arc=&raw->data.p_curve_data->arc;for(int j=0;j<3;j++)center[j]=arc->origin[j];radius=arc->radius/s->scale;}
    else {ProCircledata *circle=&raw->data.p_curve_data->circle;for(int j=0;j<3;j++)center[j]=circle->center[j];radius=circle->radius/s->scale;}
    e=sh_project(s,center,c);if(e)goto done;
    if(type==PRO_ENT_CIRCLE||hypot(a[0]-b[0],a[1]-b[1])<1e-7){fprintf(f,"0\nCIRCLE\n8\n%s\n10\n%.15g\n20\n%.15g\n30\n0\n40\n%.15g\n",layer,c[0],c[1],radius);goto done;}
    if((e=ProEdgeXyzdataEval(entry->edge,.5,m,d1,d2,dir))||(e=sh_project(s,m,mid)))goto done;
    start=sh_degrees(atan2(a[1]-c[1],a[0]-c[0]));end=sh_degrees(atan2(b[1]-c[1],b[0]-c[0]));
    {double middle=sh_degrees(atan2(mid[1]-c[1],mid[0]-c[0]));if(fmod(middle-start+360,360)>fmod(end-start+360,360)){double swap=start;start=end;end=swap;}}
    fprintf(f,"0\nARC\n8\n%s\n10\n%.15g\n20\n%.15g\n30\n0\n40\n%.15g\n50\n%.15g\n51\n%.15g\n",layer,c[0],c[1],radius,start,end);
done:if(raw)ProGeomitemdataFree(&raw);return e;
}
