static ProError ax_hole_pair_frame(const AxHardwareSite pair[2],AxDeviceFrame *frame,double *pitch){
    memcpy(frame->origin,pair[0].contact,sizeof(ProVector));
    memcpy(frame->axis[2],pair[0].plane.direction,sizeof(ProVector));
    ad_sub(pair[1].contact,pair[0].contact,frame->axis[0]);
    *pitch=sqrt(ad_dot(frame->axis[0],frame->axis[0]));
    if(!ad_unit(frame->axis[0])||fabs(ad_dot(frame->axis[0],frame->axis[2]))>1e-7||
       ad_dot(pair[1].plane.direction,frame->axis[2])<1-1e-7)return PRO_TK_BAD_CONTEXT;
    ad_cross(frame->axis[2],frame->axis[0],frame->axis[1]);
    return PRO_TK_NO_ERROR;
}

static ProError ax_device_facing(AxComponent *device,const wchar_t *face,const wchar_t *direction,ProMatrix pose){
    AxDevicePlane plane;
    ProVector normal;
    ProError error=ax_device_face(device,face,&plane);
    if(!error)error=ProVectorTrfEval(plane.normal,pose,normal);
    if(error)return error;
    int axis=ax_axis_index(direction+1);
    return normal[axis]*(direction[0]==L'+'?1:-1)>1-1e-7?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_pcb_height(ProMdl assembly,const PartCommand *cmd,AxBatchItem *item,double mm,PartText *out){
    AxComponent floor={0};
    AxDevicePlane plane;
    AxDeviceFrame world,local;
    ProMatrix inverse;
    ProOutline bounds;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[12],mm,&floor);
    if(!error)error=ax_device_plane_world(&floor,cmd->arg[13],&plane);
    if(error)return error;
    memcpy(world.origin,plane.point,sizeof(ProVector));
    ax_hw_basis(plane.normal,world.axis);
    ax_device_inverse(item->pose,inverse);
    ax_transform_frame(&world,inverse,&local);
    if((error=ax_device_bounds(item->model,&local,bounds)))return error;
    double gap=bounds[0][2]*mm;
    part_text_add(out,L"PCB 最低实体离指定底面 %.8g mm，要求至少 %.8g mm。\n",gap,as_arg(cmd,14));
    return gap+0.001>=as_arg(cmd,14)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_device_bolts(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent first={0},second={0},part={0};
    AxHardwareSite target[2],source[2];
    AxDeviceFrame a,b;
    AxBatchItem item={0};
    int pcb=kind==AX_PCB_MOUNT,loaded=0;
    int model=pcb?8:6,face=model+1;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&first);
    if(!error&&pcb)error=ax_hw_occurrence(assembly,cmd->arg[5],mm,&second);
    else if(!pcb)second=first;
    if(!error)error=ax_hw_site(&first,cmd->arg[4],cmd->arg[3],mm,&target[0]);
    if(!error)error=ax_hw_site(&second,cmd->arg[pcb?7:5],cmd->arg[pcb?6:3],mm,&target[1]);
    if(!error)error=ax_hw_world(&first,&target[0]);
    if(!error)error=ax_hw_world(&second,&target[1]);
    if(!error)error=ax_batch_model(cmd->arg[model],mm,&part.model,&loaded);
    for(int i=0;!error&&i<2;i++)error=ax_hw_site(&part,cmd->arg[face+1+i],cmd->arg[face],mm,&source[i]);
    if(error)goto done;
    for(int i=0;i<2;i++)
        if(!source[i].bore||(!target[i].bore&&source[i].cylinder.radius+0.0005/mm<target[i].cylinder.radius)){
            error=PRO_TK_BAD_CONTEXT;goto done;
        }
    double source_pitch,target_pitch;
    if((error=ax_hole_pair_frame(target,&a,&target_pitch))||
       (error=ax_hole_pair_frame(source,&b,&source_pitch)))goto done;
    part_text_add(out,L"实测目标孔距 %.8g mm，设备孔距 %.8g mm。\n",target_pitch*mm,source_pitch*mm);
    if(fabs(target_pitch-source_pitch)*mm>0.01){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<3;i++){a.axis[1][i]=-a.axis[1][i];a.axis[2][i]=-a.axis[2][i];}
    item.model=part.model;
    ax_device_align(&b,&a,item.pose);
    if(pcb)error=ax_pcb_height(assembly,cmd,&item,mm,out);
    else error=ax_device_facing(&part,cmd->arg[10],cmd->arg[11],item.pose);
    if(error){if(!pcb)part_text_add(out,L"定位孔序与所选端口或出风面的方向不一致。\n");goto done;}
    if(kind==AX_FAN_MOUNT){
        AxHardwareSite vent,fan;
        ProVector center;
        if((error=ax_hw_site(&first,cmd->arg[12],cmd->arg[3],mm,&vent))||
           (error=ax_hw_world(&first,&vent))||
           (error=ax_hw_site(&part,cmd->arg[13],cmd->arg[face],mm,&fan))||
           (error=ProPntTrfEval(fan.contact,item.pose,center)))goto done;
        double radial=ax_hw_separation(center,vent.contact,vent.plane.direction,mm);
        if(!vent.bore||!fan.bore||radial>0.01||fan.cylinder.radius>vent.cylinder.radius+0.005/mm){
            part_text_add(out,L"风口中心偏差或通风直径不合要求。\n");error=PRO_TK_BAD_CONTEXT;goto done;
        }
        part_text_add(out,L"风口中心偏差 %.8g mm，风道/开口直径 %.8g / %.8g mm。\n",
            radial,2*fan.cylinder.radius*mm,2*vent.cylinder.radius*mm);
    }
    AxDevicePlane mounting={0};
    memcpy(mounting.point,target[0].contact,sizeof(ProVector));
    memcpy(mounting.normal,target[0].plane.direction,sizeof(ProVector));
    error=ax_device_create(assembly,&item,1,mm,&first,&mounting,
        ax_same_occurrence(&first,&second)?NULL:&second,&mounting,out);
    if(!error&&!pcb)part_text_add(out,L"指定端口或出风面朝总装 %ls。\n",cmd->arg[11]);
done:
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(part.model);
    return error;
}
