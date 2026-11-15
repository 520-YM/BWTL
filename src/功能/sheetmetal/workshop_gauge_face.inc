static ProError smy_gauge_face(ProMdl model,const PartCommand *cmd,PartText *out){
    int bend_id=_wtoi(cmd->arg[2]+wcslen(L"SURFACE:"));
    ProVector low,high;
    SmyFlangeScan scan={0};
    scan.model=model;
    ProError error=smy_bend_span(model,bend_id,&scan.bend,scan.axis,low,high);
    if(!error)error=ProSurfaceContourVisit(scan.bend,smy_dfm_contour,NULL,&scan);
    if(error)return error;
    CabinetEdge edge={0};
    error=cm_edge_get(model,cmd->arg[3],&edge);
    if(error)return error;
    int adjacent=0;
    for(int i=0;i<scan.count;i++)if(scan.faces[i]==edge.face)adjacent=1;
    const double *axis=scan.axis;
    if(!adjacent||fabs(ad_dot(edge.axis,axis))<0.999999){
        part_text_add(out,L"指定边需要是折弯相切板面上与弯轴平行的自由直边。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    double mm,t;
    error=fab_thickness(model,&mm,&t);
    if(error)return error;
    double a=ad_dot(edge.a,axis),b=ad_dot(edge.b,axis);
    double start=fmax(fmin(a,b),ad_dot(low,axis)),end=fmin(fmax(a,b),ad_dot(high,axis));
    double length=fmax(0,end-start)*mm;
    double jaw=as_arg(cmd,4),depth=as_arg(cmd,5),safe=as_arg(cmd,6);
    if(length+1e-6<jaw+2*safe){
        part_text_add(out,L"自由边与弯轴有效重叠 %.8g mm，小于挡料夹口含端部余量 %.8g mm。\n",length,jaw+2*safe);
        return PRO_TK_NO_ERROR;
    }
    PanelFrame frame={0};
    frame.face_id=edge.face;
    ProCsysdata *csys=&frame.placement.csys;
    double middle=(start+end)/2;
    cm_point(edge.a,edge.axis,(middle-a)/ad_dot(edge.axis,axis),csys->origin);
    memcpy(csys->x_vector,axis,sizeof(ProVector));
    memcpy(csys->y_vector,edge.inward,sizeof(ProVector));
    memcpy(csys->z_vector,edge.normal,sizeof(ProVector));
    memcpy(frame.inward,edge.normal,sizeof(ProVector));
    for(int i=0;i<3;i++)frame.inward[i]*=-1;
    FdTxn view={0};
    view.model=model;view.scale=1/mm;
    double first=fmin(0.2,depth/3);
    for(int i=0;i<=8;i++){
        for(int j=0;j<3;j++){
            double thickness;
            error=mp_read_stock(&view,&frame,as_point(jaw*(i/8.0-0.5),first+(depth-first)*j/2),0,&thickness,1);
            if(error||fabs(thickness-t)>0.005){
                part_text_add(out,L"夹口取样未取得完整板材支承，距自由边 %.8g mm。需调整夹口深度或位置。\n",first+(depth-first)*j/2);
                return PRO_TK_NO_ERROR;
            }
        }
    }
    part_text_add(out,L"挡料自由边 %ls 与折弯轴有效重叠 %.8g mm；居中 %.8g × %.8g mm 夹口的 27 处板厚取样通过。未模拟夹具运动。\n",
        cmd->arg[3],length,jaw,depth);
    return PRO_TK_NO_ERROR;
}

