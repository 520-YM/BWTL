static ProError ax_hw_sites(AxComponent *base,const PartCommand *cmd,int first,double mm,AxHardwareSite sites[12]){
    for(int i=first;i<cmd->count;i++){
        AxHardwareSite *site=&sites[i-first];
        ProError error=ax_hw_site(base,cmd->arg[i],cmd->arg[3],mm,site);
        if(!error)error=ax_hw_world(base,site);
        if(error||!site->bore)return error?error:PRO_TK_INVALID_TYPE;
        for(int j=0;j<i-first;j++)
            if(ad_near(site->contact,sites[j].contact,0.001/mm))return PRO_TK_E_FOUND;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_hw_place_holes(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent base={0},part={0};
    AxHardwareSite sites[12],fitting;
    AxBatchItem items[12]={0};
    int loaded=0,first=kind==AX_PRESS_HARDWARE?8:7;
    double limit=kind==AX_PRESS_HARDWARE?as_arg(cmd,7):0;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&base);
    if(!error)error=ax_hw_sites(&base,cmd,first,mm,sites);
    if(!error)error=ax_batch_model(cmd->arg[4],mm,&part.model,&loaded);
    if(!error)error=ax_hw_site(&part,cmd->arg[5],cmd->arg[6],mm,&fitting);
    if(error)goto done;
    double start,end;
    ax_hw_axial_span(&fitting,fitting.contact,fitting.plane.direction,mm,&start,&end);
    if(fitting.bore||end<=0.001){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<cmd->count-first;i++){
        double interference=2*(fitting.cylinder.radius-sites[i].cylinder.radius)*mm;
        if(interference>limit+0.001){
            part_text_add(out,L"%ls 的直径差 %.6g mm 超过允许过盈 %.6g mm。\n",cmd->arg[first+i],interference,limit);
            error=PRO_TK_BAD_CONTEXT;goto done;
        }
        items[i].model=part.model;
        ax_hw_seat_pose(&fitting,&sites[i],items[i].pose);
    }
    error=ax_batch_create(assembly,items,cmd->count-first,mm,1,out);
done:
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(part.model);
    return error;
}

static ProError ax_hw_stack(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent base={0},bolt={0},washer={0},nut={0};
    AxHardwareSite front,back,shank,washer_bottom,washer_top,nut_seat;
    AxBatchItem items[3]={0};
    int loaded[3]={0};
    double thickness,washer_mm;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&base);
    if(!error)error=ax_hw_slab(&base,cmd->arg[3],cmd->arg[4],cmd->arg[5],mm,&front,&back,&thickness);
    if(!error)error=ax_batch_model(cmd->arg[6],mm,&bolt.model,&loaded[0]);
    if(!error)error=ax_batch_model(cmd->arg[9],mm,&washer.model,&loaded[1]);
    if(!error)error=ax_batch_model(cmd->arg[13],mm,&nut.model,&loaded[2]);
    if(!error)error=ax_hw_site(&bolt,cmd->arg[7],cmd->arg[8],mm,&shank);
    if(!error)error=ax_hw_site(&washer,cmd->arg[10],cmd->arg[11],mm,&washer_bottom);
    if(!error)error=ax_hw_site(&washer,cmd->arg[10],cmd->arg[12],mm,&washer_top);
    if(!error)error=ax_hw_site(&nut,cmd->arg[14],cmd->arg[15],mm,&nut_seat);
    if(error)goto done;
    ProVector delta;
    ad_sub(washer_bottom.contact,washer_top.contact,delta);
    washer_mm=ad_dot(delta,washer_bottom.plane.direction)*mm;
    double start,end;
    ax_hw_axial_span(&shank,shank.contact,shank.plane.direction,mm,&start,&end);
    if(shank.bore||!washer_bottom.bore||!nut_seat.bore||washer_mm<=0.001||
       ad_dot(washer_bottom.plane.direction,washer_top.plane.direction)>-1+1e-7||
       shank.cylinder.radius>fmin(front.cylinder.radius,fmin(washer_bottom.cylinder.radius,nut_seat.cylinder.radius))+0.0005/mm||
       end<=thickness+washer_mm+0.001){
        part_text_add(out,L"请核对螺栓杆、垫圈及螺母孔径和穿过夹持层后的有效长度。\n");
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    items[0].model=washer.model;
    ax_hw_seat_pose(&washer_bottom,&front,items[0].pose);
    AxHardwareSite upper=front;
    for(int i=0;i<3;i++)upper.contact[i]+=upper.plane.direction[i]*washer_mm/mm;
    items[1].model=bolt.model;
    ax_hw_seat_pose(&shank,&upper,items[1].pose);
    items[2].model=nut.model;
    ax_hw_seat_pose(&nut_seat,&back,items[2].pose);
    error=ax_batch_create(assembly,items,3,mm,1,out);
    if(!error)part_text_add(out,L"已核对板厚 %.6g mm、垫圈厚 %.6g mm，螺栓从垫圈侧穿入，螺母在另一侧。\n",thickness,washer_mm);
done:
    if(error&&!ax_recovery_uncertain){
        ProMdl models[]={bolt.model,washer.model,nut.model};
        for(int i=2;i>=0;i--)if(loaded[i])ProMdlErase(models[i]);
    }
    return error;
}

static ProError ax_hw_fit(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent base={0},bolt={0};
    AxHardwareSite front,back,shank;
    double thickness;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&base);
    if(!error)error=ax_hw_slab(&base,cmd->arg[3],cmd->arg[4],cmd->arg[5],mm,&front,&back,&thickness);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[6],mm,&bolt);
    if(!error)error=ax_hw_site(&bolt,cmd->arg[7],cmd->arg[8],mm,&shank);
    if(!error)error=ax_hw_world(&bolt,&shank);
    if(error)return error;
    ProVector inward,delta;
    for(int i=0;i<3;i++)inward[i]=-front.plane.direction[i];
    ad_sub(back.contact,shank.contact,delta);
    double clamp=ad_dot(delta,inward)*mm,start,end;
    ax_hw_axial_span(&shank,shank.contact,inward,mm,&start,&end);
    double radial=ax_hw_separation(shank.contact,front.contact,inward,mm);
    if(shank.bore||fabs(ad_dot(shank.cylinder.direction,inward))<1-1e-7||
       ad_dot(shank.plane.direction,inward)<1-1e-7||radial>0.001||clamp+0.001<thickness)
        return PRO_TK_BAD_CONTEXT;
    double thread_start=as_arg(cmd,9),thread_end=thread_start+as_arg(cmd,10);
    double nut_depth=as_arg(cmd,11),required=as_arg(cmd,12);
    double engaged=fmax(0,fmin(end,fmin(thread_end,clamp+nut_depth))-fmax(start,fmax(clamp,thread_start)));
    double clearance=2*(front.cylinder.radius-shank.cylinder.radius)*mm;
    part_text_add(out,L"孔径 %.6g mm，杆径 %.6g mm，直径间隙 %.6g mm。板厚 %.6g mm，实测夹持厚度 %.6g mm。\n",
        2*front.cylinder.radius*mm,2*shank.cylinder.radius*mm,clearance,thickness,clamp);
    part_text_add(out,L"按给定螺纹区间和螺母有效深度计算，啮合 %.6g mm，要求至少 %.6g mm。%ls\n",
        engaged,required,clearance>=-0.001&&engaged+0.001>=required?L"符合":L"不符合");
    return PRO_TK_NO_ERROR;
}

static ProError ax_hw_audit(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent base={0};
    AxHardwareSite sites[12];
    AxScan *scan=NULL;
    int matches[12]={0},n=cmd->count-6;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&base);
    if(!error)error=ax_hw_sites(&base,cmd,6,mm,sites);
    if(!error)error=ax_scan(assembly,1,&scan);
    if(error)goto done;
    if(scan->suppressed){error=PRO_TK_INCOMPLETE;goto done;}
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model||item->type!=PRO_MDL_PART||_wcsicmp(item->name,cmd->arg[4]))continue;
        ProIdTable ids={0};
        AxComponent fastener={0};
        AxHardwareSite actual={0};
        fastener.model=item->model;
        double model_mm;
        memcpy(ids,item->ids,item->depth*sizeof(int));
        if((error=ProAsmcomppathInit((ProSolid)assembly,ids,item->depth,&fastener.path))||
           (error=qa_mm_per_unit(fastener.model,&model_mm)))goto done;
        if(!part_close(model_mm,mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
        error=ax_hw_cylinder(&fastener,cmd->arg[5],&actual.cylinder,actual.end,&actual.bore);
        if(error||actual.bore){if(!error)error=PRO_TK_INVALID_TYPE;goto done;}
        memcpy(actual.contact,actual.cylinder.point,sizeof(actual.contact));
        memcpy(actual.plane.direction,actual.cylinder.direction,sizeof(actual.plane.direction));
        if((error=ax_hw_world(&fastener,&actual)))goto done;
        for(int j=0;j<n;j++){
            ProVector inward;
            for(int k=0;k<3;k++)inward[k]=-sites[j].plane.direction[k];
            if(fabs(ad_dot(actual.cylinder.direction,inward))<1-1e-7||
               ax_hw_separation(actual.contact,sites[j].contact,inward,mm)>0.05)continue;
            double start,end;
            ax_hw_axial_span(&actual,sites[j].contact,inward,mm,&start,&end);
            if(start<=0.05&&end>0.01)matches[j]++;
        }
    }
    int missing=0,duplicate=0;
    for(int i=0;i<n;i++){
        if(!matches[i])missing++;
        if(matches[i]>1)duplicate++;
        part_text_add(out,L"%ls：%ls（%d 个实例）\n",cmd->arg[i+6],
            !matches[i]?L"漏装":matches[i]>1?L"重复安装":L"已安装",matches[i]);
    }
    part_text_add(out,L"检查 %d 个孔位，漏装 %d，重复安装 %d。\n",n,missing,duplicate);
done:
    free(scan);
    return error;
}

static ProError ax_hardware(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    if(kind==AX_FASTENER_FIT)return ax_hw_fit(assembly,cmd,mm,out);
    if(kind==AX_FASTENER_AUDIT)return ax_hw_audit(assembly,cmd,mm,out);
    if(kind==AX_FASTENER_STACK)return ax_hw_stack(assembly,cmd,mm,out);
    return ax_hw_place_holes(assembly,cmd,kind,mm,out);
}
