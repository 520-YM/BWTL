static ProError aw_air_pair(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame inlet,outlet;
    ProError error=aw_frame_world(assembly,cmd->arg[2],cmd->arg[3],&outlet);
    if(!error)error=aw_frame_world(assembly,cmd->arg[5],cmd->arg[6],&inlet);
    if(error)return error;
    ProVector direction,to;
    const int x=ax_axis_index(cmd->arg[4]+1),y=ax_axis_index(cmd->arg[7]+1);
    const double s_out=cmd->arg[4][0]==L'+'?1:-1,s_in=cmd->arg[7][0]==L'+'?1:-1;
    for(int i=0;i<3;i++)direction[i]=outlet.axis[x][i]*s_out;
    ad_sub(inlet.origin,outlet.origin,to);
    const double gap=sqrt(ad_dot(to,to))*mm;
    if(gap<0.001||!ad_unit(to))return PRO_TK_BAD_CONTEXT;
    const double away=acos(qa_clamp(ad_dot(direction,to)))*180/AS_PI;
    const double facing=acos(qa_clamp(-ad_dot(to,inlet.axis[y])*s_in))*180/AS_PI;
    const double limit=as_arg(cmd,8),cone=as_arg(cmd,9);
    const int risk=gap<limit&&away<=cone&&facing<=cone;
    part_text_add(out,L"排风到进风 %.8g mm，排风轴偏离连线 %.8g°，进风外向轴偏离回指方向 %.8g°。\n",gap,away,facing);
    part_text_add(out,L"距离阈值 %.8g mm、方向半角 %.8g° 下，%ls。依据安装位置和方向筛查，实际回流待风道或样机验证。\n",
        limit,cone,risk?L"有直接回流风险":L"未触发所选方向和距离条件");
    return PRO_TK_NO_ERROR;
}

static ProError aw_heat_zone(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxQualityFace hot={0};
    AxDeviceFrame direction={0};
    ProError error=ax_q_face(assembly,cmd->arg[2],cmd->arg[3],mm,&hot);
    if(error)return error;
    memcpy(direction.origin,hot.plane.point,sizeof(ProVector));
    ax_hw_basis(hot.plane.normal,direction.axis);
    double a[3],b[3];
    if((error=aw_project_face(&hot,&direction,mm,a,b)))return error;
    const double depth=as_arg(cmd,4),padding=as_arg(cmd,5);
    int overlaps=0;
    for(int i=6;i<cmd->count;i++){
        ProOutline bounds;
        if((error=aw_part_bounds(assembly,cmd->arg[i],&direction,mm,bounds)))return error;
        int inside=bounds[1][2]*mm>0.001&&bounds[0][2]*mm<depth;
        for(int j=0;j<2;j++)if(bounds[1][j]*mm<a[j]-padding||bounds[0][j]*mm>b[j]+padding)inside=0;
        overlaps+=inside;
        part_text_add(out,L"%ls 在散热面法向范围 %.8g～%.8g mm，%ls。\n",cmd->arg[i],
            bounds[0][2]*mm,bounds[1][2]*mm,inside?L"侵入散热禁放包络":L"未侵入指定包络");
    }
    part_text_add(out,L"按给定投影外扩 %.8g mm、留空深度 %.8g mm，%d 个实例需调整。依据实际外廓筛查。\n",padding,depth,overlaps);
    return PRO_TK_NO_ERROR;
}

static ProError aw_no_pressure(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent zone={0};
    ProSelection protected_zone=NULL;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&zone);
    if(!error)error=ax_part_selection(&zone,&protected_zone);
    if(error)return error;
    const double required=as_arg(cmd,3);
    int failed=0;
    for(int i=4;i<cmd->count;i++){
        AxComponent object={0};
        ProSelection selected=NULL;
        error=ax_hw_occurrence(assembly,cmd->arg[i],mm,&object);
        if(!error)error=ax_part_selection(&object,&selected);
        double distance=0;
        int collision=0;
        if(!error)error=ax_motion_sample_fit(protected_zone,selected,mm,&distance,&collision);
        if(selected)ProSelectionFree(&selected);
        if(error)break;
        int pass=!collision&&qa_at_least(distance,required);
        failed+=!pass;
        part_text_add(out,L"%ls 距指定屏幕禁压实体包络 %.8g mm。%ls。\n",cmd->arg[i],distance,
            collision?L"存在压入干涉":pass?L"留空符合":L"留空不足");
    }
    ProSelectionFree(&protected_zone);
    if(!error)part_text_add(out,L"检测到 %d 个需要复核的接触或干涉部件。限指定禁压区及给定装配姿态。\n",failed);
    return error;
}

static ProError aw_cast_drop(ProMdl assembly,const wchar_t *obstacle,const double world[3],const double vector[3],double mm,
                            double length,double *hit,int *surface){
    AxComponent owner={0};
    ProMatrix pose,inverse;
    ProError error=ax_hw_occurrence(assembly,obstacle,mm,&owner);
    if(!error)error=ax_device_pose(&owner,pose);
    if(error)return error;
    ax_device_inverse(pose,inverse);
    ProRay ray;
    if((error=ProPntTrfEval((double *)world,inverse,ray.start_point))||
       (error=ProVectorTrfEval((double *)vector,inverse,ray.dir_vector)))return error;
    if(!ad_unit(ray.dir_vector))return PRO_TK_BAD_CONTEXT;
    QualityRay crosses={0};
    if((error=qa_ray(owner.model,&ray,0.001/mm,&crosses)))return error;
    *hit=INFINITY;*surface=0;
    for(int j=0;j<crosses.count;j++){
        double depth=crosses.crossing[j].depth*mm;
        if(crosses.crossing[j].entering&&depth>0.001&&depth<=length){
            *hit=depth;*surface=crosses.crossing[j].surface_id;break;
        }
    }
    return PRO_TK_NO_ERROR;
}

static ProError aw_rays_audit(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxDeviceFrame start;
    ProError error=aw_frame_world(assembly,cmd->arg[2],cmd->arg[3],&start);
    if(error)return error;
    const int axis=ax_axis_index(cmd->arg[4]+1);
    const double sense=cmd->arg[4][0]==L'+'?1:-1;
    ProVector vector;
    for(int i=0;i<3;i++)vector[i]=start.axis[axis][i]*sense;
    double limit=as_arg(cmd,5),first=INFINITY;
    int blocked=-1,surface=0,unread=0;
    for(int i=6;i<cmd->count;i++){
        if(!wcscmp(cmd->arg[i],cmd->arg[2]))return PRO_TK_BAD_INPUTS;
        double hit;
        int face;
        error=aw_cast_drop(assembly,cmd->arg[i],start.origin,vector,mm,limit,&hit,&face);
        if(error){
            part_text_add(out,L"%ls 的射线几何未核实（Creo %d）。\n",cmd->arg[i],error);
            unread++;continue;
        }
        if(hit<first){first=hit;blocked=i;surface=face;}
    }
    if(blocked>=0){
        const wchar_t *verb=kind==AW_LABEL_VIEW?L"标牌视线受遮挡":
            kind==AW_WATER_PATH?L"垂直滴水路径命中组件":L"释放后路径命中组件";
        part_text_add(out,L"%ls。最先碰到 %ls SURFACE:%d，距离起点 %.8g mm。\n",verb,cmd->arg[blocked],surface,first);
    }else part_text_add(out,L"在 %.8g mm 内，所选可读障碍中未检出交点。\n",limit);
    part_text_add(out,L"使用指定点和方向的直线模型，未检出不代表其他方向或姿态通过。未核实 %d 个组件。\n",unread);
    return unread?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
