static ProError cm_u(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetBlank blank={0};
    CabinetFace faces[2];
    ProVector along;
    int count=0;
    double first_height=0;
    double depth=as_arg(cmd,5),clear=as_arg(cmd,6),t=as_arg(cmd,7),height=as_arg(cmd,8),radius=as_arg(cmd,9);
    ProError error=cm_blank_make(tx,cmd,depth,clear+2*t,t,&blank,along);
    if(error)return error;
    for(int i=0;i<4;i++){
        CabinetEdge *edge=&blank.edges[i];
        if(fabs(ad_dot(edge->axis,along))<1-1e-8)continue;
        if(count==2)return PRO_TK_BAD_CONTEXT;
        ProFeature wall;
        ProName name;
        swprintf_s(name,PRO_NAME_SIZE,L"%ls_W%d",tx->name,count+1);
        if((error=cm_attach(tx,name,edge,0,edge->length,height,90,radius,cmd->arg[10],&wall))||
           (error=cm_wall_face(&wall,&faces[count])))return error;
        CabinetEdge tip;
        if((error=cm_wall_tip(tx->model,&wall,edge,&tip)))return error;
        double actual=cm_edge_height(edge,tip.a);
        if(!count)first_height=actual;
        if(fabs(actual)<0.1*tx->scale||actual*first_height<=0)return PRO_TK_GENERAL_ERROR;
        count++;
    }
    if(count!=2||fabs(ad_dot(faces[0].normal,faces[1].normal))<1-1e-8)return PRO_TK_GENERAL_ERROR;
    ProVector delta;
    ad_sub(faces[1].point,faces[0].point,delta);
    double separation=fabs(ad_dot(delta,faces[0].normal))/tx->scale;
    if(ad_dot(delta,faces[0].normal)<0)separation-=t;
    if(ad_dot(delta,faces[1].normal)>0)separation-=t;
    if(!cm_near(separation,clear,1))return PRO_TK_GENERAL_ERROR;
    part_text_add(out,L"已创建 U 形机箱壳，纵深 %.8g mm，两平行侧壁的内净宽 %.8g mm。",depth,clear);
    return PRO_TK_NO_ERROR;
}
