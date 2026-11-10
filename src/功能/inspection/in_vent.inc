typedef struct {
    ProSurface wall,back;
    ProVector a,m,b,offset;
    double length,tolerance;
    int found;
} InspectionVentWall;

static ProError in_vent_back_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    InspectionVentWall *s=data;
    if(s->found)return PRO_TK_NO_ERROR;
    ProSurface a,b;
    ProError error=ProEdgeNeighborsGet(edge,NULL,NULL,&a,&b);
    if(error||!(a==s->back||b==s->back))return error;
    ProVector first,mid,last;
    double length;
    if((error=ProEdgeLengthEval(edge,&length))||
       (error=ProEdgeXyzdataEval(edge,0,first,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,0.5,mid,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,1,last,NULL,NULL,NULL)))return error;
    for(int i=0;i<3;i++){first[i]-=s->offset[i];mid[i]-=s->offset[i];last[i]-=s->offset[i];}
    double e=s->tolerance;
    if(fabs(length-s->length)<=e&&ad_near(mid,s->m,e)&&
        ((ad_near(first,s->a,e)&&ad_near(last,s->b,e))||(ad_near(first,s->b,e)&&ad_near(last,s->a,e))))s->found=1;
    return PRO_TK_NO_ERROR;
}

static ProError in_vent_wall_contour(ProContour contour,ProError status,ProAppData data){
    InspectionVentWall *s=data;
    return status?status:ProContourEdgeVisit(s->wall,contour,in_vent_back_edge,NULL,s);
}

typedef struct {
    const Inspection *inspect;
    InspectionPlane front,back;
    double thickness,area,gross,confirmed,other;
    int internal,through,unconfirmed,outer;
} InspectionVent;

static ProError in_vent_through(InspectionVent *s,ProEdge edge,ProEnttype type,
    const ProVector a,const ProVector b,int *through){
    *through=0;
    ProSurface adjacent[2];
    ProError error=ProEdgeNeighborsGet(edge,NULL,NULL,&adjacent[0],&adjacent[1]);
    if(error)return error;
    int wall=adjacent[0]==s->front.handle?1:adjacent[1]==s->front.handle?0:-1;
    if(wall<0)return PRO_TK_BAD_CONTEXT;
    ProSurface side=adjacent[wall];
    ProSrftype shape;
    if((error=ProSurfaceTypeGet(side,&shape)))return error;
    if(type==PRO_ENT_LINE){
        InspectionPlane face;
        if(shape!=PRO_SRF_PLANE)return PRO_TK_NO_ERROR;
        if((error=in_plane_read(s->inspect->model,side,&face)))return error;
        if(fabs(ad_dot(face.normal,s->front.normal))>1e-7)return PRO_TK_NO_ERROR;
    }else{
        ProGeomitemdata *raw=NULL;
        if(shape!=PRO_SRF_CYL)return PRO_TK_NO_ERROR;
        if((error=ProSurfaceDataGet(side,&raw)))return error;
        double dot=fabs(ad_dot(raw->data.p_surface_data->srf_shape.cylinder.e3,s->front.normal));
        ProGeomitemdataFree(&raw);
        if(dot<1-1e-7)return PRO_TK_NO_ERROR;
    }
    InspectionVentWall match={0};
    match.wall=side;match.back=s->back.handle;
    memcpy(match.a,a,sizeof(ProVector));memcpy(match.b,b,sizeof(ProVector));
    for(int i=0;i<3;i++)match.offset[i]=-s->front.normal[i]*s->thickness;
    match.tolerance=0.0002/s->inspect->mm;
    double area;
    if((error=ProEdgeLengthEval(edge,&match.length))||
       (error=ProSurfaceAreaEval(side,&area)))return error;
    if(fabs(area-match.length*s->thickness)>fmax(1e-6,area*1e-6))return PRO_TK_NO_ERROR;
    if((error=ProEdgeXyzdataEval(edge,0.5,match.m,NULL,NULL,NULL)))return error;
    error=ProSurfaceContourVisit(side,in_vent_wall_contour,NULL,&match);
    if(!error)*through=match.found;
    return error;
}

static ProError in_vent_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    InspectionVent *s=data;
    ProEnttype type;
    int orientation;
    ProVector a,b,offset_a,offset_b,cross;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(error)return error;
    if(type!=PRO_ENT_LINE&&type!=PRO_ENT_CIRCLE&&type!=PRO_ENT_ARC)return PRO_TK_INVALID_TYPE;
    if((error=ProEdgeDirGet(edge,s->front.handle,&orientation))||
       (error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)))return error;
    ad_sub(a,s->front.point,offset_a);ad_sub(b,s->front.point,offset_b);
    ad_cross(offset_a,offset_b,cross);
    double area2=ad_dot(cross,s->front.normal);
    if(type!=PRO_ENT_LINE){
        ProGeomitemdata *raw=NULL;
        if((error=ProEdgeDataGet(edge,&raw)))return error;
        const ProCurvedata *c=raw->data.p_curve_data;
        int arc=c&&c->arc.type==PRO_ENT_ARC;
        if(!c||(!arc&&c->circle.type!=PRO_ENT_CIRCLE)){
            ProGeomitemdataFree(&raw);
            return PRO_TK_INVALID_TYPE;
        }
        ProVector center,q,ra,rq;
        memcpy(center,arc?c->arc.origin:c->circle.center,sizeof(center));
        double radius=arc?c->arc.radius:c->circle.radius,length;
        ProGeomitemdataFree(&raw);
        if((error=ProEdgeLengthEval(edge,&length))||(error=ProEdgeXyzdataEval(edge,0.25,q,NULL,NULL,NULL)))return error;
        if(radius<=0||length<=0)return PRO_TK_BAD_CONTEXT;
        ad_sub(a,center,ra);ad_sub(q,center,rq);ad_cross(ra,rq,cross);
        double direction=ad_dot(cross,s->front.normal),span=length/radius;
        if(fabs(direction)<radius*radius*1e-10||span>2*AS_PI+1e-6)return PRO_TK_BAD_CONTEXT;
        area2+=copysign(radius*radius*(span-sin(span)),direction);
    }
    s->area+=orientation*area2/2;
    if(s->internal&&s->through){
        error=in_vent_through(s,edge,type,a,b,&s->through);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError in_vent_contour(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    InspectionVent *s=data;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    if(error)return error;
    if(traversal!=PRO_CONTOUR_TRAV_EXTERNAL&&traversal!=PRO_CONTOUR_TRAV_INTERNAL)return PRO_TK_BAD_CONTEXT;
    s->internal=traversal==PRO_CONTOUR_TRAV_INTERNAL;
    s->through=1;
    s->area=0;
    error=ProContourEdgeVisit(s->front.handle,contour,in_vent_edge,NULL,s);
    if(error)return error;
    double area=fabs(s->area);
    if(area<1e-10)return PRO_TK_BAD_CONTEXT;
    if(!s->internal){s->gross+=area;s->outer++;}
    else if(s->through)s->confirmed+=area;
    else {s->other+=area;s->unconfirmed++;}
    return PRO_TK_NO_ERROR;
}

static ProError in_vent(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    InspectionVent s={0};
    s.inspect=inspect;
    ProError error=in_plane(inspect->model,cmd->arg[2],&s.front);
    if(!error)error=in_plane(inspect->model,cmd->arg[3],&s.back);
    if(error)return error;
    ProVector delta;
    ad_sub(s.front.point,s.back.point,delta);
    s.thickness=ad_dot(delta,s.front.normal);
    if(ad_dot(s.front.normal,s.back.normal)>-1+1e-8||s.thickness<=0)return PRO_TK_BAD_CONTEXT;
    if((error=ProSurfaceContourVisit(s.front.handle,in_vent_contour,NULL,&s)))return error;
    double net;
    if((error=ProSurfaceAreaEval(s.front.handle,&net)))return error;
    if(s.outer!=1||!part_close(s.gross-s.confirmed-s.other,net))return PRO_TK_BAD_CONTEXT;
    double rate=100*s.confirmed/s.gross,upper=100*(s.confirmed+s.other)/s.gross,limit=as_arg(cmd,4);
    const wchar_t *status=qa_at_least(rate,limit)?L"符合":qa_at_least(upper,limit)?L"待复核":L"低于要求";
    part_text_add(out,L"散热开孔率%ls。已核实直通开口率 %.8g%%，贯穿面积 %.8g mm²，外轮廓面积 %.8g mm²，要求至少 %.8g%%。",
        status,rate,s.confirmed*inspect->mm*inspect->mm,s.gross*inspect->mm*inspect->mm,limit);
    if(s.unconfirmed)part_text_add(out,L"%d 个轮廓未计入直通面积，包含盲腔或侧壁证据不足的开口。",s.unconfirmed);
    return PRO_TK_NO_ERROR;
}
