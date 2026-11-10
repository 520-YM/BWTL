static int cm_recovery_uncertain;

static ProError cm_tip_for_wall(ProMdl model,const wchar_t *wall_name,const wchar_t *edge_token,CabinetEdge *tip){
    ProFeature wall;
    ProFeattype type;
    ProError error=cm_feature_find(model,wall_name,&wall);
    if(!error)error=ProFeatureTypeGet(&wall,&type);
    if(!error&&type!=PRO_FEAT_WALL)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=cm_edge_get(model,edge_token,tip);
    if(!error&&tip->owner!=wall.id)error=PRO_TK_BAD_CONTEXT;
    if(error)return error;
    CabinetEdge actual;
    error=cm_wall_tip(model,&wall,tip,&actual);
    return error?error:actual.id==tip->id?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError cm_tip_cut(FdTxn *tx,const CabinetEdge *tip,int end_side,double amount,double retained,
    double *cut_height){
    CabinetSpan across;
    ProDimension thickness_dim;
    double thickness;
    ProError error=cm_face_span(tx->model,tip->face,tip->inward,&across);
    if(!error)error=ProSmtPartThicknessGet((ProPart)tx->model,&thickness_dim);
    if(!error)error=ProDimensionValueGet(&thickness_dim,&thickness);
    if(error)return error;
    double depth=(across.max-across.min)/tx->scale-retained;
    double width=tip->length/tx->scale;
    if(depth<0.1||amount>=width-0.1||thickness<=0)return PRO_TK_BAD_INPUTS;
    PanelFrame frame={0};
    ProCsysdata *csys=&frame.placement.csys;
    memcpy(csys->origin,end_side?tip->b:tip->a,sizeof(ProVector));
    memcpy(csys->y_vector,tip->inward,sizeof(ProVector));
    memcpy(csys->z_vector,tip->normal,sizeof(ProVector));
    for(int i=0;i<3;i++){
        csys->x_vector[i]=end_side?-tip->axis[i]:tip->axis[i];
        frame.inward[i]=-tip->normal[i];
    }
    frame.face_id=tip->face;
    double plate_thickness=thickness/tx->scale;
    AsPoint inside[]={{amount/2,depth/2},{amount*0.15,depth*0.15},{amount*0.85,depth*0.15},
        {amount*0.15,depth*0.85},{amount*0.85,depth*0.85}};
    AsPoint preserved[]={{(amount+width)/2,depth/2},{amount/2,depth+retained/2}};
    double overhang=fmin(0.05,fmin(amount,depth)/4);
    AsPoint outside[]={{-overhang/2,depth/2},{amount/2,-overhang/2}};
    for(int i=0;i<_countof(inside);i++)
        if((error=pn_probe(tx,&frame,inside[i],0,plate_thickness,1)))return error;
    for(int i=0;i<_countof(outside);i++)
        if((error=pn_probe(tx,&frame,outside[i],0,0,0)))return error;
    for(int i=0;i<_countof(preserved);i++)
        if((error=pn_probe(tx,&frame,preserved[i],0,plate_thickness,1)))return error;

    ShOrient orient={0};
    memcpy(orient.normal,tip->normal,sizeof(ProVector));
    error=ProSolidFeatVisit((ProSolid)tx->model,sh_orient_plane,NULL,&orient);
    if(error!=PRO_TK_E_FOUND)return error?error:PRO_TK_E_NOT_FOUND;
    ProName sketch;
    ProMatrix sketch_frame;
    PartCommand sketch_cmd={0};
    PanelProfile cut=pn_rect((amount-overhang)/2,(depth-overhang)/2,amount+overhang,depth+overhang);
    swprintf_s(sketch,PRO_NAME_SIZE,L"%.22ls_SK",tx->name);
    swprintf_s(sketch_cmd.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",tip->face);
    swprintf_s(sketch_cmd.arg[5],PART_TOKEN_MAX,L"PLANE:%ls",orient.name);
    if((error=pn_sketch(tx->model,&sketch_cmd,&frame,sketch,&cut,1,1,sketch_frame))||
       (error=pn_cut(tx,&frame,tx->name,sketch,sketch_frame,plate_thickness,1))||
       (error=md_expected_volume(tx,-amount*depth*plate_thickness)))return error;
    for(int i=0;i<_countof(inside);i++)
        if((error=pn_probe(tx,&frame,inside[i],0,0,0)))return error;
    for(int i=0;i<_countof(preserved);i++)
        if((error=pn_probe(tx,&frame,preserved[i],0,plate_thickness,0)))return error;
    AsPoint boundary_cut={amount-fmin(0.01,amount/4),depth/2};
    AsPoint boundary_keep={amount+fmin(0.01,(width-amount)/4),depth/2};
    if((error=pn_probe(tx,&frame,boundary_cut,0,0,0))||
       (error=pn_probe(tx,&frame,boundary_keep,0,plate_thickness,0)))return error;
    *cut_height=depth;
    return PRO_TK_NO_ERROR;
}

static ProError cm_gap(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetEdge tip;
    FdRef adjacent={0};
    ProVector point,normal,witness,projected;
    ProSurface other;
    ProUvParam uv;
    ProUvStatus location;
    double requested=as_arg(cmd,5)*tx->scale;
    double retained=as_arg(cmd,6),depth;
    ProError error=cm_tip_for_wall(tx->model,cmd->arg[2],cmd->arg[3],&tip);
    if(!error)error=md_solid_face(tx,cmd->arg[4],&adjacent);
    if(!error)error=fd_plane_data(&adjacent.item,point,normal);
    if(!error)error=ProGeomitemToSurface(&adjacent.item,&other);
    if(error)goto done;
    if(!ad_unit(normal)||fabs(ad_dot(normal,tip.axis))<1-1e-8){error=PRO_TK_BAD_CONTEXT;goto done;}
    double a=ad_dot(tip.a,tip.axis),b=ad_dot(tip.b,tip.axis),plane=ad_dot(point,tip.axis);
    double gap=fmax(plane-b,a-plane);
    if(gap<0.1*tx->scale||requested<=gap+0.0001*tx->scale){error=PRO_TK_BAD_CONTEXT;goto done;}
    memcpy(witness,plane>=b?tip.b:tip.a,sizeof(witness));
    cm_point(witness,tip.inward,0.1*tx->scale,witness);
    cm_point(witness,tip.axis,plane-ad_dot(witness,tip.axis),projected);
    if((error=ProSurfaceParamEval((ProSolid)tx->model,other,projected,uv))||
       (error=ProSurfaceUvpntVerify((ProSolid)tx->model,other,uv,&location)))goto done;
    if(location==PRO_UV_OUTSIDE){error=PRO_TK_BAD_CONTEXT;goto done;}
    error=cm_tip_cut(tx,&tip,plane>=b,(requested-gap)/tx->scale,retained,&depth);
    if(!error)part_text_add(out,L"相邻翻边的平直段缝隙已从 %.8g 调到 %.8g mm，切口深 %.8g mm，保留折弯根部。",
        gap/tx->scale,requested/tx->scale,depth);
done:
    fd_ref_free(&adjacent);
    return error;
}
