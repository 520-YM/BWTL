static ProError mp_cross(FdTxn *tx,const PartCommand *cmd,const ShopSetup *first,PartText *out){
    ShopSetup second;
    ProError error=mp_setup(tx,cmd,10,&second);
    if(error)return error;
    const ShopSetup *setups[]={first,&second};
    AsPoint points[]={as_point(as_arg(cmd,6),as_arg(cmd,7)),as_point(as_arg(cmd,13),as_arg(cmd,14))};
    double radii[]={as_arg(cmd,8)/2,as_arg(cmd,15)/2},depths[]={as_arg(cmd,9),as_arg(cmd,16)};
    ProVector origins[2],intersections[2],between;
    const double *d1=first->frame.inward,*d2=second.frame.inward;
    if(fabs(ad_dot(d1,d2))>1e-8)return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<2;i++)am_world(&setups[i]->frame.placement,points[i].x*tx->scale,points[i].y*tx->scale,0,origins[i]);
    ad_sub(origins[1],origins[0],between);
    double t[]={ad_dot(between,d1)/tx->scale,-ad_dot(between,d2)/tx->scale};
    ProName sketches[2],branch;
    ProMatrix frames[2];
    for(int i=0;i<2;i++){
        cm_point(origins[i],setups[i]->frame.inward,t[i]*tx->scale,intersections[i]);
        if(t[i]<=radii[1-i]+0.1||depths[i]<=t[i]+radii[1-i]+0.1)return PRO_TK_BAD_CONTEXT;
        if((error=mp_probe_min(tx,&setups[i]->frame,points[i],0,depths[i]+0.1,1))||
           (error=mp_circle_probes(tx,&setups[i]->frame,points[i],radii[i]*0.96,0,depths[i]+0.1,1)))return error;
    }
    if(cm_distance(intersections[0],intersections[1])>0.0002*tx->scale)return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<2;i++){
        PanelProfile profile=mp_circle(points[i],2*radii[i]);
        if((error=mp_prepare_profile(tx,setups[i],i?L"SK2":L"SK1",&profile,1,sketches[i],frames[i])))return error;
    }
    double primary=AS_PI*radii[0]*radii[0]*depths[0],secondary=AS_PI*radii[1]*radii[1]*depths[1];
    swprintf_s(branch,PRO_NAME_SIZE,L"%ls_BRANCH",tx->name);
    if((error=pn_cut(tx,&first->frame,tx->name,sketches[0],frames[0],depths[0],0))||
       (error=md_expected_volume(tx,-primary))||
       (error=pn_cut(tx,&second.frame,branch,sketches[1],frames[1],depths[1],0)))return error;
    double smaller=fmin(radii[0],radii[1]),larger=fmax(radii[0],radii[1]);
    double lower=16*smaller*smaller*smaller/3,upper=2*larger*AS_PI*smaller*smaller;
    if(fabs(radii[0]-radii[1])<0.000001){
        if((error=md_expected_volume(tx,-primary-secondary+lower)))return error;
    }else{
        ProMassProperty mass;
        if((error=qa_geometry_mass(tx->model,&mass)))return error;
        double overlap=primary+secondary-(tx->volume-mass.volume)/pow(tx->scale,3);
        double tolerance=fmax(0.02,(primary+secondary)*0.0005);
        if(overlap<=tolerance||overlap<lower-tolerance||overlap>upper+tolerance)return PRO_TK_GENERAL_ERROR;
    }
    for(int i=0;i<2;i++)if((error=mp_probe_min(tx,&setups[i]->frame,points[i],depths[i],depths[i]+0.1,0)))return error;
    part_text_add(out,L"已创建并核实两条垂直相通的盲通道，孔底深度 %.8g 和 %.8g mm。",depths[0],depths[1]);
    return PRO_TK_NO_ERROR;
}
