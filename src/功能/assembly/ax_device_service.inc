static ProError ax_device_rail(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent rail={0},module={0};
    AxDeviceFrame local,world,source,target;
    ProOutline length,body;
    AxBatchItem items[12]={0};
    int count=(int)as_arg(cmd,6),loaded=0;
    double start=as_arg(cmd,7)/mm,pitch=as_arg(cmd,8)/mm;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&rail);
    if(!error)error=ax_device_frame(&rail,cmd->arg[3],0,&local);
    if(!error)error=ax_device_frame(&rail,cmd->arg[3],1,&world);
    if(!error)error=ax_device_bounds(rail.model,&local,length);
    if(!error)error=ax_batch_model(cmd->arg[4],mm,&module.model,&loaded);
    if(!error)error=ax_device_frame(&module,cmd->arg[5],0,&source);
    if(!error)error=ax_device_bounds(module.model,&source,body);
    if(error)goto done;
    double low=start+body[0][0],high=start+(count-1)*pitch+body[1][0];
    if(low<length[0][0]-0.001/mm||high>length[1][0]+0.001/mm||
       (count>1&&pitch<body[1][0]-body[0][0]-0.001/mm)){
        part_text_add(out,L"模块超出导轨实测长度，或排列间距不足以容纳模块。\n");
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    for(int i=0;i<count;i++){
        target=world;
        for(int j=0;j<3;j++)target.origin[j]+=world.axis[0][j]*(start+i*pitch);
        items[i].model=module.model;
        ax_device_align(&source,&target,items[i].pose);
    }
    AxDevicePlane mounting={0};
    memcpy(mounting.point,world.origin,sizeof(ProVector));
    memcpy(mounting.normal,world.axis[2],sizeof(ProVector));
    error=ax_device_create(assembly,items,count,mm,&rail,&mounting,NULL,NULL,out);
    if(!error)part_text_add(out,L"导轨方向实测包络长度 %.8g mm，模块占用 %.8g mm，已核对排列间距。\n",
        (length[1][0]-length[0][0])*mm,(high-low)*mm);
done:
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(module.model);
    return error;
}

static ProError ax_device_line(AxComponent *owner,const wchar_t *token,int world,AxGeometry *line){
    FdRef ref={0};
    ProError error=fd_ref_resolve(owner->model,token,PRO_TYPE_UNUSED,&ref);
    if(!error)error=ax_geometry(&ref.item,line);
    fd_ref_free(&ref);
    if(error)return error;
    if(line->type==AX_GEOM_PLANE)return PRO_TK_INVALID_TYPE;
    if(world){
        ProMatrix pose;
        AxGeometry local=*line;
        if((error=ax_device_pose(owner,pose))||
           (error=ProPntTrfEval(local.point,pose,line->point))||
           (error=ProVectorTrfEval(local.direction,pose,line->direction)))return error;
    }
    return ad_unit(line->direction)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static int ax_hinge_axis_match(const AxGeometry *a,const AxGeometry *b,double mm){
    return fabs(ad_dot(a->direction,b->direction))>1-1e-7&&
        ax_hw_separation(a->point,b->point,a->direction,mm)<0.01;
}
static int ax_hinge_face_match(const AxDevicePlane *target,const AxDevicePlane *local,ProMatrix pose,double mm){
    ProVector point,normal,delta;
    if(ProPntTrfEval((double *)local->point,pose,point)||ProVectorTrfEval((double *)local->normal,pose,normal))return 0;
    ad_sub(point,target->point,delta);
    return ad_dot(normal,target->normal)<-1+1e-7&&fabs(ad_dot(delta,target->normal))*mm<0.01;
}

static ProError ax_device_hinges(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent cabinet={0},door={0},hinge={0};
    AxDeviceFrame source,destination;
    AxGeometry first,second,axis;
    AxDevicePlane cabinet_face,door_face,hinge_cab,hinge_door;
    AxBatchItem items[3]={0};
    int count=cmd->count-13,loaded=0;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&cabinet);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[5],mm,&door);
    if(error)return error;
    if(ax_same_occurrence(&cabinet,&door))return PRO_TK_BAD_INPUTS;
    if((error=ax_device_line(&cabinet,cmd->arg[3],1,&first))||
       (error=ax_device_line(&door,cmd->arg[6],1,&second)))return error;
    if(!ax_hinge_axis_match(&first,&second,mm)){
        part_text_add(out,L"柜体和闭门状态下的门板转轴不共线。\n");return PRO_TK_BAD_CONTEXT;
    }
    if((error=ax_device_plane_world(&cabinet,cmd->arg[4],&cabinet_face))||
       (error=ax_device_plane_world(&door,cmd->arg[7],&door_face)))return error;
    if((error=ax_batch_model(cmd->arg[8],mm,&hinge.model,&loaded))||
       (error=ax_device_frame(&hinge,cmd->arg[9],0,&source))||
       (error=ax_device_line(&hinge,cmd->arg[10],0,&axis))||
       (error=ax_device_face(&hinge,cmd->arg[11],&hinge_cab))||
       (error=ax_device_face(&hinge,cmd->arg[12],&hinge_door)))goto done;
    for(int i=0;i<count;i++){
        AxGeometry mounted=axis;
        if((error=ax_device_frame(&cabinet,cmd->arg[13+i],1,&destination)))goto done;
        items[i].model=hinge.model;
        ax_device_align(&source,&destination,items[i].pose);
        if((error=ProPntTrfEval(axis.point,items[i].pose,mounted.point))||
           (error=ProVectorTrfEval(axis.direction,items[i].pose,mounted.direction)))goto done;
        if(!ax_hinge_axis_match(&first,&mounted,mm)||
           !ax_hinge_face_match(&cabinet_face,&hinge_cab,items[i].pose,mm)||
           !ax_hinge_face_match(&door_face,&hinge_door,items[i].pose,mm)){
            part_text_add(out,L"第 %d 个铰链站位的转轴或两侧安装面不吻合。\n",i+1);
            error=PRO_TK_BAD_CONTEXT;goto done;
        }
    }
    error=ax_device_create(assembly,items,count,mm,&cabinet,&cabinet_face,&door,&door_face,out);
    if(!error)part_text_add(out,L"已核对 %d 个铰链的共同转轴与闭门状态下两侧安装面。\n",count);
done:
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(hinge.model);
    return error;
}

static ProError ax_plane_footprint(AxComponent *owner,ProSurface face,const double *world,ProMatrix inverse){
    ProVector local;
    ProUvParam uv;
    ProUvStatus location;
    ProError error=ProPntTrfEval((double *)world,inverse,local);
    if(!error)error=ProSurfaceParamEval((ProSolid)owner->model,face,local,uv);
    if(!error)error=ProSurfaceUvpntVerify((ProSolid)owner->model,face,uv,&location);
    return error?error:location==PRO_UV_OUTSIDE?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
}

static ProError ax_device_seal(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent frame={0},door={0};
    AxDevicePlane first,second;
    ProMatrix fp,dp,fi,di;
    double thickness=as_arg(cmd,6),min_pct=as_arg(cmd,7),max_pct=as_arg(cmd,8);
    double min_gap=INFINITY,max_gap=-INFINITY,length=0;
    if(min_pct>max_pct)return PRO_TK_BAD_INPUTS;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&frame);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[4],mm,&door);
    if(error)return error;
    if(ax_same_occurrence(&frame,&door))return PRO_TK_BAD_INPUTS;
    if((error=ax_device_plane_world(&frame,cmd->arg[3],&first))||
       (error=ax_device_plane_world(&door,cmd->arg[5],&second))||
       (error=ax_device_pose(&frame,fp))||(error=ax_device_pose(&door,dp)))return error;
    if(ad_dot(first.normal,second.normal)>-1+1e-7)return PRO_TK_BAD_CONTEXT;
    ax_device_inverse(fp,fi);ax_device_inverse(dp,di);
    for(int i=9;i<cmd->count;i++){
        for(int j=9;j<i;j++)if(!wcscmp(cmd->arg[i],cmd->arg[j]))return PRO_TK_BAD_INPUTS;
        ProEdge edge;
        ProEnttype type;
        double segment;
        if((error=ax_device_edge(frame.model,cmd->arg[i],&edge))||(error=ProEdgeTypeGet(edge,&type)))return error;
        if(type!=PRO_ENT_LINE)return PRO_TK_INVALID_TYPE;
        if((error=ProEdgeLengthEval(edge,&segment)))return error;
        if(!isfinite(segment)||segment*mm<0.001)return PRO_TK_BAD_CONTEXT;
        length+=segment*mm;
        for(int j=0;j<3;j++){
            ProVector local,point,delta,d1,d2,d3,projected;
            if((error=ProEdgeXyzdataEval(edge,0.15+0.35*j,local,d1,d2,d3))||
               (error=ProPntTrfEval(local,fp,point)))return error;
            ad_sub(point,first.point,delta);
            if(fabs(ad_dot(delta,first.normal))*mm>0.01)return PRO_TK_BAD_CONTEXT;
            if((error=ax_plane_footprint(&frame,first.handle,point,fi)))return error;
            ad_sub(second.point,point,delta);
            double gap=ad_dot(delta,first.normal);
            for(int k=0;k<3;k++)projected[k]=point[k]+gap*first.normal[k];
            if((error=ax_plane_footprint(&door,second.handle,projected,di))){
                part_text_add(out,L"%ls 的采样点投影落在闭门压合面之外。\n",cmd->arg[i]);return error;
            }
            min_gap=fmin(min_gap,gap*mm);max_gap=fmax(max_gap,gap*mm);
        }
    }
    double lo=100*(1-max_gap/thickness),hi=100*(1-min_gap/thickness);
    part_text_add(out,L"已检查 %d 段直线路径（每段 3 点），总长 %.8g mm。闭门间隙 %.8g 至 %.8g mm。\n",
        cmd->count-9,length,min_gap,max_gap);
    part_text_add(out,L"按自由厚度 %.8g mm，设计压缩率 %.8g%% 至 %.8g%%，要求 %.8g%% 至 %.8g%%，%ls。\n",
        thickness,lo,hi,min_pct,max_pct,lo+0.001>=min_pct&&hi-0.001<=max_pct&&min_gap>=0?L"符合":L"不符合");
    return PRO_TK_NO_ERROR;
}
