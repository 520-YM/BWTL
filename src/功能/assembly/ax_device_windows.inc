static ProError ax_rect_opening(AxComponent *base,const PartCommand *cmd,const AxDeviceFrame *frame,
    double mm,double edges[4],double depth[2]){
    double cross[4][2];
    depth[0]=-INFINITY;depth[1]=INFINITY;
    for(int i=0;i<4;i++){
        AxDevicePlane face;
        ProError error=ax_device_face(base,cmd->arg[4+i],&face);
        if(error)return error;
        int axis=i/2;
        if(ad_dot(face.normal,frame->axis[axis])*(i%2?-1:1)<1-1e-7)return PRO_TK_BAD_CONTEXT;
        edges[i]=ax_project(face.point,frame,axis);
        double low,high;
        if((error=ax_surface_span(face.handle,frame,1-axis,&cross[i][0],&cross[i][1]))||
           (error=ax_surface_span(face.handle,frame,2,&low,&high)))return error;
        depth[0]=fmax(depth[0],low);depth[1]=fmin(depth[1],high);
    }
    if((edges[1]-edges[0])*mm<0.001||(edges[3]-edges[2])*mm<0.001||
       (depth[1]-depth[0])*mm<0.001)return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<4;i++){
        int other=i<2?2:0;
        if((cross[i][0]-edges[other])*mm>0.001||(edges[other+1]-cross[i][1])*mm>0.001)
            return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_rect_profile(AxComponent *part,const wchar_t *token,const AxDeviceFrame *frame,
    double edges[4],double *depth){
    AxDevicePlane face;
    ProError error=ax_device_face(part,token,&face);
    if(error)return error;
    if(fabs(ad_dot(face.normal,frame->axis[2]))<1-1e-7)return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<2;i++)if((error=ax_surface_span(face.handle,frame,i,&edges[2*i],&edges[2*i+1])))return error;
    double area;
    if((error=ProSurfaceAreaEval(face.handle,&area)))return error;
    double box=(edges[1]-edges[0])*(edges[3]-edges[2]);
    if(!isfinite(area)||box<=0||fabs(area-box)>fmax(1e-7,box*1e-5))return PRO_TK_BAD_CONTEXT;
    *depth=ax_project(face.point,frame,2);
    return PRO_TK_NO_ERROR;
}

static ProError ax_filter_sweep(ProMdl assembly,const PartCommand *cmd,AxBatchItem *item,
    const AxDeviceFrame *world,double mm,PartText *out){
    if(!wcscmp(cmd->arg[15],L"NONE")){
        part_text_add(out,L"已记录抽出方向和行程，未指定检查障碍件。\n");
        return PRO_TK_NO_ERROR;
    }
    AxComponent obstacle={0};
    ProMatrix inverse;
    AxDeviceFrame local;
    ProOutline moving,blocking;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[15],mm,&obstacle);
    if(error)return error;
    ax_device_inverse(item->pose,inverse);
    ax_transform_frame(world,inverse,&local);
    if((error=ax_device_bounds(item->model,&local,moving))||
       (error=ax_device_world_bounds(&obstacle,world,blocking)))return error;
    int axis=ax_axis_index(cmd->arg[13]+1);
    double distance=as_arg(cmd,14)/mm;
    moving[cmd->arg[13][0]==L'+'?1:0][axis]+=cmd->arg[13][0]==L'+'?distance:-distance;
    for(int i=0;i<3;i++)
        if(moving[1][i]<blocking[0][i]-0.001/mm||blocking[1][i]<moving[0][i]-0.001/mm){
            part_text_add(out,L"%.8g mm 抽出包络与指定障碍件分离。\n",distance*mm);
            return PRO_TK_NO_ERROR;
        }
    part_text_add(out,L"抽出包络与指定障碍件包络相交，需要复核检修空间。\n");
    return PRO_TK_BAD_CONTEXT;
}

static ProError ax_device_window(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent base={0},part={0};
    AxDeviceFrame base_local,world,source,destination;
    AxBatchItem item={0};
    double opening[4],profile[4],span[2],face_z,min_required=as_arg(cmd,12);
    int loaded=0;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&base);
    if(!error)error=ax_device_frame(&base,cmd->arg[3],0,&base_local);
    if(!error)error=ax_device_frame(&base,cmd->arg[3],1,&world);
    if(!error)error=ax_rect_opening(&base,cmd,&base_local,mm,opening,span);
    if(!error)error=ax_batch_model(cmd->arg[8],mm,&part.model,&loaded);
    if(!error)error=ax_device_frame(&part,cmd->arg[9],0,&source);
    if(!error)error=ax_rect_profile(&part,cmd->arg[10],&source,profile,&face_z);
    if(error)goto done;
    int cover=kind==AX_GLASS_MOUNT||kind==AX_FILTER_MOUNT;
    double x=(opening[1]-opening[0]-profile[1]+profile[0])*mm/2;
    double y=(opening[3]-opening[2]-profile[3]+profile[2])*mm/2;
    if(cover){x=-x;y=-y;}
    part_text_add(out,L"实测开口 %.8g × %.8g mm，居中后左右%ls %.8g mm、上下%ls %.8g mm。\n",
        (opening[1]-opening[0])*mm,(opening[3]-opening[2])*mm,cover?L"压边":L"余量",x,cover?L"压边":L"余量",y);
    if(fmin(x,y)+0.001<min_required){error=PRO_TK_BAD_CONTEXT;goto done;}
    double z=as_arg(cmd,11)/mm;
    if(kind==AX_GLASS_MOUNT){
        z+=span[1];
        part_text_add(out,L"玻璃承接面距开口正侧 %.8g mm（设计胶层）。\n",as_arg(cmd,11));
    }
    if(kind==AX_LATCH_MOUNT){
        double entry=span[0];
        if(fabs(entry*mm)>0.001||z>span[1]+0.001/mm||(z-entry)*mm+0.001<as_arg(cmd,13)){
            part_text_add(out,L"锁舌端面没有达到指定的插入深度或超出直槽。\n");
            error=PRO_TK_BAD_CONTEXT;goto done;
        }
        part_text_add(out,L"锁舌端面计划插入 %.8g mm。\n",(z-entry)*mm);
    }
    destination=world;
    double shift[]={(opening[0]+opening[1]-profile[0]-profile[1])/2,
        (opening[2]+opening[3]-profile[2]-profile[3])/2,z-face_z};
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)destination.origin[j]+=world.axis[i][j]*shift[i];
    item.model=part.model;
    ax_device_align(&source,&destination,item.pose);
    if(kind==AX_FILTER_MOUNT&&(error=ax_filter_sweep(assembly,cmd,&item,&world,mm,out)))goto done;
    error=ax_device_create(assembly,&item,1,mm,&base,NULL,NULL,NULL,out);
    if(!error)part_text_add(out,L"已核对选定组件的实体干涉。\n");
done:
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(part.model);
    return error;
}
