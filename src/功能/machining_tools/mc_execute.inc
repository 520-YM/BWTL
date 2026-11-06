static int mc_recovery_uncertain=0;
static ProError mc_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};McHoleGeometry geometry={0};ProError e;int bodies=0,i,hole=!wcscmp(cmd->arg[0],L"HOLE"),round=!wcscmp(cmd->arg[0],L"EDGE_ROUND");
    ProFeattype type=hole?PRO_FEAT_HOLE:round?PRO_FEAT_ROUND:PRO_FEAT_CHAMFER;double volume=0,d=0,h=0,expected;
    report[0]=0;
    if(machining_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1){wcscpy_s(report,capacity,L"加工协议无效，未调用写接口。");return PRO_TK_BAD_INPUTS;}
    if(!wcscmp(cmd->arg[0],L"MACHINING_INFO"))return mc_references(model,report,capacity);
    if(mc_recovery_uncertain){wcscpy_s(report,capacity,L"先前加工恢复未能核实，本次会话已停止加工写入；请检查模型后重新启动独立BWTL。");return PRO_TK_BAD_CONTEXT;}
    tx.stage="validate ordinary single-body part";
    if((e=fd_begin(&tx,model,cmd->arg[2],type)))goto done;
    tx.stage="validate existing single solid body";
    if((e=sd_body_count(model,&bodies)))goto done;
    if(bodies!=1||tx.was_empty||!tx.has_volume||!isfinite(tx.volume)||tx.volume<=0){e=PRO_TK_BAD_CONTEXT;goto done;}
    e=hole?mc_hole(&tx,cmd,&geometry):mc_edge(&tx,cmd);if(e)goto done;
    tx.stage="verify feature type, name, parents, health and material";
    if((e=mc_verify(&tx,type,&volume)))goto done;
    if(hole){
        ProElemId dia[]={PRO_E_HLE_COM,PRO_E_DIAMETER};
        ProElemId dep[]={PRO_E_HLE_COM,PRO_E_HOLE_STD_DEPTH,PRO_E_HOLE_DEPTH_TO,PRO_E_HOLE_DEPTH_TO_TYPE};
        part_number(cmd->arg[8],&d);part_number(cmd->arg[10],&h);d*=tx.scale;h*=tx.scale;
        if(volume>=tx.volume){e=PRO_TK_GENERAL_ERROR;goto done;}
        if((e=mc_tree_value(&tx.feature,dia,2,d,0))||(e=mc_tree_value(&tx.feature,dep,4,h>0?PRO_HLE_STRGHT_BLIND_DEPTH:PRO_HLE_STRGHT_THRU_ALL_DEPTH,1)))goto done;
        tx.stage="verify actual cylindrical wall, diameter and hole axis location";
        if((e=ProFeatureGeomitemVisit(&tx.feature,PRO_SURFACE,mc_hole_wall,NULL,&geometry)))goto done;
        expected=3.141592653589793*d*d*geometry.length/4;
        if(geometry.cylinders<1||fabs(geometry.angle-2*3.141592653589793)>1e-5||fabs(tx.volume-volume-expected)>fmax(1e-7,expected*1e-5)){e=PRO_TK_BAD_CONTEXT;goto done;}
        if(h>0){
            ProElemId deep[]={PRO_E_HLE_COM,PRO_E_HOLE_STD_DEPTH,PRO_E_HOLE_DEPTH_TO,PRO_E_EXT_DEPTH_TO_VALUE};
            tx.stage="verify blind depth and full cylindrical removed volume";
            if((e=mc_tree_value(&tx.feature,deep,4,h,0)))goto done;
            expected=3.141592653589793*d*d*h/4;
            if(fabs(tx.volume-volume-expected)>fmax(1e-7,expected*1e-5)){e=PRO_TK_BAD_CONTEXT;goto done;}
        }
    }else{
        part_number(cmd->arg[3],&d);tx.stage="verify actual round or chamfer size";
        if((e=mc_edge_size(&tx.feature,d*tx.scale)))goto done;
    }
    swprintf_s(report,capacity,L"已在当前零件创建%ls %ls，特征ID=%d。特征类型、名称、父参照、重生成、单实体和体积读回通过：%.9g → %.9g mm³。%ls未自动保存。",hole?(h>0?L"独立平底盲孔":L"独立通孔"):round?L"固定半径圆角":L"等距离倒角",cmd->arg[2],tx.feature.id,tx.volume/(tx.scale*tx.scale*tx.scale),volume/(tx.scale*tx.scale*tx.scale),hole?L"":L"仅使用明确指定边，不自动扩展相切链。");
done:
    if(e&&tx.errors.error_list)for(i=0;i<tx.errors.error_number&&i<32;i++){trace("machining element id",tx.errors.error_list[i].err_item_id);trace("machining element error",tx.errors.error_list[i].error);}
    mc_cleanup(&tx,e);
    if(e&&!tx.rollback_ok)mc_recovery_uncertain=1;
    if(e)swprintf_s(report,capacity,L"加工未完成：%hs，错误码%d。%ls；未保存。%ls",tx.stage,e,tx.rollback_ok?L"未新增或已核对移除本次特征，原实体体积保持":L"恢复未能核实，本次会话已停止加工写入",hole?L"孔首版要求完整圆柱孔壁和去除体积；盲孔不得穿透。":L"边缘加工首版要求明确直边、正交平面邻面和可实现的固定尺寸。");
    return e;
}
