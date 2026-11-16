typedef struct {ProSurface face;ProVector center;double nearest;int edges;} AwPressRim;

static ProError aw_rim_edge(ProEdge edge,ProError status,ProAppData context){
    if(status)return status;
    AwPressRim *rim=context;
    ProEnttype type;
    ProVector first,last,difference,offset;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(!error)error=ProEdgeXyzdataEval(edge,0,first,NULL,NULL,NULL);
    if(!error)error=ProEdgeXyzdataEval(edge,1,last,NULL,NULL,NULL);
    if(error)return error;
    if(type!=PRO_ENT_LINE)return PRO_TK_UNSUPPORTED;
    ad_sub(last,first,difference);
    ad_sub(rim->center,first,offset);
    double squared=ad_dot(difference,difference);
    if(squared<=1e-12)return PRO_TK_BAD_CONTEXT;
    double projection=fmin(1,fmax(0,ad_dot(difference,offset)/squared));
    for(int i=0;i<3;i++)offset[i]-=projection*difference[i];
    rim->nearest=fmin(rim->nearest,sqrt(ad_dot(offset,offset)));
    rim->edges++;
    return PRO_TK_NO_ERROR;
}

static ProError aw_rim_contour(ProContour contour,ProError status,ProAppData context){
    if(status)return status;
    AwPressRim *rim=context;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    return error||traversal!=PRO_CONTOUR_TRAV_EXTERNAL?error:
        ProContourEdgeVisit(rim->face,contour,aw_rim_edge,NULL,context);
}

static ProError aw_press_margin(AxQualityFace *panel,const wchar_t *plane,const wchar_t *hole,double mm,double *margin){
    AxHardwareSite site={0};
    ProError error=ax_hw_site(&panel->owner,hole,plane,mm,&site);
    if(error)return error;
    if(!site.bore)return PRO_TK_BAD_CONTEXT;
    AwPressRim rim={panel->plane.handle,{0},INFINITY,0};
    memcpy(rim.center,site.contact,sizeof(ProVector));
    error=ProSurfaceContourVisit(rim.face,aw_rim_contour,NULL,&rim);
    if(error)return error;
    if(rim.edges<3||!isfinite(rim.nearest))return PRO_TK_INCOMPLETE;
    *margin=(rim.nearest-site.cylinder.radius)*mm;
    return PRO_TK_NO_ERROR;
}


static ProError aw_parallel_distance(ProMdl assembly,const PartCommand *cmd,int offset,
    double mm,double *distance,AxQualityFace *base,AxQualityFace *other){
    ProError error=ax_q_face(assembly,cmd->arg[offset],cmd->arg[offset+1],mm,base);
    if(!error)error=ax_q_face(assembly,cmd->arg[offset+2],cmd->arg[offset+3],mm,other);
    if(error)return error;
    if(ax_q_angle(base->plane.normal,other->plane.normal)>0.01)return PRO_TK_BAD_CONTEXT;
    ProVector delta;
    ad_sub(other->plane.point,base->plane.point,delta);
    *distance=ad_dot(delta,base->plane.normal)*mm;
    return PRO_TK_NO_ERROR;
}

static ProError aw_hardware(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    double separation;
    AxQualityFace first={0},second={0};
    ProError error=aw_parallel_distance(assembly,cmd,2,mm,&separation,&first,&second);
    if(error){
        part_text_add(out,L"未核实两个平行实体面，请核对组件路径和面编号。\n");
        return error;
    }
    const double thickness=fabs(separation);
    switch(kind){
    case AW_SCREW_LENGTH:{
        const double minimum=as_arg(cmd,6),maximum=as_arg(cmd,7);
        if(maximum<minimum)return PRO_TK_BAD_INPUTS;
        int best=-1,ties=0;
        double length=INFINITY;
        for(int i=8;i<cmd->count;i+=2){
            const double candidate=as_arg(cmd,i+1),reach=candidate-thickness;
            if(reach+1e-7<minimum||reach>maximum+1e-7)continue;
            if(candidate<length-1e-7){best=i;length=candidate;ties=1;}
            else if(fabs(candidate-length)<=1e-7)ties++;
        }
        part_text_add(out,L"实际夹持厚度 %.8g mm，可接受承压面至尖端长度 [%.8g, %.8g] mm。\n",thickness,thickness+minimum,thickness+maximum);
        if(best<0)part_text_add(out,L"所给公司库候选中没有满足范围的长度。\n");
        else{
            part_text_add(out,L"最短合格长度 %.8g mm，伸入 %.8g mm。",length,length-thickness);
            for(int i=8;i<cmd->count;i+=2)if(fabs(as_arg(cmd,i+1)-length)<=1e-7)part_text_add(out,L" %ls",cmd->arg[i]);
            part_text_add(out,L"。%ls\n",ties==1?L"候选唯一。":L"同长度有多个型号，需按螺纹和头型区分。");
        }
        return PRO_TK_NO_ERROR;
    }
    case AW_BLIND_BOTTOM:{
        const double screw=as_arg(cmd,6),engaged_depth=as_arg(cmd,7),insertable=as_arg(cmd,8);
        const double min_engagement=as_arg(cmd,9),min_clearance=as_arg(cmd,10);
        if(insertable<engaged_depth||min_engagement<=0)return PRO_TK_BAD_INPUTS;
        const double reach=screw-thickness,bottom=insertable-reach,engagement=fmax(0,fmin(reach,engaged_depth));
        part_text_add(out,L"夹层 %.8g mm，伸入 %.8g mm，预计孔底余量 %.8g mm，几何啮合 %.8g mm。\n",
            thickness,reach,bottom,engagement);
        part_text_add(out,L"%ls，%ls。\n",bottom<0?L"螺钉将顶底":bottom<min_clearance?L"孔底余量不足":L"孔底余量符合",
            engagement<min_engagement?L"啮合长度不足":L"啮合长度符合");
        return PRO_TK_NO_ERROR;
    }
    case AW_COUNTERSINK:{
        const double hole_angle=as_arg(cmd,6),head_angle=as_arg(cmd,7),plane_tol=as_arg(cmd,8),angle_tol=as_arg(cmd,9);
        const double diff=fabs(hole_angle-head_angle);
        part_text_add(out,L"螺钉头相对安装面 %ls %.8g mm，平齐允许 %.8g mm。角度差 %.8g°，允许 %.8g°。%ls。\n",
            separation>=0?L"高出":L"低于",thickness,plane_tol,diff,angle_tol,
            ax_q_within(thickness,plane_tol)&&ax_q_within(diff,angle_tol)?L"符合":L"不符合");
        return PRO_TK_NO_ERROR;
    }
    case AW_PRESS_FIT:{
        int bore=0;
        AxGeometry hole;
        ProVector ends[2];
        error=ax_hw_cylinder(&first.owner,cmd->arg[6],&hole,ends,&bore);
        if(error||!bore||!ax_same_occurrence(&first.owner,&second.owner))return error?error:PRO_TK_BAD_CONTEXT;
        ProMatrix pose;
        ProVector world_axis,world_point,delta;
        if((error=ax_device_pose(&first.owner,pose))||(error=ProVectorTrfEval(hole.direction,pose,world_axis))||
           (error=ProPntTrfEval(ends[0],pose,world_point)))return error;
        ad_sub(world_point,first.plane.point,delta);
        double approach=fabs(ad_dot(delta,first.plane.normal))*mm;
        if((error=ProPntTrfEval(ends[1],pose,world_point)))return error;
        ad_sub(world_point,first.plane.point,delta);
        approach=fmin(approach,fabs(ad_dot(delta,first.plane.normal))*mm);
        const double min_t=as_arg(cmd,7),max_t=as_arg(cmd,8),min_d=as_arg(cmd,9),max_d=as_arg(cmd,10),available=as_arg(cmd,11);
        if(max_t<min_t||max_d<min_d)return PRO_TK_BAD_INPUTS;
        const double diameter=2*hole.radius*mm;
        int side=approach<0.01&&ax_q_angle(world_axis,first.plane.normal)<0.01;
        part_text_add(out,L"首面孔口 %ls，板厚 %.8g mm（要求 %.8g～%.8g），孔径 %.8g mm（要求 %.8g～%.8g）。输入背面可用 %.8g mm。\n",
            side?L"已核对":L"未核实",thickness,min_t,max_t,diameter,min_d,max_d,available);
        const int fits=side&&thickness+1e-8>=min_t&&thickness<=max_t+1e-8&&diameter+1e-8>=min_d&&diameter<=max_d+1e-8;
        part_text_add(out,L"%ls。\n",fits?L"首面、板厚和孔径范围符合":L"板厚、孔径或首面条件不满足");
        if(cmd->count==12){
            part_text_add(out,L"需要规定最小外边距和所需背面突出净空，才能完成剩余检查。\n");
            return PRO_TK_NO_ERROR;
        }
        double margin;
        if((error=aw_press_margin(&first,cmd->arg[3],cmd->arg[6],mm,&margin))){
            part_text_add(out,L"外边距未核实。本方法适用于直线外轮廓。\n");
            return error;
        }
        const double required=as_arg(cmd,12),rear=as_arg(cmd,13);
        part_text_add(out,L"实测孔边至板外轮廓 %.8g mm（要求 ≥ %.8g），背面净空 %.8g mm（要求 ≥ %.8g）。%ls。\n",
            margin,required,available,rear,fits&&qa_at_least(margin,required)&&qa_at_least(available,rear)?L"工艺条件符合":L"工艺条件不符合");
        return PRO_TK_NO_ERROR;
    }
    case AW_RIVET_GRIP:{
        const double minimum=as_arg(cmd,6),maximum=as_arg(cmd,7),available=as_arg(cmd,8),required=as_arg(cmd,9);
        if(maximum<minimum||required<=0)return PRO_TK_BAD_INPUTS;
        part_text_add(out,L"夹层 %.8g mm（铆钉范围 %.8g～%.8g mm），输入的背面成形空间 %.8g mm（要求 %.8g mm），%ls。\n",
            thickness,minimum,maximum,available,required,thickness+1e-8>=minimum&&thickness<=maximum+1e-8&&available+1e-8>=required?L"符合":L"不符合");
        return PRO_TK_NO_ERROR;
    }
    case AW_ASBUILT:{
        const double sample=as_arg(cmd,6),nominal=as_arg(cmd,7),minus=as_arg(cmd,8),plus=as_arg(cmd,9),difference=sample-nominal;
        part_text_add(out,L"CAD 两平行面名义间距 %.8g mm，图纸名义 %.8g mm，样机实测 %.8g mm，图纸允许 [%.8g, %.8g] mm。\n",
            thickness,nominal,sample,nominal-minus,nominal+plus);
        if(fabs(thickness-nominal)>0.001)part_text_add(out,L"图纸名义与当前 CAD 相差 %.8g mm，需核对版本。\n",fabs(thickness-nominal));
        part_text_add(out,L"样机相对图纸名义偏差 %+.8g mm，%ls。\n",difference,difference>=-minus-1e-8&&difference<=plus+1e-8?L"在图纸公差内":L"超出图纸公差");
        return PRO_TK_NO_ERROR;
    }
    default:return PRO_TK_BAD_INPUTS;
    }
}
