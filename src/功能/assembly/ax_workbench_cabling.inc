static ProError aw_route_curve(ProMdl assembly,const wchar_t *id,const wchar_t *name,
                              AxComponent *owner,ProCurve *curve){
    ProError error=ax_ref_occurrence(assembly,id,owner);
    if(error)return error;
    ProFeature feature;
    ProGeomitem item;
    if((error=ux_named_feature(owner->model,name+6,&feature)))return error;
    return ad_single_curve(&feature,&item,curve);
}

static ProError aw_route_point(AxComponent *owner,ProCurve curve,double param,ProVector point){
    ProVector local;
    ProError error=ProCurveXyzdataEval(curve,param,local,NULL,NULL);
    if(error)return error;
    if(!owner->path.table_num){memcpy(point,local,sizeof(ProVector));return PRO_TK_NO_ERROR;}
    ProMatrix pose;
    error=ax_device_pose(owner,pose);
    return error?error:ProPntTrfEval(local,pose,point);
}

static ProError aw_cable_length(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    const double start=as_arg(cmd,2),end=as_arg(cmd,3),service=as_arg(cmd,4);
    const double tolerance=as_arg(cmd,5);
    double total=0;
    ProVector tail={0};
    for(int i=6;i<cmd->count;i+=2){
        AxComponent owner={0};
        ProCurve curve;
        ProError error=aw_route_curve(assembly,cmd->arg[i],cmd->arg[i+1],&owner,&curve);
        if(error)return error;
        double scale,length;
        ProVector first,last;
        if((error=qa_mm_per_unit(owner.model,&scale))||!part_close(scale,mm))return error?error:PRO_TK_BAD_CONTEXT;
        if((error=ProCurveLengthEval(curve,&length))||(error=aw_route_point(&owner,curve,0,first))||
           (error=aw_route_point(&owner,curve,1,last)))return error;
        if(!isfinite(length)||length*mm<0.001)return PRO_TK_BAD_CONTEXT;
        int reversed=0;
        if(i>6){
            const double a=aw_distance3(tail,first)*mm,b=aw_distance3(tail,last)*mm;
            if(fmin(a,b)>tolerance){
                part_text_add(out,L"%ls %ls 与上一段端点不连续，最小接头距 %.8g mm。\n",cmd->arg[i],cmd->arg[i+1],fmin(a,b));
                return PRO_TK_BAD_CONTEXT;
            }
            if(a<=tolerance&&b<=tolerance)return PRO_TK_BAD_CONTEXT;
            reversed=b<a;
        }else if(i+2<cmd->count){
            AxComponent second={0};
            ProCurve second_curve;
            ProVector p,q;
            if((error=aw_route_curve(assembly,cmd->arg[i+2],cmd->arg[i+3],&second,&second_curve))||
               (error=aw_route_point(&second,second_curve,0,p))||(error=aw_route_point(&second,second_curve,1,q)))return error;
            const double a=fmin(aw_distance3(first,p),aw_distance3(first,q))*mm;
            const double b=fmin(aw_distance3(last,p),aw_distance3(last,q))*mm;
            reversed=a+tolerance<b;
        }
        memcpy(tail,reversed?first:last,sizeof(ProVector));
        length*=mm;total+=length;
        part_text_add(out,L"%ls %ls 的原生曲线长 %.8g mm。\n",cmd->arg[i],cmd->arg[i+1],length);
    }
    part_text_add(out,L"路径长 %.8g mm，两端余量 %.8g / %.8g mm，检修余量 %.8g mm。下料长度 %.8g mm。\n",
        total,start,end,service,total+start+end+service);
    return PRO_TK_NO_ERROR;
}

static ProError aw_cable_bend(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    const double required=as_arg(cmd,2);
    int unknown=0,failed=0,measured=0;
    for(int i=3;i<cmd->count;i+=2){
        AxComponent owner={0};
        ProCurve curve;
        ProEnttype kind;
        ProError error=aw_route_curve(assembly,cmd->arg[i],cmd->arg[i+1],&owner,&curve);
        if(!error)error=ProCurveTypeGet(curve,&kind);
        if(error)return error;
        double scale;
        if((error=qa_mm_per_unit(owner.model,&scale))||!part_close(scale,mm))return error?error:PRO_TK_BAD_CONTEXT;
        double length;
        ProVector start,end;
        if((error=ProCurveLengthEval(curve,&length))||
           (error=ProCurveXyzdataEval(curve,0,start,NULL,NULL))||
           (error=ProCurveXyzdataEval(curve,1,end,NULL,NULL)))return error;
        const double chord=aw_distance3(start,end);
        if((kind==PRO_ENT_LINE||fabs(length-chord)*mm<=1e-5)&&chord*mm>0.001){
            part_text_add(out,L"%ls %ls 是直线段，段内没有弯曲。\n",cmd->arg[i],cmd->arg[i+1]);continue;
        }
        if(kind!=PRO_ENT_ARC&&kind!=PRO_ENT_CIRCLE){
            part_text_add(out,L"%ls %ls 曲线类型 %d，弯曲半径未核实。\n",cmd->arg[i],cmd->arg[i+1],kind);unknown++;continue;
        }
        ProGeomitemdata *raw=NULL;
        if((error=ProCurveDataGet(curve,&raw)))return error;
        const ProCurvedata *shape=raw->data.p_curve_data;
        double radius=shape?(kind==PRO_ENT_ARC?shape->arc.radius:shape->circle.radius)*mm:0;
        ProGeomitemdataFree(&raw);
        if(!isfinite(radius)||radius<=0){unknown++;continue;}
        int pass=qa_at_least(radius,required);
        failed+=!pass;
        measured++;
        part_text_add(out,L"%ls %ls 原生弯曲半径 %.8g mm（最少 %.8g），%ls。\n",
            cmd->arg[i],cmd->arg[i+1],radius,required,pass?L"符合":L"半径不足");
    }
    part_text_add(out,L"量测 %d 个圆弧段，半径不足 %d 段，未核实 %d 段。段间尖角需单独核对。\n",measured,failed,unknown);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static ProError aw_channel_fill(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent channel={0};
    AxDeviceFrame frame;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&channel);
    if(!error)error=ax_device_frame(&channel,cmd->arg[3],0,&frame);
    if(error)return error;
    double limits[4],length[2];
    if((error=ax_rect_opening(&channel,cmd,&frame,mm,limits,length)))return error;
    const double width=(limits[1]-limits[0])*mm,height=(limits[3]-limits[2])*mm,area=width*height;
    const double cover=as_arg(cmd,8),max_percent=as_arg(cmd,9);
    if(cover>=height)return PRO_TK_BAD_INPUTS;
    double sum=0,diameter_max=0;
    for(int i=10;i<cmd->count;i+=2){
        const double diameter=as_arg(cmd,i),qty=as_arg(cmd,i+1);
        sum+=AS_PI*diameter*diameter*qty/4;
        diameter_max=fmax(diameter_max,diameter);
    }
    double net=width*(height-cover),fill=100*sum/net;
    int pass=ax_q_within(fill,max_percent)&&ax_q_within(diameter_max,fmin(width,height-cover));
    part_text_add(out,L"槽内实际截面 %.8g×%.8g mm（毛面积 %.8g mm²），扣盖板占高 %.8g mm 后有效面积 %.8g mm²。\n",
        width,height,area,cover,net);
    part_text_add(out,L"线缆截面积 %.8g mm²，占用 %.8g%%（上限 %.8g%%），最大线径 %.8g mm。%ls。线束实际堆叠和转角仍需核对。\n",
        sum,fill,max_percent,diameter_max,pass?L"面积及单根线径符合":L"面积或线径不符合");
    return PRO_TK_NO_ERROR;
}
