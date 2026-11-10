typedef struct {InspectionPlane wall;ProVector a,b,axis;double angle;} InspectionBend;

static ProError in_bend_geometry(const Inspection *inspect,const wchar_t *token,InspectionBend *bend){
    ProEdge edge;
    ProEnttype type;
    ProSurface faces[2];
    ProError error=in_edge(inspect->model,token,&edge);
    if(!error)error=ProEdgeTypeGet(edge,&type);
    if(error||type!=PRO_ENT_LINE)return error?error:PRO_TK_BAD_CONTEXT;
    if((error=ProEdgeNeighborsGet(edge,NULL,NULL,&faces[0],&faces[1])))return error;
    for(int i=0;i<2;i++){
        ProSrftype curve;
        ProSmtSurfType side;
        if((error=ProSurfaceTypeGet(faces[i],&curve)))return error;
        if(curve!=PRO_SRF_CYL)continue;
        if((error=ProSmtSurfaceTypeGet((ProPart)inspect->model,faces[i],&side)))return error;
        if(side!=PRO_SMT_SURF_FACE&&side!=PRO_SMT_SURF_OFFSET)continue;
        double radius,developed;
        ProBool inside;
        ProSmtDvlLenCalcInfo calculation;
        error=ProSmtBendsrfInfoGet((ProPart)inspect->model,faces[i],&radius,&inside,&bend->angle,&developed,&calculation);
        if(error)return error;
        if((error=in_plane_read(inspect->model,faces[1-i],&bend->wall))||
           (error=ProEdgeXyzdataEval(edge,0,bend->a,NULL,NULL,NULL))||
           (error=ProEdgeXyzdataEval(edge,1,bend->b,NULL,NULL,NULL)))return error;
        ad_sub(bend->b,bend->a,bend->axis);
        return ad_unit(bend->axis)&&fabs(bend->angle)>0.01?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_BAD_CONTEXT;
}

static ProError in_bend_mouth(const Inspection *inspect,const InspectionBend *bend,const wchar_t *token,
    ProVector center,double *radius){
    FdRef ref={0};
    QualityBore bore;
    ProSurface surface;
    ProError error=qa_bore(inspect->model,token,&ref,&bore);
    if(!error)error=ProGeomitemToSurface(&ref.item,&surface);
    if(!error&&fabs(ad_dot(bore.axis,bend->wall.normal))<1-1e-8)error=PRO_TK_BAD_CONTEXT;
    if(!error){
        ProVector delta,y;
        ad_sub(bend->wall.point,bore.origin,delta);
        cm_point(bore.origin,bend->wall.normal,ad_dot(delta,bend->wall.normal),center);
        ad_cross(bend->wall.normal,bend->axis,y);
        if(!ad_unit(y)){error=PRO_TK_BAD_CONTEXT;goto done;}
        int on_selected_wall=0;
        double epsilon=fmin(bore.radius*0.03,0.025/inspect->mm);
        for(int i=0;i<8;i++){
            ProVector radial,a,b,wall;
            double angle=(i+0.25)*AS_PI/4;
            for(int j=0;j<3;j++)radial[j]=bend->axis[j]*cos(angle)+y[j]*sin(angle);
            cm_point(center,radial,bore.radius-epsilon,a);
            cm_point(center,radial,bore.radius+epsilon,b);
            if(in_on_plane(inspect->model,&bend->wall,a)||!in_on_plane(inspect->model,&bend->wall,b)){
                error=PRO_TK_BAD_CONTEXT;
                goto done;
            }
            cm_point(center,radial,bore.radius,wall);
            cm_point(wall,bend->wall.normal,-epsilon,wall);
            ProUvParam uv;
            ProUvStatus inside;
            if(!ProSurfaceParamEval((ProSolid)inspect->model,surface,wall,uv)&&
               !ProSurfaceUvpntVerify((ProSolid)inspect->model,surface,uv,&inside)&&inside==PRO_UV_INSIDE)
                on_selected_wall=1;
        }
        if(!on_selected_wall)error=PRO_TK_BAD_CONTEXT;
        else *radius=bore.radius*inspect->mm;
    }
done:
    fd_ref_free(&ref);
    return error;
}

static ProError in_bend(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    InspectionBend bend;
    int face;
    part_integer(cmd->arg[2]+8,&face);
    ProError error=in_bend_geometry(inspect,cmd->arg[4],&bend);
    if(error||bend.wall.id!=face)return error?error:PRO_TK_BAD_CONTEXT;
    ProVector hole;
    double radius;
    if((error=in_bend_mouth(inspect,&bend,cmd->arg[3],hole,&radius)))return error;
    double center=in_segment_distance(hole,bend.a,bend.b)*inspect->mm;
    double gap=center-radius,limit=as_arg(cmd,5);
    part_text_add(out,L"孔与折弯距离%ls。孔心到同一板面上实际折弯切线边 %.8g mm，扣除孔半径后净距 %.8g mm，要求至少 %.8g mm。",
        qa_at_least(gap,limit)?L"符合":L"低于要求",center,gap,limit);
    return PRO_TK_NO_ERROR;
}
