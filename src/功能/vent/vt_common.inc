enum {VT_MAX_OPENINGS=64};

static ProError vt_through(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,const PanelProfile *profiles,
    int count,int sheet,PartText *out){
    double t=as_arg(cmd,8),area=0;
    ProName sketch;
    ProMatrix matrix;
    ProError error;
    for(int i=0;i<count;i++){
        if((error=pn_profile_probes(tx,frame,&profiles[i],t,0)))return error;
        area+=pn_area(&profiles[i]);
    }
    swprintf_s(sketch,PRO_NAME_SIZE,L"%ls_SK",tx->name);
    if((error=pn_sketch(tx->model,cmd,frame,sketch,profiles,count,sheet,matrix))||
       (error=pn_cut(tx,frame,tx->name,sketch,matrix,t,sheet))||
       (error=md_expected_volume(tx,-area*t)))return error;
    for(int i=0;i<count;i++)if((error=pn_profile_probes(tx,frame,&profiles[i],t,1)))return error;
    part_text_add(out,L"已核对 %d 个贯穿轮廓，开孔总面积 %.8g mm²，实际板厚 %.8g mm。",count,area,t);
    return PRO_TK_NO_ERROR;
}

static ProError vt_pattern(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,int sheet,
    enum PanelShape shape,PartText *out){
    PanelProfile profiles[VT_MAX_OPENINGS];
    double x=as_arg(cmd,6),y=as_arg(cmd,7),dx=as_arg(cmd,11),dy=as_arg(cmd,12),w=as_arg(cmd,13);
    double h=shape==PN_CIRCLE?w:as_arg(cmd,14);
    int rows=(int)as_arg(cmd,9),cols=(int)as_arg(cmd,10),n=0;
    int stagger=shape==PN_CIRCLE&&rows>1&&!wcscmp(cmd->arg[14],L"STAGGER");
    double web=INFINITY;
    if(cols>1)web=dx-w;
    if(rows>1){
        if(stagger){
            web=fmin(web,hypot(dx/2,dy)-w);
            if(rows>2)web=fmin(web,2*dy-w);
        }else web=fmin(web,dy-h);
    }
    if(web<0.1)return PRO_TK_BAD_INPUTS;
    for(int row=0;row<rows;row++)for(int col=0;col<cols;col++)
        profiles[n++]=(PanelProfile){shape,x+(col-(cols-1)*0.5)*dx+(stagger?((row%2)?0.25:-0.25)*dx:0),
            y+(row-(rows-1)*0.5)*dy,w,h,0,0};
    ProError error=vt_through(tx,cmd,frame,profiles,n,sheet,out);
    if(!error&&isfinite(web))part_text_add(out,L"孔间留料下界 %.8g mm。",web);
    return error;
}

static ProError vt_grip_cut(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,PanelProfile profile,int sheet,PartText *out){
    double t=as_arg(cmd,8),low=as_arg(cmd,cmd->count-2),high=as_arg(cmd,cmd->count-1);
    if(t<low||t>high)return PRO_TK_BAD_INPUTS;
    ProError error=vt_through(tx,cmd,frame,&profile,1,sheet,out);
    if(!error)part_text_add(out,L"已核对板厚落在所提供的配件允许范围 %.8g–%.8g mm。",low,high);
    return error;
}
