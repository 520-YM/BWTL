static ProError smy_finish_protection(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,t;
    ProError error=fab_thickness(model,&mm,&t);
    if(error)return error;
    PartCommand frame_args={0};
    wcscpy_s(frame_args.arg[3],PART_TOKEN_MAX,cmd->arg[2]);
    wcscpy_s(frame_args.arg[4],PART_TOKEN_MAX,cmd->arg[3]);
    FdTxn view={0};
    view.model=model;view.scale=1/mm;
    PanelFrame frame={0};
    error=pn_frame(&view,&frame_args,&frame);
    if(error)return error;
    double width=as_arg(cmd,6),height=as_arg(cmd,7),border=as_arg(cmd,8),min_pad=as_arg(cmd,9),pad=as_arg(cmd,10);
    SmxClearance scan={0};
    scan.model=model;scan.mm=mm;scan.frame=frame;scan.nearest=INFINITY;scan.all_boundaries=1;
    scan.profile=pn_rect(as_arg(cmd,4),as_arg(cmd,5),width,height);
    error=ProSurfaceInit((ProSolid)model,frame.face_id,&scan.surface);
    if(!error)error=ProSurfaceContourVisit(scan.surface,smx_clearance_contour,NULL,&scan);
    if(error)return error;
    if(!isfinite(scan.nearest))return PRO_TK_E_NOT_FOUND;
    double clearance=scan.nearest-smx_profile_bound(&scan.profile);
    int panel=clearance+0.000001>=border;
    if(panel){
        error=pn_profile_probes(&view,&frame,&scan.profile,t,0);
        if(error==PRO_TK_BAD_CONTEXT)panel=0;
        else if(error)return error;
    }
    part_text_add(out,L"外观面保护：接触垫 %.8g × %.8g mm，距最近边界 EDGE:%d 的净距下界 %.8g mm，要求 %.8g mm；输入保护垫厚度 %.8g mm，要求 %.8g mm，%ls。\n",
        width,height,scan.edge,clearance,border,pad,min_pad,panel&&pad>=min_pad?L"符合":L"超限");
    return PRO_TK_NO_ERROR;
}

static ProError smy_coating_point(ProMdl model,const PartCommand *cmd,PartText *out){
    ShopMouth mouth={0};
    double mm,t;
    ProError error=smy_access_mouth(model,cmd->arg[3],cmd->arg[4],&mouth,&mm,&t);
    if(error)return error;
    InspectionSeat scan={&mouth,1/mm,INFINITY};
    error=ProSurfaceContourVisit(mouth.ring.face,in_seat_contour,NULL,&scan);
    if(error)return error;
    if(!isfinite(scan.nearest))return PRO_TK_E_NOT_FOUND;
    double diameter=2*mouth.radius,tool=as_arg(cmd,5),min=as_arg(cmd,6),max=as_arg(cmd,7),requirement=as_arg(cmd,8);
    int mask=!wcscmp(cmd->arg[2],L"MASK");
    double difference=mask?tool-diameter:diameter-tool;
    double web=scan.nearest-mouth.radius;
    int pass=difference>=min-0.000001&&difference<=max+0.000001&&web>=requirement-0.000001;
    part_text_add(out,L"%ls：孔径 %.8g mm，%ls %.8g mm，允许 %.8g–%.8g mm；到其它边界最小料桥 %.8g mm，要求 %.8g mm，%ls。\n",
        mask?L"喷涂遮蔽塞":L"喷涂挂钩",diameter,mask?L"直径过盈":L"直径余量",difference,min,max,web,requirement,pass?L"符合":L"超限");
    return PRO_TK_NO_ERROR;
}
