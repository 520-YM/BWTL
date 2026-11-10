typedef struct {ProSurface face;int edge,found;} VentDrainEdge;

static ProError vt_drain_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    VentDrainEdge *check=data;
    int id;
    ProError error=ProEdgeIdGet(edge,&id);
    if(!error&&id==check->edge)check->found=1;
    return error;
}

static ProError vt_drain_contour(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    VentDrainEdge *check=data;
    ProContourTraversal side;
    ProError error=ProContourTraversalGet(contour,&side);
    return error?error:side==PRO_CONTOUR_TRAV_EXTERNAL?
        ProContourEdgeVisit(check->face,contour,vt_drain_edge,NULL,check):PRO_TK_NO_ERROR;
}

static ProError vt_drain(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,int sheet,PartText *out){
    VentDrainEdge check={0};
    part_integer(cmd->arg[9]+5,&check.edge);
    ProEdge edge;
    ProEnttype type;
    ProVector p,q;
    ProError error=ProSurfaceInit((ProSolid)tx->model,frame->face_id,&check.face);
    if(!error)error=ProSurfaceContourVisit(check.face,vt_drain_contour,NULL,&check);
    if(error||!check.found)return error?error:PRO_TK_BAD_CONTEXT;
    if((error=ProEdgeInit((ProSolid)tx->model,check.edge,&edge))||(error=ProEdgeTypeGet(edge,&type)))return error;
    if(type!=PRO_ENT_LINE)return PRO_TK_BAD_CONTEXT;
    if((error=ProEdgeXyzdataEval(edge,0,p,NULL,NULL,NULL))||(error=ProEdgeXyzdataEval(edge,1,q,NULL,NULL,NULL)))return error;
    AsPoint a=fx_local_xy(frame,p,tx->scale),b=fx_local_xy(frame,q,tx->scale);
    double x=as_arg(cmd,6),gap=as_arg(cmd,7),length=as_arg(cmd,10),width=as_arg(cmd,11),t=as_arg(cmd,8);
    if(fabs(a.y-b.y)>0.0002||x-length/2<=fmin(a.x,b.x)+0.1||x+length/2>=fmax(a.x,b.x)-0.1)
        return PRO_TK_BAD_CONTEXT;
    double y=(a.y+b.y)/2,step=fmin(gap/2,0.1);
    if((error=pn_probe(tx,frame,as_point(x,y-step),0,0,0))||
       (error=pn_probe(tx,frame,as_point(x,y+step),0,t,1)))return error;
    PanelProfile profile={PN_CAPSULE,x,y+gap+width/2,length,width,0,0};
    if((error=vt_through(tx,cmd,frame,&profile,1,sheet,out)))return error;
    part_text_add(out,L"排水槽距所选底边 %.8g mm，沿局部 X。",gap);
    return PRO_TK_NO_ERROR;
}
