static ProError quick_cuts(FdTxn *tx,const QuickPlan *plan){
    PartCommand cmd={0};
    PanelFrame frame={0};
    ProMatrix location;
    PanelProfile profile[6];
    double area=0;
    ProError error=quick_frame(tx,plan->thickness,&cmd,&frame);
    if(error)return error;
    static const enum PanelShape shapes[]={PN_CIRCLE,PN_RECTANGLE,PN_EDGE_SLOT,PN_CAPSULE};
    for(int i=0;i<plan->count;i++){
        const QuickOpening *o=&plan->cut[i];
        profile[i]=(PanelProfile){shapes[o->shape],o->x,o->y,o->w,o->h,0,0};
        error=pn_profile_probes(tx,&frame,&profile[i],plan->thickness,0);
        if(error)return error;
        area+=pn_area(&profile[i]);
    }
    ProMassProperty mass;
    if((error=qa_geometry_mass(tx->model,&mass)))return error;
    tx->volume=mass.volume;
    error=pn_sketch(tx->model,&cmd,&frame,L"QP_OPENING_SK",profile,plan->count,plan->walls!=0,location);
    if(!error)error=pn_cut(tx,&frame,L"QP_OPENINGS",L"QP_OPENING_SK",location,plan->thickness,plan->walls!=0);
    if(!error)error=md_expected_volume(tx,-area*plan->thickness);
    for(int i=0;!error&&i<plan->count;i++)
        error=pn_profile_probes(tx,&frame,&profile[i],plan->thickness,1);
    return error;
}
