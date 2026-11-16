static ProError aw_door_hand(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    ProMdl alternate=NULL;
    int loaded=0;
    ProError error=ax_load_subassembly(cmd->arg[2],&alternate,&loaded);
    if(error)return error;
    if(alternate==assembly){error=PRO_TK_BAD_INPUTS;goto done;}
    AxDeviceFrame frame;
    double scale;
    if((error=ax_master(alternate))||(error=qa_mm_per_unit(alternate,&scale))||
       (error=aw_frame_world(alternate,L"ROOT",cmd->arg[3],&frame)))goto done;
    const int left=!wcscmp(cmd->arg[4],L"LEFT");
    const double center=as_arg(cmd,5),minimum=as_arg(cmd,6);
    int failures=0;
    for(int i=7;i<cmd->count;i+=4){
        AxDeviceFrame station;
        if((error=aw_frame_world(alternate,cmd->arg[i+1],cmd->arg[i+2],&station)))goto done;
        const double x=ax_project(station.origin,&frame,0)*scale-center;
        int target_left=left;
        if(!wcscmp(cmd->arg[i],L"LATCH"))target_left=!left;
        if(!wcscmp(cmd->arg[i],L"WIRE"))target_left=!wcscmp(cmd->arg[i+3],L"L");
        const int pass=target_left?x<=-minimum:x>=minimum;
        failures+=!pass;
        part_text_add(out,L"%ls %ls 距门中线有符号 X %.8g mm，要求 %ls且距中线 ≥ %.8g mm。%ls。\n",
            cmd->arg[i],cmd->arg[i+1],x,target_left?L"左侧":L"右侧",minimum,pass?L"符合":L"方向或距离不符");
    }
    part_text_add(out,L"备用 %ls 开门配置 %ls 有 %d 项站位不符。检修净空需用相应闭门和开门姿态核对，当前装配未改变。\n",
        left?L"左":L"右",cmd->arg[2],failures);
done:
    if(loaded&&alternate){ProError erase=ProMdlErase(alternate);if(!error)error=erase;}
    return error;
}

static ProError aw_reassembly_guide(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame source,target;

    AxComponent installed={0};
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&installed);
    if(!error)error=ax_device_frame(&installed,cmd->arg[3],1,&source);
    if(!error)error=aw_frame_world(assembly,cmd->arg[4],cmd->arg[5],&target);
    if(error)return error;
    const double required=as_arg(cmd,6);
    const double position=aw_distance3(source.origin,target.origin)*mm;
    const double forward=acos(qa_clamp(ad_dot(source.axis[0],target.axis[0])))*180/AS_PI;
    const double key=acos(qa_clamp(ad_dot(source.axis[1],target.axis[1])))*180/AS_PI;
    const double angle_tolerance=as_arg(cmd,7);
    const int pass=ax_q_within(position,required)&&ax_q_within(forward,angle_tolerance)&&ax_q_within(key,angle_tolerance);
    part_text_add(out,L"回装 %ls 时，把定位坐标系 %ls 的原点对准 %ls 的 %ls；X 轴对应定位方向，Y 轴对应防转边。\n",
        cmd->arg[2],cmd->arg[3],cmd->arg[4],cmd->arg[5]);
    part_text_add(out,L"当前站位偏差 %.8g mm，定位轴偏角 %.8g°，防转轴偏角 %.8g°。%ls。\n",
        position,forward,key,pass?L"当前回装方向符合":L"原点、安装轴或防转朝向需调整");
    return PRO_TK_NO_ERROR;
}

static ProError aw_posture(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    const int count=(cmd->count-6)/2;
    if(count<3)return PRO_TK_BAD_INPUTS;
    AxDeviceFrame upright;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&upright);
    if(error)return error;
    const int around=ax_axis_index(cmd->arg[3]);
    const double degrees=as_arg(cmd,4),minimum=as_arg(cmd,5),angle=degrees*AS_PI/180;
    AxDeviceFrame rest=upright;
    for(int i=0;i<3;i++)
        ax_rotate_vector(upright.axis[i],upright.axis[around],cos(angle),-sin(angle),rest.axis[i]);
    ProMassProperty props;
    error=ProSolidMassPropertyWithDensityGet((ProSolid)assembly,NULL,PRO_MP_DENS_DEFAULT,0,&props);
    if(error||!isfinite(props.mass)||props.mass<=0||
       !isfinite(props.center_of_gravity[0])||!isfinite(props.center_of_gravity[1])||!isfinite(props.center_of_gravity[2])){
        part_text_add(out,L"当前装配的实际质量或重心不可读，请先核对活动零件的材料和密度。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    AsPoint support[AX_BATCH_LIMIT];
    for(int i=0;i<count;i++){
        const double x=as_arg(cmd,6+2*i),y=as_arg(cmd,7+2*i);
        support[i]=as_point(x,y);
    }
    double sd=INFINITY,orientation=0;
    const AsPoint center=as_point(ax_project(props.center_of_gravity,&rest,0)*mm,
                                  ax_project(props.center_of_gravity,&rest,1)*mm);
    for(int i=0;i<count;i++){
        AsPoint a=support[i],b=support[(i+1)%count],c=support[(i+2)%count];
        AsPoint u=as_minus(b,a),v=as_minus(c,b);
        double length=as_dist(a,b),winding=as_cross2(u,v);
        if(length<0.001||fabs(winding)<1e-8||orientation*winding<0)return PRO_TK_BAD_INPUTS;
        orientation=winding;
        for(int j=0;j<count;j++)
            if(as_cross2(u,as_minus(support[j],a))*orientation<-1e-8)return PRO_TK_BAD_INPUTS;
        double signed_gap=as_cross2(u,as_minus(center,a))/length*(winding>0?1:-1);
        sd=fmin(sd,signed_gap);
    }
    part_text_add(out,L"按 %ls 轴翻 %.8g°，重心在新支撑平面投影 (%.8g, %.8g) mm，至支撑边最小有符号距离 %.8g mm。\n",
        cmd->arg[3],degrees,center.x,center.y,sd);
    part_text_add(out,L"最小支撑裕量 %.8g mm：%ls。根据指定工位支撑多边形和模型当前材料；摆放应验证真实支撑。\n",
        minimum,qa_at_least(sd,minimum)?L"当前姿态几何通过":L"不宜按此姿态支撑");
    return PRO_TK_NO_ERROR;
}

static ProError aw_handling(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame frame,lift,forks[2];
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&frame);
    if(!error)error=aw_frame_world(assembly,cmd->arg[3],cmd->arg[4],&lift);
    if(!error)error=aw_frame_world(assembly,cmd->arg[5],cmd->arg[6],&forks[0]);
    if(!error)error=aw_frame_world(assembly,cmd->arg[7],cmd->arg[8],&forks[1]);
    if(error)return error;
    ProMassProperty props;
    error=ProSolidMassPropertyWithDensityGet((ProSolid)assembly,NULL,PRO_MP_DENS_DEFAULT,0,&props);
    if(error||!isfinite(props.mass)||props.mass<=0||
       !isfinite(props.center_of_gravity[0])||!isfinite(props.center_of_gravity[1])||!isfinite(props.center_of_gravity[2])){
        part_text_add(out,L"当前装配的实际质量或重心不可读，请先核对活动零件的材料和密度。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    const double dx=(ax_project(lift.origin,&frame,0)-ax_project(props.center_of_gravity,&frame,0))*mm;
    const double dy=(ax_project(lift.origin,&frame,1)-ax_project(props.center_of_gravity,&frame,1))*mm;
    const double offset=hypot(dx,dy);
    ProVector span;
    ad_sub(forks[1].origin,forks[0].origin,span);
    const double spacing=fabs(ad_dot(span,frame.axis[1]))*mm;
    const double lift_allow=as_arg(cmd,9),low=as_arg(cmd,10),high=as_arg(cmd,11);
    part_text_add(out,L"指定吊点相对重心平面投影偏心 %.8g mm（允许 %.8g），前后 %.8g mm、左右 %.8g mm。%ls。\n",
        offset,lift_allow,dx,dy,ax_q_within(offset,lift_allow)?L"几何偏心符合":L"几何偏心超限");
    part_text_add(out,L"两叉车孔中心沿基准 Y 跨距 %.8g mm（输入叉齿范围 %.8g～%.8g），%ls。\n",
        spacing,low,high,spacing+1e-8>=low&&spacing<=high+1e-8?L"间距符合":L"间距不足或超限");
    const double sweep=as_arg(cmd,12);
    int blocked=0;
    if(sweep>0){
        for(int i=0;i<3;i++){
            const AxDeviceFrame *station=i?&forks[i-1]:&lift;
            const double *direction=station->axis[i?0:2];
            for(int j=13;j<cmd->count;j++){
                double hit;
                int face;
                if((error=aw_cast_drop(assembly,cmd->arg[j],station->origin,direction,mm,sweep,&hit,&face)))return error;
                if(!isfinite(hit))continue;
                blocked++;
                part_text_add(out,L"%ls中心线在 %.8g mm 处碰到 %ls SURFACE:%d。\n",
                    i?L"叉运":L"吊装",hit,cmd->arg[j],face);
            }
        }
    }
    part_text_add(out,L"已核对几何中心线阻碍 %d 处，实际吊索及叉齿包络、吊点承载和现场起吊条件须单独验证。\n",blocked);
    return PRO_TK_NO_ERROR;
}
