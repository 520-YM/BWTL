static ProError aw_thermal(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent glass={0},base={0};
    AxDeviceFrame frame,world;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&glass);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[3],mm,&base);
    if(!error)error=ax_device_frame(&base,cmd->arg[4],0,&frame);
    if(!error)error=ax_device_frame(&base,cmd->arg[4],1,&world);
    if(error)return error;
    PartCommand window=*cmd;
    for(int i=0;i<4;i++)wcscpy_s(window.arg[4+i],PART_TOKEN_MAX,cmd->arg[5+i]);
    double opening[4],depth[2];
    ProOutline body;
    if((error=ax_rect_opening(&base,&window,&frame,mm,opening,depth))||
       (error=ax_device_world_bounds(&glass,&world,body)))return error;
    const double delta=as_arg(cmd,11),g=as_arg(cmd,9)*delta/1e6,b=as_arg(cmd,10)*delta/1e6;
    const double anchors[]={as_arg(cmd,13)/mm,as_arg(cmd,14)/mm};
    const double required=as_arg(cmd,12);
    const wchar_t *side[]={L"左",L"右",L"下",L"上"};
    int failed=0;
    for(int axis=0;axis<2;axis++){
        const double center=(body[0][axis]+body[1][axis])*0.5;
        const double widened_low=opening[2*axis]+(opening[2*axis]-anchors[axis])*b;
        const double widened_high=opening[2*axis+1]+(opening[2*axis+1]-anchors[axis])*b;
        const double shifted=center+(center-anchors[axis])*b;
        const double expanded=(body[1][axis]-body[0][axis])*(1+g)*0.5;
        const double cold[]={(body[0][axis]-opening[2*axis])*mm,(opening[2*axis+1]-body[1][axis])*mm};
        const double hot[]={(shifted-expanded-widened_low)*mm,(widened_high-shifted-expanded)*mm};
        for(int i=0;i<2;i++){
            int pass=qa_at_least(hot[i],required);
            failed+=!pass;
            part_text_add(out,L"%ls侧常温余量 %.8g mm，温差 %+.8g °C 时预计 %.8g mm（要求 ≥ %.8g），%ls。\n",
                side[2*axis+i],cold[i],delta,hot[i],required,pass?L"符合":L"不足");
        }
    }
    part_text_add(out,L"共 %d 侧不足。均匀温差模型，假设玻璃中心随框体、从玻璃中心自由膨胀。\n",failed);
    return PRO_TK_NO_ERROR;
}

static ProError aw_shared_face_span(AxQualityFace *a,AxQualityFace *b,double mm){
    ProVector basis[3];
    ax_hw_basis(a->plane.normal,basis);
    AxDevicePlane direction={0};
    memcpy(direction.point,a->plane.point,sizeof(ProVector));
    for(int i=0;i<2;i++){
        double lo1,hi1,lo2,hi2;
        memcpy(direction.normal,basis[i],sizeof(ProVector));
        ProError error=ax_q_span(a,&direction,mm,&lo1,&hi1);
        if(!error)error=ax_q_span(b,&direction,mm,&lo2,&hi2);
        if(error)return error;
        if(fmin(hi1,hi2)-fmax(lo1,lo2)<0.001)return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static ProError aw_door_gaps(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    double global_min=INFINITY,global_max=-INFINITY;
    double minimum=as_arg(cmd,2),maximum=as_arg(cmd,3),variation=as_arg(cmd,4);
    if(maximum<minimum)return PRO_TK_BAD_INPUTS;
    for(int i=5;i<cmd->count;i+=4){
        AxQualityFace first={0},second={0};
        ProError error=ax_q_face(assembly,cmd->arg[i],cmd->arg[i+1],mm,&first);
        if(!error)error=ax_q_face(assembly,cmd->arg[i+2],cmd->arg[i+3],mm,&second);
        if(error)return error;
        const double d=ad_dot(first.plane.normal,second.plane.normal);
        if(d>-0.95){
            part_text_add(out,L"第 %d 对侧面没有相向，请选择门板和门框面对门缝的实体面。\n",(i-5)/4+1);
            return PRO_TK_BAD_CONTEXT;
        }
        if((error=aw_shared_face_span(&first,&second,mm))){
            part_text_add(out,L"第 %d 对侧面找不到共同投影跨度。\n",(i-5)/4+1);
            return error;
        }
        double low,high,min2,max2;
        if((error=ax_q_span(&second,&first.plane,mm,&low,&high))||
           (error=ax_q_span(&first,&second.plane,mm,&min2,&max2)))return error;
        const double bore_min=fmin(low,min2),bore_max=fmax(high,max2);
        global_min=fmin(global_min,bore_min);
        global_max=fmax(global_max,bore_max);
        part_text_add(out,L"第 %d 对侧面 %ls / %ls 的投影门缝 %.8g～%.8g mm。\n",(i-5)/4+1,cmd->arg[i],cmd->arg[i+2],bore_min,bore_max);
    }
    const double spread=global_max-global_min;
    part_text_add(out,L"所选侧面最小 %.8g mm、最大 %.8g mm、变化 %.8g mm（允许 %.8g～%.8g mm、变化 ≤ %.8g mm），%ls。\n",
        global_min,global_max,spread,minimum,maximum,variation,
        qa_at_least(global_min,minimum)&&ax_q_within(global_max,maximum)&&ax_q_within(spread,variation)?L"符合":L"不符合");
    return PRO_TK_NO_ERROR;
}

static ProError aw_drain(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxQualityFace basin={0},outlet={0};
    AxComponent root={0};
    AxDeviceFrame gravity;
    ProError error=ax_q_face(assembly,cmd->arg[2],cmd->arg[3],mm,&basin);
    if(!error)error=ax_q_face(assembly,cmd->arg[4],cmd->arg[5],mm,&outlet);
    if(!error)error=ax_ref_occurrence(assembly,L"ROOT",&root);
    if(!error)error=ax_device_frame(&root,cmd->arg[6],0,&gravity);
    if(error)return error;
    AxDevicePlane elevation={0};
    memcpy(elevation.point,gravity.origin,sizeof(ProVector));
    memcpy(elevation.normal,gravity.axis[2],sizeof(ProVector));
    double basin_low,outlet_low,high;
    if((error=ax_q_span(&basin,&elevation,mm,&basin_low,&high))||
       (error=ax_q_span(&outlet,&elevation,mm,&outlet_low,&high)))return error;
    const double rise=outlet_low-basin_low,limit=as_arg(cmd,7);
    part_text_add(out,L"底板实测最低标高 %.8g mm，所选排水口壁面最低标高 %.8g mm，高差 %+.8g mm（允许 ≤ %.8g）：%ls。\n",
        basin_low,outlet_low,rise,limit,rise<=limit+1e-8?L"符合":L"排水口高于底板低点");
    return PRO_TK_NO_ERROR;
}

static ProError aw_shim_plan(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    const double target=as_arg(cmd,2),stock=as_arg(cmd,3),max_shim=as_arg(cmd,4);
    int failed=0,unknown=0;
    for(int i=5;i<cmd->count;i+=4){
        AxQualityFace from={0},to={0};
        double gap;
        ProError error=aw_parallel_distance(assembly,cmd,i,mm,&gap,&from,&to);
        if(error){
            part_text_add(out,L"铰链站位 %ls 未取得两个平行面（Creo %d）。\n",cmd->arg[i],error);unknown++;continue;
        }
        const double adjustment=target-fabs(gap);
        const double rounded=stock>0?round(adjustment/stock)*stock:adjustment;
        int pass=rounded>=-1e-7&&ax_q_within(rounded,max_shim);
        failed+=!pass;
        part_text_add(out,L"站位 %ls / %ls 实测 %.8g mm，目标 %.8g mm，计算需 %+.8g mm，按给定垫片取 %+.8g mm，预估 %.8g mm。\n",
            cmd->arg[i],cmd->arg[i+2],fabs(gap),target,adjustment,rounded,fabs(gap)+rounded);
        if(!pass)part_text_add(out,L"%ls。\n",rounded<0?L"需减薄已有垫片或调铰链":L"超出允许垫片量");
    }
    part_text_add(out,L"%d 个站位超范围，%d 个未核实。垫入方向取增加两选定面的距离，未修改模型。\n",failed,unknown);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

