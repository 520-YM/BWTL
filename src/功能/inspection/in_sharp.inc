static int in_edge_inside(const Inspection *inspect,const InspectionPlane *face,const ProVector p,
    const ProVector tangent,ProVector inward){
    ad_cross(face->normal,tangent,inward);
    if(!ad_unit(inward))return 0;
    double steps[]={0.05,0.015,0.003};
    for(int i=0;i<_countof(steps);i++){
        ProVector a,b;
        cm_point(p,inward,steps[i]/inspect->mm,a);
        cm_point(p,inward,-steps[i]/inspect->mm,b);
        int forward=in_on_plane(inspect->model,face,a),backward=in_on_plane(inspect->model,face,b);
        if(forward==backward)continue;
        if(backward)for(int j=0;j<3;j++)inward[j]*=-1;
        return 1;
    }
    return 0;
}

static ProError in_sharp_edge(const Inspection *inspect,const wchar_t *token,double *angle,int *convex){
    ProEdge edge;
    ProEnttype type;
    ProSurface faces[2];
    InspectionPlane planes[2];
    ProVector point,tangent,inward[2];
    ProError error=in_edge(inspect->model,token,&edge);
    if(!error)error=ProEdgeTypeGet(edge,&type);
    if(error||type!=PRO_ENT_LINE)return error?error:PRO_TK_INVALID_TYPE;
    if((error=ProEdgeNeighborsGet(edge,NULL,NULL,&faces[0],&faces[1]))||
       (error=ProEdgeXyzdataEval(edge,0.5,point,tangent,NULL,NULL)))return error;
    if(!ad_unit(tangent))return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<2;i++){
        if((error=in_plane_read(inspect->model,faces[i],&planes[i])))return error;
        if(!in_edge_inside(inspect,&planes[i],point,tangent,inward[i]))return PRO_TK_BAD_CONTEXT;
    }
    double d=ad_dot(planes[0].normal,planes[1].normal);
    double a=ad_dot(inward[0],planes[1].normal),b=ad_dot(inward[1],planes[0].normal);
    if(fabs(a)<1e-6||fabs(b)<1e-6||(a<0)!=(b<0))return PRO_TK_BAD_CONTEXT;
    *angle=180-acos(qa_clamp(d))*180/AS_PI;
    *convex=a<0;
    return PRO_TK_NO_ERROR;
}

static ProError in_sharp(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    double threshold=as_arg(cmd,2);
    int found=0,pending=0,marked=0;
    for(int i=3;i<cmd->count;i++){
        double angle;
        int convex;
        ProError error=in_sharp_edge(inspect,cmd->arg[i],&angle,&convex);
        if(error){
            part_text_add(out,L"%ls 无法核实为两平面相交直边，待复核。\n",cmd->arg[i]);
            pending++;
            continue;
        }
        int alert=convex&&angle<=threshold+1e-7;
        found++;
        marked+=alert;
        part_text_add(out,L"%ls %ls夹角 %.8g°，%ls。\n",cmd->arg[i],convex?L"外凸材料":L"内凹开口",angle,
            alert?L"列为外凸尖角":convex?L"高于尖角阈值":L"内角");
    }
    part_text_add(out,L"已核对 %d 条，标出 %d 条外凸尖角，待复核 %d 条。",found,marked,pending);
    return PRO_TK_NO_ERROR;
}
