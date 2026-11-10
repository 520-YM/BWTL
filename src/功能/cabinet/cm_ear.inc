static ProError cm_ear_hole(FdTxn *tx,ProFeature *ear,const CabinetEdge *base,double diameter){
    CabinetEdge tip;
    CabinetFace face;
    CabinetSpan span;
    ProVector center;
    ProDimension dimension;
    double thickness;
    ProError error=cm_wall_face(ear,&face);
    if(!error)error=cm_wall_tip(tx->model,ear,base,&tip);
    if(!error)error=cm_face_span(tx->model,face.id,tip.inward,&span);
    if(!error)error=ProSmtPartThicknessGet((ProPart)tx->model,&dimension);
    if(!error)error=ProDimensionValueGet(&dimension,&thickness);
    if(error)return error;
    if(diameter*tx->scale>=fmin(tip.length,span.max-span.min)-0.1*tx->scale)return PRO_TK_BAD_INPUTS;
    cm_point(tip.a,tip.axis,tip.length/2,center);
    cm_point(center,tip.inward,(span.max-span.min)/2,center);
    PanelFrame panel={0};
    ProCsysdata *csys=&panel.placement.csys;
    memcpy(csys->origin,center,sizeof(ProVector));
    memcpy(csys->x_vector,tip.axis,sizeof(ProVector));
    memcpy(csys->z_vector,face.normal,sizeof(ProVector));
    ad_cross(csys->z_vector,csys->x_vector,csys->y_vector);
    for(int i=0;i<3;i++)panel.inward[i]=-face.normal[i];
    panel.face_id=face.id;
    PanelPlan plan={0};
    pn_circle(&plan,0,0,diameter);
    if((error=pn_profile_probes(tx,&panel,&plan.through[0],thickness/tx->scale,0)))return error;
    ShOrient orient={0};
    memcpy(orient.normal,face.normal,sizeof(ProVector));
    error=ProSolidFeatVisit((ProSolid)tx->model,sh_orient_plane,NULL,&orient);
    if(error!=PRO_TK_E_FOUND)return error?error:PRO_TK_E_NOT_FOUND;
    PartCommand sketch_cmd={0};
    ProName sketch,hole;
    ProMatrix sketch_frame;
    ProMassProperty before,after;
    swprintf_s(sketch,PRO_NAME_SIZE,L"%ls_HSK",tx->name);
    swprintf_s(hole,PRO_NAME_SIZE,L"%ls_HOLE",tx->name);
    swprintf_s(sketch_cmd.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",face.id);
    swprintf_s(sketch_cmd.arg[5],PART_TOKEN_MAX,L"PLANE:%ls",orient.name);
    if((error=qa_geometry_mass(tx->model,&before))||
       (error=pn_sketch(tx->model,&sketch_cmd,&panel,sketch,plan.through,1,1,sketch_frame))||
       (error=pn_cut(tx,&panel,hole,sketch,sketch_frame,thickness/tx->scale,1))||
       (error=pn_profile_probes(tx,&panel,&plan.through[0],thickness/tx->scale,1))||
       (error=qa_geometry_mass(tx->model,&after)))return error;
    return part_close(before.volume-after.volume,pn_area(&plan.through[0])*tx->scale*tx->scale*thickness)?
        PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError cm_ear(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetEdge base;
    ProFeature ear;
    double from=as_arg(cmd,4),to=as_arg(cmd,5),diameter=as_arg(cmd,9);
    ProError error=cm_edge_get(tx->model,cmd->arg[3],&base);
    if(!error)error=cm_attach(tx,tx->name,&base,from*tx->scale,to*tx->scale,as_arg(cmd,6),90,as_arg(cmd,7),cmd->arg[8],&ear);
    if(!error&&diameter)error=cm_ear_hole(tx,&ear,&base,diameter);
    if(!error){
        part_text_add(out,L"已生成宽 %.8g mm 的板边折弯挂耳。",to-from);
        if(diameter)part_text_add(out,L"中心固定孔直径 %.8g mm，已核对贯穿。",diameter);
    }
    return error;
}
