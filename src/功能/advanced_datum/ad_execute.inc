static ProError ad_verify_geometry(FdTxn *tx,AdKind kind,ProType type,FdRef ref[3],AdGeometry *g){
    FdRef actual={0};
    ProVector xyz,normal,delta,cross;
    double tolerance=fmax(1e-7,0.001*tx->scale);
    ProError e;
    if(kind==AD_INTERSECTION)return ad_verify_intersection(tx,ref);
    if(kind==AD_SPLINE)return PRO_TK_NO_ERROR;
    if((e=fd_feature_geom(&tx->feature,type,&actual.item)))return e;
    if(type==PRO_SURFACE){
        if((e=fd_plane_data(&actual.item,xyz,normal)))return e;
        ad_sub(g->point,xyz,delta);
        return fabs(ad_dot(delta,normal))<=tolerance&&fabs(fabs(ad_dot(normal,g->normal))-1)<=1e-7?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    }
    if(type==PRO_AXIS){
        if((e=ad_line(&actual,xyz,normal)))return e;
        ad_sub(g->point,xyz,delta);ad_cross(delta,normal,cross);
        return sqrt(ad_dot(cross,cross))<=tolerance&&fabs(fabs(ad_dot(normal,g->normal))-1)<=1e-7?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    }
    if(type==PRO_POINT){
        if((e=ad_point(&actual,xyz)))return e;
        return ad_near(xyz,g->point,tolerance)?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    }
    {
        ProCsys csys;
        ProGeomitemdata *raw=NULL;
        ProCsysdata *frame;
        if((e=ProGeomitemToCsys(&actual.item,&csys))||(e=ProCsysDataGet(csys,&raw)))return e;
        frame=raw->data.p_csys_data;
        ad_cross(g->normal,g->axis_y,cross);
        if(!ad_near(frame->origin,g->point,tolerance)||ad_dot(frame->x_vector,g->normal)<1-1e-7||
           ad_dot(frame->y_vector,g->axis_y)<1-1e-7||ad_dot(frame->z_vector,cross)<1-1e-7)e=PRO_TK_GENERAL_ERROR;
        ProGeomitemdataFree(&raw);
        return e;
    }
}

static ProError ad_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    const wchar_t *labels[]={L"",L"角度基准面",L"三点基准面",L"两平面中分面",L"相切基准面",L"路径法向基准面",
        L"两点基准轴",L"曲线定位点",L"曲面交线",L"三维插值样条",L"对称基准坐标系"};
    FdTxn tx={0};FdRef ref[3]={{0}};
    AdGeometry geometry={0};
    AdKind kind=ad_kind(cmd->arg[0]);
    ProElement constraints=NULL;
    ProPath spline_file=L"";
    ProError e;
    ProFeattype feature=kind<=AD_NORMAL?PRO_FEAT_DATUM:kind==AD_AXIS_POINTS?PRO_FEAT_DATUM_AXIS:
        kind==AD_POINT_CURVE?PRO_FEAT_DATUM_POINT:kind==AD_SYM_CSYS?PRO_FEAT_CSYS:PRO_FEAT_CURVE;
    ProType type=kind<=AD_NORMAL?PRO_SURFACE:kind==AD_AXIS_POINTS?PRO_AXIS:kind==AD_POINT_CURVE?PRO_POINT:kind==AD_SYM_CSYS?PRO_CSYS:PRO_CURVE;
    int i;
#define AD_STEP(stage_label,call) do{tx.stage=stage_label;e=(call);trace(stage_label,e);if(e)goto done;}while(0)
    if(advanced_datum_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1){wcscpy_s(report,capacity,L"高级基准参数无效，未调用 Creo 写接口。");return PRO_TK_BAD_INPUTS;}
    AD_STEP("prepare advanced datum",fd_begin(&tx,model,cmd->arg[2],feature));
    if(kind<=AD_NORMAL)AD_STEP("allocate native plane constraints",model_element(tx.root,PRO_E_DTMPLN_CONSTRAINTS,0,0,&constraints));
    switch(kind){
    case AD_ANGLE:AD_STEP("resolve hinge and angle plane",ad_angle(&tx,cmd,ref,constraints,&geometry));break;
    case AD_THREE:AD_STEP("resolve non-collinear datum points",ad_three(&tx,cmd,ref,constraints,&geometry));break;
    case AD_MID:AD_STEP("resolve distinct parallel planes",ad_mid(&tx,cmd,ref,constraints,&geometry));break;
    case AD_TANGENT:AD_STEP("resolve cylindrical or conical face and tangent point",ad_tangent(&tx,cmd,ref,constraints,&geometry));break;
    case AD_NORMAL:AD_STEP("resolve point on single curve and tangent",ad_normal(&tx,cmd,ref,constraints,&geometry));break;
    case AD_AXIS_POINTS:AD_STEP("resolve distinct axis points",ad_axis_points(&tx,cmd,ref,&geometry));break;
    case AD_POINT_CURVE:AD_STEP("resolve curve point distance or ratio",ad_point_curve(&tx,cmd,ref,&geometry));break;
    case AD_INTERSECTION:AD_STEP("bind both surfaces to intersection",ad_intersection(&tx,cmd,ref));break;
    case AD_SPLINE:e=ad_spline(&tx,cmd,ref,spline_file);trace(tx.stage,e);if(e)goto done;break;
    case AD_SYM_CSYS:AD_STEP("resolve three orthogonal planes and axes",ad_sym_csys(&tx,cmd,ref,&geometry));break;
    default:e=PRO_TK_BAD_INPUTS;goto done;
    }
    if(kind!=AD_SPLINE)AD_STEP("create advanced native datum",fd_create(&tx,0));
    AD_STEP("verify datum state and linked parents",fd_verify(&tx));
    AD_STEP("verify datum placement and geometry",ad_verify_geometry(&tx,kind,type,ref,&geometry));
    swprintf_s(report,capacity,L"已创建%ls %ls（特征 ID %d）。已核对实际几何、重生成和实体保持。未保存零件。",labels[kind],cmd->arg[2],tx.feature.id);
done:
    if(e&&tx.errors.error_list)for(i=0;i<tx.errors.error_number&&i<32;i++){
        trace("advanced datum element id",tx.errors.error_list[i].err_item_id);
        trace("advanced datum element error",tx.errors.error_list[i].error);
    }
    fd_cleanup(&tx,e);
    for(i=0;i<3;i++)fd_ref_free(&ref[i]);
    if(e){
        if(spline_file[0]&&tx.rollback_ok)DeleteFileW(spline_file);
        swprintf_s(report,capacity,L"基准未完成：%hs，错误码%d。%ls。未保存零件。",tx.stage,e,
            tx.rollback_ok?L"已核对原特征保留":L"回滚未能验证，请检查当前零件");
    }
    return e;
#undef AD_STEP
}
