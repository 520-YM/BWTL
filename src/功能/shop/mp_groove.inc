static ProError mp_shaft_refs(FdTxn *tx,const PartCommand *cmd,const AmFrame *shaft,const PanelFrame *frame){
    FdRef plane={0},axis={0};
    ProVector point,direction,delta,offset;
    ProError error=fd_ref_resolve(tx->model,cmd->arg[6],PRO_SURFACE,&plane);
    if(!error)error=fd_plane_data(&plane.item,point,direction);
    if(!error){
        ad_sub(shaft->csys.origin,point,delta);
        if(!ad_unit(direction)||fabs(ad_dot(direction,frame->placement.csys.z_vector))<1-1e-8||
           fabs(ad_dot(delta,direction))>0.0001*tx->scale)error=PRO_TK_BAD_CONTEXT;
    }
    if(!error)error=fd_ref_resolve(tx->model,cmd->arg[5],PRO_AXIS,&axis);
    if(!error)error=ad_line(&axis,point,direction);
    if(!error){
        ad_sub(point,shaft->csys.origin,delta);
        ad_cross(delta,shaft->csys.z_vector,offset);
        if(fabs(ad_dot(direction,shaft->csys.z_vector))<1-1e-8||
           sqrt(ad_dot(offset,offset))>0.0001*tx->scale)error=PRO_TK_BAD_CONTEXT;
    }
    fd_ref_free(&plane);
    fd_ref_free(&axis);
    return error;
}

static ProError mp_shaft_radius(FdTxn *tx,const AmFrame *shaft,double z,double radius){
    for(int i=0;i<4;i++){
        double angle=(i+0.125)*AS_PI/2,margin=0.2*tx->scale;
        ProRay ray;
        QualityRay hits;
        for(int j=0;j<3;j++)ray.dir_vector[j]=cos(angle)*shaft->csys.x_vector[j]+sin(angle)*shaft->csys.y_vector[j];
        ProVector center;
        am_world(shaft,0,0,z*tx->scale,center);
        cm_point(center,ray.dir_vector,-(shaft->radius+margin),ray.start_point);
        ProError error=qa_ray(tx->model,&ray,0.0001*tx->scale,&hits);
        if(error)return error;
        double mid=shaft->radius+margin,r=radius*tx->scale;
        if(hits.count!=2||!hits.crossing[0].entering||
           fabs(hits.crossing[0].depth-(mid-r))>0.0002*tx->scale||
           fabs(hits.crossing[1].depth-(mid+r))>0.0002*tx->scale)return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static ProError mp_groove(FdTxn *tx,const PartCommand *cmd,PartText *out){
    PartCommand shaft_cmd={0};
    AmFrame shaft={0};
    ShopSetup setup={0};
    wcscpy_s(shaft_cmd.arg[3],PART_TOKEN_MAX,L"SHAFT");
    wcscpy_s(shaft_cmd.arg[4],PART_TOKEN_MAX,cmd->arg[3]);
    wcscpy_s(shaft_cmd.arg[5],PART_TOKEN_MAX,cmd->arg[4]);
    ProError error=am_frame(tx,&shaft_cmd,&shaft);
    if(error)return error;
    double z=as_arg(cmd,8),width=as_arg(cmd,9),depth=as_arg(cmd,10),fillet=as_arg(cmd,11);
    double radius=shaft.radius/tx->scale,root=radius-depth;
    if(z<=shaft.zmin/tx->scale+0.1||z+width>=shaft.zmax/tx->scale-0.1||root<=0.1)return PRO_TK_BAD_CONTEXT;
    setup.frame.placement=shaft;
    memcpy(setup.frame.placement.csys.y_vector,shaft.csys.z_vector,sizeof(ProVector));
    ad_cross(shaft.csys.x_vector,shaft.csys.z_vector,setup.frame.placement.csys.z_vector);
    wcscpy_s(setup.sketch_command.arg[3],PART_TOKEN_MAX,cmd->arg[6]);
    wcscpy_s(setup.sketch_command.arg[5],PART_TOKEN_MAX,cmd->arg[7]);
    if((error=mp_shaft_refs(tx,cmd,&shaft,&setup.frame)))return error;
    double sections[]={z-0.05,z+fillet/2,z+width/2,z+width-fillet/2,z+width+0.05};
    for(int i=0;i<_countof(sections);i++)if((error=mp_shaft_radius(tx,&shaft,sections[i],radius)))return error;
    PanelProfile profile={PN_SHOP_GROOVE,root,z,depth+0.1,width,fillet,0};
    ProName sketch;
    ProMatrix frame;
    if((error=mp_prepare_profile(tx,&setup,L"SK",&profile,1,sketch,frame))||
       (error=am_sub(tx->model,L"CREO_OP SOLID_REVOLVE_CUT %ls %ls %ls %ls 360 POS",cmd->arg[1],tx->name,sketch,cmd->arg[5])))return error;
    double corner=fillet*fillet*(root+fillet/2)-AS_PI*fillet*fillet/4*(root+fillet-4*fillet/(3*AS_PI));
    double volume=2*AS_PI*(width*depth*(radius+root)/2-2*corner);
    if((error=md_expected_volume(tx,-volume)))return error;
    double blend=root+fillet*(1-sqrt(0.75)),expected[]={radius,blend,root,blend,radius};
    for(int i=0;i<_countof(sections);i++)if((error=mp_shaft_radius(tx,&shaft,sections[i],expected[i])))return error;
    part_text_add(out,L"已创建整周轴肩退刀槽，宽 %.8g mm，径向深 %.8g mm，核对两侧槽根圆角 R%.8g。",width,depth,fillet);
    return PRO_TK_NO_ERROR;
}
