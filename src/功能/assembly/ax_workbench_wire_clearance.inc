static ProError aw_line_curve(ProMdl assembly,const wchar_t *id,const wchar_t *token,double mm,ProVector ends[2]){
    AxComponent owner={0};
    ProCurve curve;
    ProError error=aw_route_curve(assembly,id,token,&owner,&curve);
    if(error)return error;
    double scale,length;
    if((error=qa_mm_per_unit(owner.model,&scale))||(error=ProCurveLengthEval(curve,&length)))return error;
    if(!part_close(scale,mm))return PRO_TK_BAD_CONTEXT;
    for(int j=0;j<2;j++)if((error=aw_route_point(&owner,curve,j,ends[j])))return error;
    const double chord=aw_distance3(ends[0],ends[1]);
    return chord*mm>0.001&&fabs(chord-length)*mm<0.001?PRO_TK_NO_ERROR:PRO_TK_INVALID_TYPE;
}

static double aw_clamp(double parameter){return fmin(1,fmax(0,parameter));}

static double aw_segment_distance(ProVector a[2],ProVector b[2],double *x,double *y){
    ProVector u,v,w,difference;
    ad_sub(a[1],a[0],u);ad_sub(b[1],b[0],v);ad_sub(a[0],b[0],w);
    double s=0,t=0;
    const double uu=ad_dot(u,u),vv=ad_dot(v,v),uv=ad_dot(u,v),uw=ad_dot(u,w),vw=ad_dot(v,w);
    const double det=uu*vv-uv*uv;
    if(det>1e-12*uu*vv)s=aw_clamp((uv*vw-vv*uw)/det);
    t=(uv*s+vw)/vv;
    if(t<0){t=0;s=aw_clamp(-uw/uu);}
    else if(t>1){t=1;s=aw_clamp((uv-uw)/uu);}
    for(int i=0;i<3;i++)difference[i]=w[i]+s*u[i]-t*v[i];
    *x=s;*y=t;
    return sqrt(ad_dot(difference,difference));
}

static ProError aw_edge_clearance(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    ProVector route[2],edge_points[2];
    ProError error=aw_line_curve(assembly,cmd->arg[2],cmd->arg[3],mm,route);
    AxComponent carrier={0};
    ProMatrix pose;
    ProEdge edge;
    ProEnttype type;
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[4],mm,&carrier);
    if(!error)error=ax_device_edge(carrier.model,cmd->arg[5],&edge);
    if(!error)error=ProEdgeTypeGet(edge,&type);
    if(!error)error=ax_device_pose(&carrier,pose);
    if(error||type!=PRO_ENT_LINE)return error?error:PRO_TK_INVALID_TYPE;
    for(int i=0;i<2;i++){
        ProVector point;
        if((error=ProEdgeXyzdataEval(edge,i,point,NULL,NULL,NULL))||(error=ProPntTrfEval(point,pose,edge_points[i])))return error;
    }
    if(aw_distance3(edge_points[0],edge_points[1])*mm<0.001)return PRO_TK_BAD_CONTEXT;
    double cable_at,edge_at;
    const double gap=aw_segment_distance(route,edge_points,&cable_at,&edge_at)*mm-as_arg(cmd,6)/2;
    const double required=as_arg(cmd,7);
    ProVector nearest;
    for(int i=0;i<3;i++)nearest[i]=(route[0][i]+cable_at*(route[1][i]-route[0][i]))*mm;
    part_text_add(out,L"线缆外皮到指定直板边的最小余量 %.8g mm（要求 ≥ %.8g）。最近线中心 (%.8g, %.8g, %.8g) mm。\n",
        gap,required,nearest[0],nearest[1],nearest[2]);
    part_text_add(out,L"%ls。%ls 位于该板边 %.8g%% 处。\n",qa_at_least(gap,required)?L"余量符合":L"需复核磨线和护边",
        cmd->arg[5],edge_at*100);
    return PRO_TK_NO_ERROR;
}

static ProError aw_wire_crossing(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    ProVector high[2],low[2],u,v;
    ProError error=aw_line_curve(assembly,cmd->arg[2],cmd->arg[3],mm,high);
    if(!error)error=aw_line_curve(assembly,cmd->arg[4],cmd->arg[5],mm,low);
    if(error)return error;
    ad_sub(high[1],high[0],u);ad_sub(low[1],low[0],v);
    if(!ad_unit(u)||!ad_unit(v))return PRO_TK_BAD_CONTEXT;
    double x,y;
    const double separation=aw_segment_distance(high,low,&x,&y)*mm-(as_arg(cmd,6)+as_arg(cmd,7))*0.5;
    const double limit=as_arg(cmd,8),angle=ax_q_angle(u,v),parallel_limit=as_arg(cmd,9),minimum_cross_angle=as_arg(cmd,10);
    ProVector b0,b1;
    ad_sub(low[0],high[0],b0);ad_sub(low[1],high[0],b1);
    const double first=ad_dot(b0,u),second=ad_dot(b1,u),span=aw_distance3(high[0],high[1]);
    const double overlap=fmax(0,fmin(span,fmax(first,second))-fmax(0,fmin(first,second)))*mm;
    int parallel=angle<5&&overlap>parallel_limit;
    int cross=(x>0&&x<1&&y>0&&y<1&&angle>=5&&angle<minimum_cross_angle);
    part_text_add(out,L"强弱电直线段最小外皮间距 %.8g mm，夹角 %.8g°，沿强电方向投影重叠 %.8g mm。\n",separation,angle,overlap);
    part_text_add(out,L"最小间距：%ls。长距离近似平行：%ls。内部相交方向夹角：%ls。\n",
        qa_at_least(separation,limit)?L"符合":L"间距不足",parallel?L"超过输入限制":L"所选段未超输入限制",
        cross?L"低于输入交叉角要求":L"所选段未检出小角度内部交叉");
    return PRO_TK_NO_ERROR;
}

static ProError aw_door_wire(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame fixed,moving;
    AxComponent door={0};
    AxGeometry axis;
    ProError error=aw_frame_world(assembly,cmd->arg[2],cmd->arg[3],&fixed);
    if(!error)error=aw_frame_world(assembly,cmd->arg[4],cmd->arg[5],&moving);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[4],mm,&door);
    if(!error)error=ax_device_line(&door,cmd->arg[6],1,&axis);
    if(error)return error;
    const double length=as_arg(cmd,9),reserve=as_arg(cmd,10),minimum=as_arg(cmd,7),maximum=as_arg(cmd,8);
    const int n=(int)as_arg(cmd,11);
    ProVector radial;
    ad_sub(moving.origin,axis.point,radial);
    double highest=0,when=minimum,lowest=INFINITY;
    for(int i=0;i<n;i++){
        double angle=minimum+(maximum-minimum)*i/(n-1),radian=angle*AS_PI/180;
        ProVector point;
        ax_rotate_vector(radial,axis.direction,cos(radian),sin(radian),point);
        for(int j=0;j<3;j++)point[j]+=axis.point[j];
        const double distance=aw_distance3(point,fixed.origin)*mm;
        if(distance>highest){highest=distance;when=angle;}
        lowest=fmin(lowest,distance);
    }
    const double slack=length-highest;
    part_text_add(out,L"采样 %d 个开合姿态，固定与门侧端点的直线间距 %.8g～%.8g mm，最长对应 %.8g°。\n",n,lowest,highest,when);
    part_text_add(out,L"可动线段长 %.8g mm，最长点的理论余量 %.8g mm（规定 ≥ %.8g）。%ls。\n",length,slack,reserve,
        qa_at_least(slack,reserve)?L"端点直线余量符合":L"有拉紧风险");
    part_text_add(out,L"端点距离是路径长度下界。夹线、护边和实际弯线包络需按对应运动检查。\n");
    return PRO_TK_NO_ERROR;
}
