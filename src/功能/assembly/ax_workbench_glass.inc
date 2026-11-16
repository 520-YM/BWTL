typedef struct {
    ProSurface face;
    ProMatrix pose;
    const AxDeviceFrame *reference;
    int required_edge,match,lines,arcs,radius_mismatch;
    double low[2],high[2],radius;
} AwRoundedProfile;

static ProError aw_round_edge(ProEdge edge,ProError status,ProAppData context){
    if(status)return status;
    AwRoundedProfile *shape=context;
    ProEnttype kind;
    int id;
    ProError error=ProEdgeTypeGet(edge,&kind);
    if(!error)error=ProEdgeIdGet(edge,&id);
    if(error)return error;
    shape->match|=id==shape->required_edge;
    if(kind!=PRO_ENT_LINE&&kind!=PRO_ENT_ARC){
        shape->radius_mismatch=1;
        return PRO_TK_NO_ERROR;
    }
    if(kind==PRO_ENT_ARC){
        ProGeomitemdata *data=NULL;
        if((error=ProEdgeDataGet(edge,&data)))return error;
        const ProCurvedata *curve=data->data.p_curve_data;
        double radius=curve&&curve->arc.type==PRO_ENT_ARC?curve->arc.radius:0;
        ProGeomitemdataFree(&data);
        double length;
        if((error=ProEdgeLengthEval(edge,&length)))return error;
        if(radius<=0||fabs(length/radius-AS_PI/2)>1e-6)shape->radius_mismatch=1;
        if(!shape->arcs)shape->radius=radius;
        else if(fabs(radius-shape->radius)>1e-6)shape->radius_mismatch=1;
        shape->arcs++;
    }else shape->lines++;
    double endpoints[2][2];
    for(int j=0;j<=8;j++){
        ProVector local,world;
        if((error=ProEdgeXyzdataEval(edge,j/8.0,local,NULL,NULL,NULL))||
           (error=ProPntTrfEval(local,shape->pose,world)))return error;
        for(int i=0;i<2;i++){
            const double position=ax_project(world,shape->reference,i);
            shape->low[i]=fmin(shape->low[i],position);shape->high[i]=fmax(shape->high[i],position);
            if(j==0||j==8)endpoints[j==8][i]=position;
        }
    }
    if(kind==PRO_ENT_LINE&&fabs(endpoints[0][0]-endpoints[1][0])>0.0001&&
       fabs(endpoints[0][1]-endpoints[1][1])>0.0001)shape->radius_mismatch=1;
    return PRO_TK_NO_ERROR;
}

static ProError aw_round_loop(ProContour contour,ProError status,ProAppData context){
    if(status)return status;
    AwRoundedProfile *target=context;
    if(target->match)return PRO_TK_NO_ERROR;
    AwRoundedProfile found=*target;
    ProError error=ProContourEdgeVisit(target->face,contour,aw_round_edge,NULL,&found);
    if(!error&&found.match)*target=found;
    return error;
}

static ProError aw_round_profile(ProMdl assembly,const wchar_t *id,const wchar_t *face,const wchar_t *edge,
    const AxDeviceFrame *frame,double mm,AwRoundedProfile *profile){
    AxQualityFace owning={0};
    ProError error=ax_q_face(assembly,id,face,mm,&owning);
    if(error)return error;
    if(ax_q_angle(owning.plane.normal,frame->axis[2])>0.001)return PRO_TK_BAD_CONTEXT;
    profile->face=owning.plane.handle;profile->reference=frame;
    profile->low[0]=profile->low[1]=INFINITY;profile->high[0]=profile->high[1]=-INFINITY;
    if(!part_integer(edge+5,&profile->required_edge))return PRO_TK_BAD_INPUTS;
    if((error=ax_device_pose(&owning.owner,profile->pose))||
       (error=ProSurfaceContourVisit(profile->face,aw_round_loop,NULL,profile)))return error;
    if(!profile->match||profile->radius_mismatch||profile->lines!=4||(profile->arcs!=0&&profile->arcs!=4))return PRO_TK_UNSUPPORTED;
    return PRO_TK_NO_ERROR;
}

static ProError aw_glass_corners(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame frame;
    ProError error=aw_frame_world(assembly,cmd->arg[2],cmd->arg[3],&frame);
    if(error)return error;
    AwRoundedProfile aperture={0},glass={0};
    if((error=aw_round_profile(assembly,cmd->arg[2],cmd->arg[4],cmd->arg[5],&frame,mm,&aperture))||
       (error=aw_round_profile(assembly,cmd->arg[6],cmd->arg[7],cmd->arg[8],&frame,mm,&glass)))return error;
    const double tolerance=as_arg(cmd,9),minimum=as_arg(cmd,10);
    const double ra=aperture.radius*mm,rg=glass.radius*mm;
    const double centers_a[]={(aperture.low[0]+aperture.high[0])*mm/2,(aperture.low[1]+aperture.high[1])*mm/2};
    const double centers_g[]={(glass.low[0]+glass.high[0])*mm/2,(glass.low[1]+glass.high[1])*mm/2};
    const double sizes_a[]={(aperture.high[0]-aperture.low[0])*mm/2-ra,(aperture.high[1]-aperture.low[1])*mm/2-ra};
    const double sizes_g[]={(glass.high[0]-glass.low[0])*mm/2-rg,(glass.high[1]-glass.low[1])*mm/2-rg};
    if(sizes_a[0]<0||sizes_a[1]<0||sizes_g[0]<0||sizes_g[1]<0)return PRO_TK_BAD_CONTEXT;
    const wchar_t *names[]={L"左下",L"右下",L"左上",L"右上"};
    int failed=0;
    for(int i=0;i<4;i++){
        const double dx=sizes_a[0]-sizes_g[0]+(i&1?1:-1)*(centers_a[0]-centers_g[0]);
        const double dy=sizes_a[1]-sizes_g[1]+(i&2?1:-1)*(centers_a[1]-centers_g[1]);
        const double clearance=ra-rg-tolerance+(dx<0&&dy<0?-hypot(dx,dy):fmin(dx,dy));
        int pass=qa_at_least(clearance,minimum);
        failed+=!pass;
        part_text_add(out,L"%ls角保守轮廓余量 %.8g mm（要求 ≥ %.8g mm），%ls。\n",names[i],clearance,minimum,pass?L"符合":L"角部余量不足");
    }
    part_text_add(out,L"窗口圆角 R%.8g，玻璃圆角 R%.8g，计入输入的外扩/收缩总公差 %.8g mm。%d 角不足。\n",
        ra,rg,tolerance,failed);
    return PRO_TK_NO_ERROR;
}

static ProError aw_glue_space(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    const double min_width=as_arg(cmd,2),min_depth=as_arg(cmd,3),nozzle=as_arg(cmd,4);
    int failed=0,unknown=0;
    for(int i=5;i<cmd->count;i+=8){
        double width,depth;
        AxQualityFace a={0},b={0},c={0},d={0};
        ProError error=aw_parallel_distance(assembly,cmd,i,mm,&width,&a,&b);
        if(!error)error=aw_parallel_distance(assembly,cmd,i+4,mm,&depth,&c,&d);
        if(error||fabs(ad_dot(a.plane.normal,c.plane.normal))>0.01){
            unknown++;
            part_text_add(out,L"胶槽截面 %d 的宽面与深面无法核实为两组正交平行面。\n",(i-5)/8+1);continue;
        }
        width=fabs(width);depth=fabs(depth);
        int pass=qa_at_least(width,fmax(min_width,nozzle))&&qa_at_least(depth,min_depth);
        failed+=!pass;
        part_text_add(out,L"胶槽截面 %d：净宽 %.8g mm，净深 %.8g mm，胶嘴直径 %.8g mm。%ls。\n",
            (i-5)/8+1,width,depth,nozzle,pass?L"截面通过":L"胶槽或胶嘴空间不足");
    }
    part_text_add(out,L"指定胶路截面中 %d 处不足，%d 处未核实。转角运动及段间连续性按样机或额外截面复核。\n",failed,unknown);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
