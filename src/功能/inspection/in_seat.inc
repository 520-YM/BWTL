typedef struct {ShopMouth *mouth;double scale,nearest;} InspectionSeat;

static ProError in_seat_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    InspectionSeat *s=data;
    int id;
    ProEnttype type;
    ProVector a,b;
    ProError error=ProEdgeIdGet(edge,&id);
    if(error)return error;
    for(int i=0;i<s->mouth->ring.count;i++)if(s->mouth->ring.ids[i]==id)return PRO_TK_NO_ERROR;
    if((error=ProEdgeTypeGet(edge,&type))||(error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)))return error;
    PanelFrame *frame=&s->mouth->frame;
    double distance;
    if(type==PRO_ENT_LINE)
        distance=fx_segment_distance(as_point(0,0),fx_local_xy(frame,a,s->scale),fx_local_xy(frame,b,s->scale));
    else if(type==PRO_ENT_CIRCLE||type==PRO_ENT_ARC){
        const PanelProfile center={0};
        if((error=fx_arc_distance(edge,frame,s->scale,a,b,&center,&distance)))return error;
    }else return PRO_TK_INVALID_TYPE;
    s->nearest=fmin(s->nearest,distance);
    return PRO_TK_NO_ERROR;
}

static ProError in_seat_contour(ProContour contour,ProError status,ProAppData data){
    InspectionSeat *s=data;
    return status?status:ProContourEdgeVisit(s->mouth->ring.face,contour,in_seat_edge,NULL,s);
}

static ProError in_seat(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    ShopMouth mouth={0};
    ProError error=in_mouth(inspect,cmd->arg[2],cmd->arg[3],&mouth);
    if(error)return error;
    InspectionSeat s={&mouth,1/inspect->mm,INFINITY};
    if((error=ProSurfaceContourVisit(mouth.ring.face,in_seat_contour,NULL,&s)))return error;
    if(!isfinite(s.nearest))return PRO_TK_BAD_CONTEXT;
    double head=as_arg(cmd,4)/2,shank=as_arg(cmd,5),minimum=as_arg(cmd,6),bore=2*mouth.radius;
    double ring=head-mouth.radius,edge=s.nearest-head;
    int ok=ring>0.0001&&qa_at_least(ring,minimum)&&edge>0.0001&&qa_at_least(bore,shank);
    part_text_add(out,L"孔口螺钉头落位%ls。孔径 %.8g mm，杆部直径余量 %.8g mm，环形支承宽 %.8g mm，头缘到其它面边界净距 %.8g mm。",
        ok?L"符合":L"不符合",bore,bore-shank,ring,edge);
    return PRO_TK_NO_ERROR;
}
