static ProError aw_glass_blocks(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent glass={0},pad={0};
    AxDeviceFrame frame,source;
    ProOutline box,pad_box;
    int loaded=0;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&glass);
    if(!error)error=ax_device_frame(&glass,cmd->arg[3],1,&frame);
    if(!error)error=ax_device_world_bounds(&glass,&frame,box);
    if(!error)error=ax_batch_model(cmd->arg[4],mm,&pad.model,&loaded);
    if(!error)error=ax_device_frame(&pad,cmd->arg[5],0,&source);
    if(!error)error=ax_device_bounds(pad.model,&source,pad_box);
    if(error)goto done;
    const double ratio=as_arg(cmd,6)/100,const_gap=as_arg(cmd,7)/mm;
    const double edge=as_arg(cmd,8)/mm,width=box[1][0]-box[0][0];
    const double low=width*ratio,high=width*(1-ratio);
    if(low+pad_box[0][0]<edge||high+pad_box[1][0]>width-edge||
       high+pad_box[0][0]-low-pad_box[1][0]<0.001/mm){
        part_text_add(out,L"按给定位置和实体垫块外廓，支点边距不足或两块重叠。\n");
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    if(pad_box[1][1]-pad_box[0][1]<0.001/mm){error=PRO_TK_BAD_CONTEXT;goto done;}
    const double x[2]={box[0][0]+low,box[0][0]+high};
    const double y=box[0][1]-const_gap,z=(box[0][2]+box[1][2])*0.5;
    const int side_blocks=!wcscmp(cmd->arg[9],L"SIDES");
    const int count=side_blocks?4:2;
    AxBatchItem items[4]={0};
    AxDevicePlane support[4]={0};
    for(int i=0;i<count;i++){
        AxDeviceFrame destination=frame;
        double px=i==0?x[0]:x[1],py=y;
        double station[2]={box[0][0],box[0][1]};
        const int side=i>=2;
        if(side){
            if(pad_box[1][0]-pad_box[0][0]+2*edge>box[1][1]-box[0][1]){
                error=PRO_TK_BAD_CONTEXT;goto done;
            }
            const double theta=(i==2?-1:1)*AS_PI/2;
            px=i==2?box[0][0]-const_gap:box[1][0]+const_gap;
            py=(box[0][1]+box[1][1])*0.5-(pad_box[0][0]+pad_box[1][0])*0.5*(i==2?-1:1);
            station[0]=i==2?box[0][0]:box[1][0];
            for(int j=0;j<3;j++)ax_rotate_vector(frame.axis[j],frame.axis[2],cos(theta),sin(theta),destination.axis[j]);
        }
        for(int j=0;j<3;j++){
            destination.origin[j]=frame.origin[j]+px*frame.axis[0][j]+py*frame.axis[1][j]+z*frame.axis[2][j]-pad_box[1][1]*destination.axis[1][j];
            support[i].normal[j]=destination.axis[1][j];
            support[i].point[j]=frame.origin[j]+station[0]*frame.axis[0][j]+station[1]*frame.axis[1][j];
        }
        items[i].model=pad.model;
        ax_device_align(&source,&destination,items[i].pose);
    }
    error=ax_batch_create(assembly,items,count,mm,1,out);
    if(!error){
        for(int i=0;!error&&i<count;i++){
            double interference;
            error=ax_device_fit(assembly,&items[i],&glass,&support[i],mm,&interference);
            if(!error&&interference>0.001)error=PRO_TK_BAD_CONTEXT;
        }
        if(error){
            for(int i=count-1;i>=0;i--)ax_discard_occurrence(assembly,&items[i].feature,out);
            if(!ax_recovery_uncertain)part_text_add(out,L"已移除本次新增垫块，需调整厚度或姿态。\n");
        }
    }
    if(!error)part_text_add(out,L"沿玻璃实际下缘布置 2 个标准支撑块%ls，支点各距左右 %.8g mm。承压留空 %.8g mm，未保存。\n",
        side_blocks?L"和 2 个防窜块":L"",low*mm,const_gap*mm);

done:
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(pad.model);
    return error;
}

static ProError aw_ground_bond(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent cabinet={0},terminal={0};
    AxHardwareSite stud,lug;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&cabinet);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[5],mm,&terminal);
    if(!error)error=ax_hw_site(&cabinet,cmd->arg[3],cmd->arg[4],mm,&stud);
    if(!error)error=ax_hw_site(&terminal,cmd->arg[6],cmd->arg[7],mm,&lug);
    if(!error)error=ax_hw_world(&cabinet,&stud);
    if(!error)error=ax_hw_world(&terminal,&lug);
    if(error)return error;
    if(stud.bore||!lug.bore)return PRO_TK_INVALID_TYPE;
    const double offset=ax_hw_separation(stud.contact,lug.contact,stud.cylinder.direction,mm);
    const double angle=ax_q_angle(stud.cylinder.direction,lug.cylinder.direction);
    ProVector delta;
    ad_sub(lug.contact,stud.contact,delta);
    const double seat=fabs(ad_dot(delta,stud.plane.direction))*mm;
    const double allowance=(lug.cylinder.radius-stud.cylinder.radius)*mm;
    const wchar_t *const names[]={L"BWTL_GROUND_PATH"};
    ProLine wire;
    ax_parameter_text(terminal.model,names,1,wire);
    const int good=allowance+0.001>=offset&&angle<0.01&&ax_q_within(seat,as_arg(cmd,8));
    part_text_add(out,L"接地点 %ls 与线鼻子 %ls：杆径 %.8g mm、环孔径 %.8g mm、偏心 %.8g mm、承压间隙 %.8g mm。%ls。\n",
        cmd->arg[2],cmd->arg[5],2*stud.cylinder.radius*mm,2*lug.cylinder.radius*mm,offset,seat,
        good?L"几何适配":L"几何或接触条件不符");
    part_text_add(out,L"接地走向属性 %ls。电气连续性由样机实测确认。\n",ax_bom_known(wire));
    return PRO_TK_NO_ERROR;
}

static ProError aw_connector_opening(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    double opening[4],depth[2];
    AxDeviceFrame frame;
    ProError error=aw_rectangular_opening(assembly,cmd,2,mm,&frame,opening,depth);
    if(error)return error;
    AxComponent plug={0};
    if((error=ax_hw_occurrence(assembly,cmd->arg[8],mm,&plug)))return error;
    const double tilt=as_arg(cmd,10)*AS_PI/180;
    const int axis=ax_axis_index(cmd->arg[9]);
    const double wanted=as_arg(cmd,11);
    const double width=(opening[1]-opening[0])*mm,height=(opening[3]-opening[2])*mm;
    for(int i=0;i<2;i++){
        AxDeviceFrame projected=frame;
        if(i){
            ProVector direction;
            memcpy(direction,frame.axis[axis],sizeof(direction));
            for(int j=0;j<3;j++)ax_rotate_vector(frame.axis[j],direction,cos(tilt),-sin(tilt),projected.axis[j]);
        }
        ProOutline extents;
        if((error=ax_device_world_bounds(&plug,&projected,extents)))return error;
        const double w=(extents[1][0]-extents[0][0])*mm,h=(extents[1][1]-extents[0][1])*mm;
        const double gap_x=(width-w)*0.5,gap_y=(height-h)*0.5;
        int pass=qa_at_least(gap_x,wanted)&&qa_at_least(gap_y,wanted);
        part_text_add(out,L"%ls，开口 %.8g×%.8g mm，插头及其锁扣外廓投影 %.8g×%.8g mm，居中穿过的单侧余量 %.8g / %.8g mm。%ls。\n",
            i?L"指定斜穿姿态":L"当前姿态",width,height,w,h,gap_x,gap_y,pass?L"截面几何可通过":L"截面不足");
    }
    part_text_add(out,L"结果覆盖给定角度和所选插头实体。转向、线缆行程及开口前后障碍另用插拔运动检查。\n");
    return PRO_TK_NO_ERROR;
}

static ProError aw_spacing_modes(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame frame;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[3],&frame);
    if(error)return error;
    ProOutline cabinet;
    if((error=aw_part_bounds(assembly,cmd->arg[2],&frame,mm,cabinet)))return error;
    const wchar_t *const sides[]={L"-X",L"+X",L"-Y",L"+Y",L"-Z",L"+Z"};
    int failed=0;
    for(int i=0;i<6;i++){
        const int axis=i/2,positive=i%2;
        const double boundary=as_arg(cmd,4+i);
        const double actual=positive?boundary-cabinet[1][axis]*mm:cabinet[0][axis]*mm-boundary;
        const double required=as_arg(cmd,10+i);
        int pass=qa_at_least(actual,required);
        failed+=!pass;
        part_text_add(out,L"%ls 安装净空 %.8g mm（要求 ≥ %.8g），%ls。\n",sides[i],actual,required,pass?L"符合":L"不足");
    }
    part_text_add(out,L"在给定墙面、邻柜或现场边界的六向装配外廓检查中，%d 向不足。开门和动态取出需按对应姿态复核。\n",failed);
    return PRO_TK_NO_ERROR;
}
