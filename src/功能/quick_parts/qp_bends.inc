static ProError quick_bends(FdTxn *tx,const QuickPlan *plan,const wchar_t *name){
    int face;
    ProError error=quick_top(tx,plan->thickness,&face);
    if(!error)error=cm_native_command(tx->model,L"CREO_OP SMT_CONVERT %ls QP_SHEET SURFACE:%d %.12g",
        name,face,plan->thickness);
    if(error)return error;
    for(int i=0;i<plan->walls;i++){
        if((error=quick_top(tx,plan->thickness,&face)))return error;
        const double y=i?plan->depth*tx->scale:0;
        CabinetEdge line={0};
        line.a[1]=y;line.a[2]=plan->thickness*tx->scale;line.axis[0]=1;
        CabinetEdgeSearch search={0};
        search.model=tx->model;search.face_id=face;search.line=&line;
        search.to=plan->width*tx->scale;search.tolerance=1e-5*tx->scale;
        if((error=cm_edges_on_face(&search)))return error;
        if(search.matches!=1)return PRO_TK_BAD_CONTEXT;
        ProFeature wall;
        const wchar_t *feature=i?L"QP_WALL_B":L"QP_WALL_A";
        if((error=cm_attach(tx,feature,&search.edge,0,search.edge.length,plan->height,90,plan->radius,L"NEG",&wall)))return error;
        CabinetEdge tip;
        if((error=cm_wall_tip(tx->model,&wall,&search.edge,&tip))){trace("quick wall tip query",error);return error;}
        if(tip.a[2]-line.a[2]<=0.01*tx->scale||fabs(tip.a[2]-tip.b[2])>1e-5*tx->scale){trace("quick flange orientation z microns",(int)(1000*(tip.a[2]-line.a[2])/tx->scale));return PRO_TK_BAD_CONTEXT;}
    }
    FabBends formed={0};
    formed.model=tx->model;formed.mm=1/tx->scale;formed.thickness=plan->thickness;
    error=ProSolidSurfaceVisit((ProSolid)tx->model,fab_bend_visit,NULL,&formed);
    if(error||formed.count!=plan->walls)return error?error:PRO_TK_BAD_CONTEXT;
    for(int i=0;i<formed.count;i++){
        const FabBend *row=&formed.rows[i];
        if(fabs(fabs(row->angle)-90)>0.001||fabs(row->radius-plan->radius)>0.001)
            return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}
