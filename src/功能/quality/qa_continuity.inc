static ProError qa_continuity(ProMdl model,const PartCommand *cmd,double scale,PartText *out){
    FdRef ref={0};
    ProEdge edge;
    ProSurface faces[2];
    double max_gap=0,max_angle=0;
    double angle_limit=wcstod(cmd->arg[3],NULL),gap_limit=wcstod(cmd->arg[4],NULL);
    ProError error;
    if((error=fd_ref_resolve(model,cmd->arg[2],PRO_EDGE,&ref))||
       (error=ProGeomitemToEdge(&ref.item,&edge))||
       (error=ProEdgeNeighborsGet(edge,NULL,NULL,&faces[0],&faces[1])))goto done;
    if(!faces[0]||!faces[1]||faces[0]==faces[1]){error=PRO_TK_BAD_CONTEXT;goto done;}
    int ids[2];
    for(int i=0;i<2;i++)if((error=ProSurfaceIdGet(faces[i],&ids[i])))goto done;
    if(ids[0]==ids[1]){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<11;i++){
        ProVector edge_point,point[2],normal[2],gap;
        double t=(i+0.5)/11.0;
        if((error=ProEdgeXyzdataEval(edge,t,edge_point,NULL,NULL,NULL)))goto done;
        for(int j=0;j<2;j++){
            ProUvParam uv;
            if((error=ProSurfaceParamEval((ProSolid)model,faces[j],edge_point,uv))||
               (error=ProSurfaceXyzdataEval(faces[j],uv,point[j],NULL,NULL,normal[j])))goto done;
            if(!ad_unit(normal[j])){error=PRO_TK_BAD_CONTEXT;goto done;}
        }
        for(int j=0;j<3;j++)gap[j]=point[0][j]-point[1][j];
        max_gap=fmax(max_gap,scale*sqrt(ad_dot(gap,gap)));
        max_angle=fmax(max_angle,acos(fabs(qa_clamp(ad_dot(normal[0],normal[1]))))*180.0/AS_PI);
    }
    part_text_add(out,L"%ls 邻面 SURFACE:%d / SURFACE:%d，共11个边内采样点。\n"
        L"最大点距 %.9g mm（容差 %.9g mm），最大切平面夹角 %.9g°（容差 %.9g°）。采样判定：%ls。\n",
        cmd->arg[2],ids[0],ids[1],max_gap,gap_limit,max_angle,angle_limit,
        !qa_at_least(gap_limit,max_gap)?L"位置间隙超限":!qa_at_least(angle_limit,max_angle)?L"G0，切向超限":L"G0 与 G1 符合");
done:
    fd_ref_free(&ref);
    return error;
}
