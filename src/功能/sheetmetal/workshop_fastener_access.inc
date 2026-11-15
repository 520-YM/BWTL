typedef struct { int blocked,unknown,clear; double intrusion; } SmyAccessResult;

static ProError smy_access_sweep(ProMdl model,const PanelFrame *frame,double mm,double x,double y,
    double width,double height,double low,double high,int round,SmyAccessResult *result){
    memset(result,0,sizeof(*result));
    ProOutline bounds;
    ProError error=ProSolidOutlineGet((ProSolid)model,bounds);
    if(error)return error;
    const ProCsysdata *csys=&frame->placement.csys;
    double start=0;
    for(int i=0;i<3;i++){
        double a=(bounds[0][i]-csys->origin[i])*csys->z_vector[i]*mm;
        double b=(bounds[1][i]-csys->origin[i])*csys->z_vector[i]*mm;
        start+=fmin(a,b);
    }
    start=fmin(start,low)-1;
    for(int i=-3;i<=3;i++)for(int j=-3;j<=3;j++){
        if(round&&i*i+j*j>12)continue;
        ProRay ray;
        am_world(&frame->placement,(x+width*i/7)/mm,(y+height*j/7)/mm,start/mm,ray.start_point);
        memcpy(ray.dir_vector,csys->z_vector,sizeof(ProVector));
        QualityRay hits;
        error=qa_ray(model,&ray,0.0001/mm,&hits);
        if(error){result->unknown++;continue;}
        double occupied=0;
        for(int k=0;k+1<hits.count;k+=2){
            double a=hits.crossing[k].depth*mm+start,b=hits.crossing[k+1].depth*mm+start;
            occupied+=fmax(0,fmin(high,b)-fmax(low,a));
        }
        if(occupied>0.0002){result->blocked++;result->intrusion=fmax(result->intrusion,occupied);}
        else result->clear++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError smy_access_mouth(ProMdl model,const wchar_t *plane,const wchar_t *bore,ShopMouth *mouth,double *mm,double *t){
    ProError error=fab_thickness(model,mm,t);
    if(error)return error;
    FdTxn view={0};
    view.model=model;view.scale=1/(*mm);
    return mp_mouth(&view,plane,bore,mouth);
}

static void smy_access_report(const wchar_t *label,const SmyAccessResult *sample,PartText *out){
    part_text_add(out,L"%ls：原生射线净空 %d、确认侵入 %d、状态待复核 %d，最大测得侵入 %.8g mm。\n",
        label,sample->clear,sample->blocked,sample->unknown,sample->intrusion);
}

static ProError smy_tool_access(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    double mm,t;
    ShopMouth hole={0};
    ProError error=smy_access_mouth(model,cmd->arg[2],cmd->arg[3],&hole,&mm,&t);
    if(error)return error;
    double upper=as_arg(cmd,4),run=as_arg(cmd,5);
    double gap=as_arg(cmd,cmd->count-1);
    SmyAccessResult front,back;
    if(kind==SMY_PRESS_ACCESS){
        error=smy_access_sweep(model,&hole.frame,mm,0,0,upper+2*gap,upper+2*gap,0.002,run+gap,1,&front);
        if(!error)error=smy_access_sweep(model,&hole.frame,mm,0,0,as_arg(cmd,6)+2*gap,as_arg(cmd,6)+2*gap,
            -t-as_arg(cmd,7)-gap,-t-0.002,1,&back);
        if(!error){smy_access_report(L"上模进给", &front,out);smy_access_report(L"下模与托座", &back,out);}
    }else{
        double shaft=as_arg(cmd,6);
        if(shaft+2*gap>2*hole.radius){
            part_text_add(out,L"轴杆加径向余量超过孔径 %.8g mm，无法通过实际孔口。\n",2*hole.radius);
            return PRO_TK_NO_ERROR;
        }
        error=smy_access_sweep(model,&hole.frame,mm,0,0,upper+2*gap,upper+2*gap,0.002,run+gap,1,&front);
        if(!error)error=smy_access_sweep(model,&hole.frame,mm,0,0,shaft+2*gap,shaft+2*gap,-t,0.002,1,&back);
        if(!error){smy_access_report(L"螺钉工具旋转及轴向行程包络",&front,out);smy_access_report(L"轴杆进入孔口",&back,out);}
    }
    if(!error)part_text_add(out,L"参照实际圆孔轴，检查取样位置 %ls。\n",
        front.blocked||back.blocked?L"已确认有障碍":front.unknown||back.unknown?L"仍有待复核射线":L"均未发现侵入");
    return error;
}

static ProError smy_gauge_access(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,t;
    ShopMouth hole={0};
    ProError error=smy_access_mouth(model,cmd->arg[2],cmd->arg[3],&hole,&mm,&t);
    if(error)return error;
    double handle=as_arg(cmd,4),run=as_arg(cmd,5),plug=as_arg(cmd,6),gap=as_arg(cmd,7);
    if(plug+2*gap>2*hole.radius){
        part_text_add(out,L"塞规加径向余量大于实测孔径 %.8g mm，量具无法进入孔口。\n",2*hole.radius);
        return PRO_TK_NO_ERROR;
    }
    if(run<t){
        part_text_add(out,L"塞规有效行程 %.8g mm 短于实测板厚 %.8g mm，无法验到孔底。\n",run,t);
        return PRO_TK_NO_ERROR;
    }
    SmyAccessResult front,inside;
    error=smy_access_sweep(model,&hole.frame,mm,0,0,handle+2*gap,handle+2*gap,0.002,run+gap,1,&front);
    if(!error)error=smy_access_sweep(model,&hole.frame,mm,0,0,plug+2*gap,plug+2*gap,-run,0.002,1,&inside);
    if(!error){
        smy_access_report(L"塞规手柄接近区",&front,out);
        smy_access_report(L"塞规插入行程",&inside,out);
        part_text_add(out,L"塞规 %ls。\n",front.blocked||inside.blocked?L"取样发现干涉":
            front.unknown||inside.unknown?L"仍有位置待复核":L"取样通过孔口及插入行程");
    }
    return error;
}

static ProError smy_press_stud(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,t;
    ProError error=fab_thickness(model,&mm,&t);
    if(error)return error;
    SmyFastener rule={cmd->arg[6],cmd->arg[7],L"STUD"};
    error=smx_table(L"BWTL_SMT_FASTENERS.tsv",9,smy_fastener_row,&rule,out);
    if(error)return error;
    if(!rule.found||t<rule.grip_min||t>rule.grip_max)return PRO_TK_BAD_CONTEXT;
    FdTxn tx={0};
    error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT_SMT);
    PanelFrame frame={0};
    if(!error)error=pn_frame(&tx,cmd,&frame);
    double x=as_arg(cmd,8),y=as_arg(cmd,9),reach=as_arg(cmd,10),margin=as_arg(cmd,11);
    double d=(rule.hole_min+rule.hole_max)/2;
    SmyAccessResult sample={0};
    if(!error)error=smy_access_sweep(model,&frame,mm,x,y,d+2*margin,d+2*margin,-t-reach-margin,-t-0.002,1,&sample);
    if(!error&&(sample.blocked||sample.unknown))error=PRO_TK_BAD_CONTEXT;
    if(!error){
        PanelProfile profile={PN_CIRCLE,x,y,d,d};
        error=smx_openings_apply(model,cmd,&profile,1,rule.edge_min,0,out);
    }
    if(!error){
        SmxLine mark;
        ProMatrix sketch_frame;
        ProName name;
        swprintf_s(name,_countof(name),L"%ls_SIDE",cmd->arg[2]);
        am_world(&frame.placement,(x+d*0.6)/mm,y/mm,0,mark.a);
        am_world(&frame.placement,(x+d*0.85)/mm,y/mm,0,mark.b);
        error=smx_custom_sketch(model,name,cmd->arg[3],cmd->arg[5],smx_line_draw,&mark,sketch_frame);
    }
    if(error&&tx.root){
        wchar_t receipt[900];
        am_finish(&tx,error,1,receipt,_countof(receipt));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",receipt);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"压铆螺柱 %ls（%ls）底孔 %.8g mm，安装侧标识 %ls_SIDE，背侧突出 %.8g mm 的空间已按孔轴取样核对。未保存。\n",
        cmd->arg[6],rule.revision,d,cmd->arg[2],reach);
    return error;
}

static void smy_align_local(const ShopMouth *hole,const ProCsysdata *csys,double mm,ProVector point,ProVector direction){
    ProVector delta;
    ad_sub(hole->frame.placement.csys.origin,csys->origin,delta);
    const double *axis[]={csys->x_vector,csys->y_vector,csys->z_vector};
    for(int i=0;i<3;i++){
        point[i]=ad_dot(delta,axis[i])*mm;
        direction[i]=ad_dot(hole->frame.placement.csys.z_vector,axis[i]);
    }
}

static ProError smy_rivet_align(ProMdl model,const PartCommand *cmd,PartText *out){
    ProMdl other=NULL;
    int loaded=0;
    ProError error=ax_load_part(cmd->arg[2],&other,&loaded);
    ProMdl plates[]={model,other};
    ShopMouth holes[2]={0};
    ProCsysdata frames[2]={0};
    ProVector position[2],axis[2];
    double mm[2],thickness[2];
    for(int i=0;!error&&i<2;i++){
        int at=3+3*i;
        error=in_box_frame(plates[i],cmd->arg[at],&frames[i]);
        if(!error)error=smy_access_mouth(plates[i],cmd->arg[at+1],cmd->arg[at+2],&holes[i],&mm[i],&thickness[i]);
        if(!error)smy_align_local(&holes[i],&frames[i],mm[i],position[i],axis[i]);
    }
    if(!error&&(fabs(axis[0][2])<0.999999||fabs(axis[1][2])<0.999999||axis[0][2]*axis[1][2]>=0))error=PRO_TK_BAD_CONTEXT;
    if(!error){
        double dx=position[0][0]-position[1][0],dy=position[0][1]-position[1][1];
        double deviation=hypot(dx,dy),lo=as_arg(cmd,10),hi=as_arg(cmd,11),gun=as_arg(cmd,12);
        double low[2],high[2];
        for(int i=0;i<2;i++){
            double inner=position[i][2]-axis[i][2]*thickness[i];
            low[i]=fmin(position[i][2],inner);high[i]=fmax(position[i][2],inner);
        }
        double gap=fmax(low[0],low[1])-fmin(high[0],high[1]);
        int pass=deviation<=as_arg(cmd,9)&&hi>=lo&&gap>=-0.001&&gap<=as_arg(cmd,13)&&
            2*holes[0].radius>=lo&&2*holes[0].radius<=hi&&2*holes[1].radius>=lo&&2*holes[1].radius<=hi;
        SmyAccessResult approach;
        if(!error)error=smy_access_sweep(model,&holes[0].frame,mm[0],0,0,gun,gun,0.002,gun,1,&approach);
        if(!error){
            part_text_add(out,L"在指定的对应坐标系中，孔轴偏差 %.8g mm，双侧孔径 %.8g / %.8g mm，板厚合计 %.8g mm，面间隙 %.8g mm。孔系几何 %ls。\n",
                deviation,2*holes[0].radius,2*holes[1].radius,thickness[0]+thickness[1],gap,pass?L"符合":L"超限");
            smy_access_report(L"铆枪前方包络", &approach,out);
        }
    }
    if(loaded&&other){
        ProError erased=ProMdlErase(other);
        if(!error)error=erased;
    }
    return error;
}
