static AsPoint as_reflect(AsPoint p,AsLine axis,AsPoint unit){
    AsPoint relative=as_minus(p,axis.a);
    return as_plus(axis.a,as_minus(as_mul(unit,2*as_dot2(relative,unit)),relative));
}

static ProError as_mirror_entity(ProSection section,int id,AsLine axis,AsPoint unit,int *created){
    Pro2dEntdef *entity=NULL;
    ProError e=as_regular(section,id,&entity);
    if(e)return e;
    if(entity->type==PRO_2D_LINE){
        Pro2dLinedef *line=(Pro2dLinedef *)entity;
        return as_add_line(section,as_reflect(as_p(line->end1),axis,unit),as_reflect(as_p(line->end2),axis,unit),created);
    }
    if(entity->type==PRO_2D_CIRCLE){
        Pro2dCircledef circle=*(Pro2dCircledef *)entity;
        as_xy(circle.center,as_reflect(as_p(circle.center),axis,unit));
        return ProSectionEntityAdd(section,(Pro2dEntdef *)&circle,created);
    }
    if(entity->type==PRO_2D_ARC){
        Pro2dArcdef arc=*(Pro2dArcdef *)entity;
        double angle=2*atan2(unit.y,unit.x),span=arc.end_angle-arc.start_angle;
        if(span<0)span+=2*AS_PI;
        return as_add_arc(section,as_reflect(as_p(arc.center),axis,unit),arc.radius,angle-arc.end_angle,angle-arc.end_angle+span,created);
    }
    if(entity->type==PRO_2D_CONIC){
        Pro2dConicdef conic=*(Pro2dConicdef *)entity;
        as_xy(conic.first_end_point,as_reflect(as_p(conic.first_end_point),axis,unit));
        as_xy(conic.second_end_point,as_reflect(as_p(conic.second_end_point),axis,unit));
        as_xy(conic.shoulder_point,as_reflect(as_p(conic.shoulder_point),axis,unit));
        return ProSectionEntityAdd(section,(Pro2dEntdef *)&conic,created);
    }
    if(entity->type==PRO_2D_SPLINE){
        Pro2dSplinedef spline=*(Pro2dSplinedef *)entity;
        Pro2dPnt reflected[64];
        double angle=2*atan2(unit.y,unit.x);
        if(spline.n_points>_countof(reflected))return PRO_TK_UNSUPPORTED;
        for(unsigned int i=0;i<spline.n_points;i++)as_xy(reflected[i],as_reflect(as_p(spline.point_arr[i]),axis,unit));
        spline.point_arr=reflected;spline.start_tang_angle=angle-spline.start_tang_angle;spline.end_tang_angle=angle-spline.end_tang_angle;
        return ProSectionEntityAdd(section,(Pro2dEntdef *)&spline,created);
    }
    if(entity->type==PRO_2D_B_SPLINE){
        Pro2dBSplinedef *spline=(Pro2dBSplinedef *)entity;
        Pro2dPnt reflected[64];
        if(spline->num_c_points<2||spline->num_c_points>_countof(reflected)||
           spline->num_knots!=spline->num_c_points+spline->degree+1)return PRO_TK_UNSUPPORTED;
        for(int i=0;i<spline->num_c_points;i++){
            double weight=spline->weights?spline->weights[i]:1;
            if(!isfinite(weight)||weight<=0)return PRO_TK_UNSUPPORTED;
            as_xy(reflected[i],as_mul(as_reflect(as_mul(as_p(spline->c_pnts[i]),1/weight),axis,unit),weight));
        }
        return as_add_bspline(section,spline->degree,reflected,spline->num_c_points,spline->params,spline->weights,created);
    }
    return PRO_TK_UNSUPPORTED;
}

static ProError as_mirror(ProSection section,const PartCommand *cmd,double scale,AsEdit *edit){
    Pro2dEntdef *entity=NULL;
    Pro2dLinedef *def;
    AsLine axis;
    AsPoint unit;
    double length;
    int i;
    ProError e=ProSectionEntityGet(section,as_id(cmd,3),&entity);
    if(e)return e;
    if(entity->type!=PRO_2D_LINE&&entity->type!=PRO_2D_CENTER_LINE)return PRO_TK_INVALID_TYPE;
    def=(Pro2dLinedef *)entity;axis.a=as_p(def->end1);axis.b=as_p(def->end2);
    length=as_dist(axis.a,axis.b);
    if(length<=1e-5*scale)return PRO_TK_BAD_INPUTS;
    unit=as_mul(as_direction(axis),1/length);edit->count=cmd->count-4;
    for(i=0;i<edit->count;i++)if((e=as_mirror_entity(section,as_id(cmd,i+4),axis,unit,&edit->ids[i])))return e;
    return PRO_TK_NO_ERROR;
}
