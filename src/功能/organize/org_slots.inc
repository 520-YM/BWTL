typedef struct {ProVector a,b,center;double length,radius;int arc,id;} OrganizeSlotEdge;
typedef struct {ProVector center,axis;double length,width;int shape,edge;} OrganizeSlot;
typedef struct {
    const OrganizeView *view;
    InspectionPlane plane;
    OrganizeSlotEdge edges[24];
    OrganizeSlot slots[128];
    double minimum;
    int count,skip,found,unknown,limited;
} OrganizeSlots;

static ProError org_slot_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    OrganizeSlots *s=data;
    if(s->skip)return PRO_TK_NO_ERROR;
    ProEnttype type;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(error)return error;
    if(s->count==_countof(s->edges)||(type!=PRO_ENT_LINE&&type!=PRO_ENT_ARC)){
        s->skip=1;
        return PRO_TK_NO_ERROR;
    }
    OrganizeSlotEdge *p=&s->edges[s->count++];
    memset(p,0,sizeof(*p));
    if((error=ProEdgeIdGet(edge,&p->id))||
       (error=ProEdgeXyzdataEval(edge,0,p->a,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,1,p->b,NULL,NULL,NULL))||
       (error=ProEdgeLengthEval(edge,&p->length)))return error;
    p->arc=type==PRO_ENT_ARC;
    if(p->arc){
        ProGeomitemdata *raw=NULL;
        if((error=ProEdgeDataGet(edge,&raw)))return error;
        const ProCurvedata *curve=raw->data.p_curve_data;
        if(!curve||curve->arc.type!=PRO_ENT_ARC)s->skip=1;
        else{
            p->radius=curve->arc.radius;
            memcpy(p->center,curve->arc.origin,sizeof(ProVector));
        }
        ProGeomitemdataFree(&raw);
    }
    return PRO_TK_NO_ERROR;
}

static int org_slot_rectangle(OrganizeSlotEdge *const lines[4],OrganizeSlot *slot,double tolerance){
    for(int i=0;i<4;i++){
        ProVector a,b;
        ad_sub(lines[i]->b,lines[i]->a,a);ad_sub(lines[(i+1)%4]->b,lines[(i+1)%4]->a,b);
        if(!ad_unit(a)||!ad_unit(b)||fabs(ad_dot(a,b))>1e-7||
           fabs(lines[i]->length-lines[(i+2)%4]->length)>tolerance)return 0;
    }
    int longest=lines[0]->length>=lines[1]->length?0:1;
    slot->length=lines[longest]->length;
    slot->width=lines[1-longest]->length;
    ad_sub(lines[longest]->b,lines[longest]->a,slot->axis);
    if(!ad_unit(slot->axis))return 0;
    for(int i=0;i<3;i++)slot->center[i]=(lines[0]->a[i]+lines[0]->b[i]+lines[2]->a[i]+lines[2]->b[i])/4;
    return 1;
}

static int org_slot_shape(OrganizeSlots *s,OrganizeSlot *slot){
    OrganizeSlotEdge *lines[4],ends[2]={0};
    int n=0,m=0;
    double tolerance=0.0003/s->view->mm;
    for(int i=0;i<s->count;i++){
        OrganizeSlotEdge *e=&s->edges[i];
        if(!e->arc){
            if(n==_countof(lines))return 0;
            lines[n++]=e;
            continue;
        }
        int j=0;
        while(j<m&&!ad_near(ends[j].center,e->center,tolerance))j++;
        if(j==m){
            if(m==_countof(ends))return 0;
            ends[m++]=*e;
        }else{
            if(fabs(ends[j].radius-e->radius)>tolerance)return 0;
            ends[j].length+=e->length;
        }
    }
    slot->edge=s->edges[0].id;
    if(n==4&&!m)return org_slot_rectangle(lines,slot,tolerance);
    if(n!=2||m!=2||ends[0].radius<=0||fabs(ends[0].radius-ends[1].radius)>tolerance||
       fabs(ends[0].length-AS_PI*ends[0].radius)>tolerance||fabs(ends[1].length-AS_PI*ends[1].radius)>tolerance)return 0;
    double span=cm_distance(ends[0].center,ends[1].center);
    ad_sub(ends[1].center,ends[0].center,slot->axis);
    if(span<=tolerance||!ad_unit(slot->axis))return 0;
    for(int i=0;i<2;i++){
        ProVector line;
        ad_sub(lines[i]->b,lines[i]->a,line);
        if(!ad_unit(line)||fabs(ad_dot(slot->axis,line))<1-1e-7||fabs(lines[i]->length-span)>tolerance)return 0;
        for(int j=0;j<2;j++){
            const double *point=j?lines[i]->b:lines[i]->a;
            if(fabs(cm_distance(point,ends[0].center)-ends[0].radius)>tolerance&&
               fabs(cm_distance(point,ends[1].center)-ends[1].radius)>tolerance)return 0;
        }
    }
    slot->width=2*ends[0].radius;slot->length=span+slot->width;
    for(int i=0;i<3;i++)slot->center[i]=(ends[0].center[i]+ends[1].center[i])/2;
    return 2;
}

static ProError org_slot_contour(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    OrganizeSlots *s=data;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    if(error||traversal!=PRO_CONTOUR_TRAV_INTERNAL)return error;
    s->count=s->skip=0;
    error=ProContourEdgeVisit(s->plane.handle,contour,org_slot_edge,NULL,s);
    if(error)return error;
    OrganizeSlot slot={0};
    slot.shape=s->skip||s->count<4?0:org_slot_shape(s,&slot);
    if(!slot.shape){s->unknown++;return PRO_TK_NO_ERROR;}
    if(slot.width<=0||slot.length/slot.width<s->minimum)return PRO_TK_NO_ERROR;
    if(s->found==_countof(s->slots))s->limited=1;
    else s->slots[s->found++]=slot;
    return PRO_TK_NO_ERROR;
}

static int org_slot_void(const OrganizeView *view,const InspectionPlane *plane,const OrganizeSlot *slot,double thickness){
    ProVector side;
    ad_cross(plane->normal,slot->axis,side);
    if(!ad_unit(side))return 0;
    double end=slot->shape==1?0.4*slot->length:0.5*slot->length-0.2*slot->width;
    double tolerance=0.0002/view->mm;
    for(int i=-1;i<=1;i++)for(int j=-1;j<=1;j++){
        ProRay ray;
        QualityRay hits;
        for(int k=0;k<3;k++){
            ray.dir_vector[k]=-plane->normal[k];
            ray.start_point[k]=slot->center[k]+i*end*slot->axis[k]+j*0.3*slot->width*side[k];
        }
        if(qa_ray(view->model,&ray,tolerance,&hits))return 0;
        for(int k=0;k<hits.count;k+=2)
            if(fmin(thickness,hits.crossing[k+1].depth)-fmax(0,hits.crossing[k].depth)>tolerance)return 0;
    }
    return 1;
}

static ProError org_slots(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    ProFeature feature,owner;
    ProFeattype type;
    ProGeomitem item;
    ProError error=org_feature(view->model,cmd->arg[2],&feature);
    if(!error)error=ProFeatureTypeGet(&feature,&type);
    if(error||type!=PRO_FEAT_IMPORT)return error?error:PRO_TK_BAD_CONTEXT;
    OrganizeSlots face[2]={0};
    if((error=qa_single_body(view->model,NULL)))return error;
    for(int i=0;i<2;i++){
        face[i].view=view;face[i].minimum=as_arg(cmd,5);
        if((error=in_plane(view->model,cmd->arg[3+i],&face[i].plane))||
           (error=ProSurfaceToGeomitem((ProSolid)view->model,face[i].plane.handle,&item))||
           (error=ProGeomitemFeatureGet(&item,&owner)))return error;
        if(owner.id!=feature.id)return PRO_TK_BAD_CONTEXT;
    }
    ProVector delta;
    ad_sub(face[0].plane.point,face[1].plane.point,delta);
    double thickness=ad_dot(delta,face[0].plane.normal),mm=view->mm,epsilon=0.0003/mm;
    if(ad_dot(face[0].plane.normal,face[1].plane.normal)>-1+1e-8||thickness<=0)return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<2;i++)
        if((error=ProSurfaceContourVisit(face[i].plane.handle,org_slot_contour,NULL,&face[i])))return error;
    int found=0,unconfirmed=face[0].unknown;
    for(int i=0;i<face[0].found;i++){
        const OrganizeSlot *a=&face[0].slots[i];
        ProVector opposite;
        cm_point(a->center,face[0].plane.normal,-thickness,opposite);
        int matched=0;
        for(int j=0;j<face[1].found&&!matched;j++){
            const OrganizeSlot *b=&face[1].slots[j];
            matched=a->shape==b->shape&&fabs(a->length-b->length)<=epsilon&&fabs(a->width-b->width)<=epsilon&&
                fabs(ad_dot(a->axis,b->axis))>1-1e-7&&ad_near(opposite,b->center,epsilon);
        }
        if(!matched||!org_slot_void(view,&face[0].plane,a,thickness)){unconfirmed++;continue;}
        found++;
        part_text_add(out,L"EDGE:%d｜%ls贯穿槽｜%.8g × %.8g mm｜中心 (%.8g, %.8g, %.8g) mm\n",
            a->edge,a->shape==1?L"矩形":L"圆端",a->length*mm,a->width*mm,a->center[0]*mm,a->center[1]*mm,a->center[2]*mm);
    }
    part_text_add(out,L"识别到 %d 个贯穿槽。按两面轮廓匹配及九点射线核对。%d 个内轮廓未核实，坐标为模型默认 XYZ。%ls",
        found,unconfirmed,face[0].limited||face[1].limited?L"已达到轮廓扫描上限。":L"");
    return PRO_TK_NO_ERROR;
}
