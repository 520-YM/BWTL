static double cm_plane_offset(const CabinetFace *face,const double *point){
    ProVector delta;
    ad_sub(face->point,point,delta);
    return fabs(ad_dot(delta,face->normal));
}

static ProError cm_resize_wall(FdTxn *tx,ProFeature *wall,double height){
    FdTxn edit={0};
    edit.feature=*wall;
    edit.scale=tx->scale;
    ProError error=ProArrayAlloc(1,sizeof(*edit.options),1,(ProArray *)&edit.options);
    if(!error)error=sh_profile_resize(&edit,height,0);
    if(edit.options)ProArrayFree((ProArray *)&edit.options);
    if(!error)error=part_regenerate(tx->model);
    return error;
}

static ProError cm_z(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetEdge base,tip;
    CabinetFace landing;
    CabinetSpan span;
    ProFeature web,step;
    ProName web_name;
    double rise=as_arg(cmd,4),land=as_arg(cmd,5),radius=as_arg(cmd,6);
    if(rise<=2*radius)return PRO_TK_BAD_INPUTS;
    ProError error=cm_edge_get(tx->model,cmd->arg[3],&base);
    if(error)return error;
    swprintf_s(web_name,PRO_NAME_SIZE,L"%ls_WEB",tx->name);
    if((error=cm_attach(tx,web_name,&base,0,base.length,rise,90,radius,cmd->arg[7],&web))||
       (error=cm_wall_tip(tx->model,&web,&base,&tip)))return error;
    const wchar_t *side=cmd->arg[7];
    for(int attempt=0;attempt<2;attempt++){
        if((error=cm_attach(tx,tx->name,&tip,0,tip.length,land,90,radius,side,&step))||
           (error=cm_wall_face(&step,&landing))||
           (error=cm_face_span(tx->model,landing.id,base.inward,&span)))return error;
        if(fabs(ad_dot(landing.normal,base.normal))>1-1e-8&&
           (span.min+span.max)/2<ad_dot(base.a,base.inward)-0.01*tx->scale)break;
        if(attempt)return PRO_TK_BAD_CONTEXT;
        if((error=model_remove_feature((ProSolid)tx->model,step.id))||(error=part_regenerate(tx->model)))return error;
        if((error=cm_wall_tip(tx->model,&web,&base,&tip)))return error;
        side=!wcscmp(side,L"POS")?L"NEG":L"POS";
    }
    double actual=cm_plane_offset(&landing,base.a)/tx->scale;
    if(!cm_near(actual,rise,1)){
        double height=2*rise-actual;
        if(height<=2*radius)return PRO_TK_BAD_INPUTS;
        if((error=cm_resize_wall(tx,&web,height))||(error=cm_wall_face(&step,&landing)))return error;
        actual=cm_plane_offset(&landing,base.a)/tx->scale;
    }
    if(!cm_near(actual,rise,1))return PRO_TK_GENERAL_ERROR;
    part_text_add(out,L"已创建两道 90° 折弯的 Z 形错台，实测平行驱动面高差 %.8g mm，第二段轮廓尺寸 %.8g mm。",actual,land);
    return PRO_TK_NO_ERROR;
}
