static ProError cm_return(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetEdge tip;
    CabinetFace face;
    ProFeature parent,wall;
    ProFeattype type;
    FdRef base={0};
    ProVector origin,normal;
    ProError error=cm_edge_get(tx->model,cmd->arg[3],&tip);
    if(!error)error=ProFeatureInit((ProSolid)tx->model,tip.owner,&parent);
    if(!error)error=ProFeatureTypeGet(&parent,&type);
    if(!error&&type!=PRO_FEAT_WALL)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=md_solid_face(tx,cmd->arg[4],&base);
    if(!error)error=fd_plane_data(&base.item,origin,normal);
    if(!error&&(!ad_unit(normal)||fabs(ad_dot(normal,tip.normal))>1e-8))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=cm_attach(tx,tx->name,&tip,0,tip.length,as_arg(cmd,5),90,as_arg(cmd,6),cmd->arg[7],&wall);
    if(!error)error=cm_wall_face(&wall,&face);
    if(!error&&fabs(ad_dot(normal,face.normal))<1-1e-8)error=PRO_TK_GENERAL_ERROR;
    if(!error)part_text_add(out,L"已在现有翻边自由直边上增加第二道 90° 回折。回折驱动面与门板参考面平行，相距 %.8g mm。",
        cm_plane_offset(&face,origin)/tx->scale);
    fd_ref_free(&base);
    return error;
}
