static ProError smy_tool_withdrawal(ProMdl model,const PartCommand *cmd,PartText *out){
    PanelFrame frame={0};
    double mm;
    ProError error=qa_mm_per_unit(model,&mm);
    if(!error)error=in_box_frame(model,cmd->arg[2],&frame.placement.csys);
    if(error)return error;
    double x=as_arg(cmd,3),y=as_arg(cmd,4),start=as_arg(cmd,5),finish=as_arg(cmd,6);
    double width=as_arg(cmd,7),depth=as_arg(cmd,8),length=as_arg(cmd,9),margin=as_arg(cmd,10);
    int round=!wcscmp(cmd->arg[11],L"ROUND");
    if(fabs(start-finish)<0.001||(round&&fabs(width-depth)>0.000001))return PRO_TK_BAD_INPUTS;
    SmyAccessResult engaged,exit,transit;
    double half=length/2+margin;
    width+=2*margin;
    depth+=2*margin;
    error=smy_access_sweep(model,&frame,mm,x,y,width,depth,start-half,start+half,round,&engaged);
    if(!error)error=smy_access_sweep(model,&frame,mm,x,y,width,depth,finish-half,finish+half,round,&exit);
    if(!error)error=smy_access_sweep(model,&frame,mm,x,y,width,depth,fmin(start,finish)-half,fmax(start,finish)+half,round,&transit);
    if(error)return error;
    smy_access_report(L"工位姿态",&engaged,out);
    smy_access_report(L"退出终点",&exit,out);
    smy_access_report(L"全退出行程",&transit,out);
    const wchar_t *result=engaged.blocked?L"工位已干涉":exit.blocked?L"退出终点已干涉":
        transit.blocked?L"起终点取样净空，退出途中已确认干涉":engaged.unknown||exit.unknown||transit.unknown?
        L"部分射线需复核":L"退出行程取样未见侵入";
    part_text_add(out,L"%ls。检查指定坐标系 Z 向直线退出，结果覆盖取样区域。\n",result);
    return PRO_TK_NO_ERROR;
}
