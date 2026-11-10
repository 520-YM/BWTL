static ProError sx_helix_curve(FdTxn *tx,const PartCommand *cmd,FdRef *csys,double radius,double height,double angle){
    ProElement equation=NULL;
    wchar_t formula[512];
    ProError e;
    if((e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_CSYS,csys))||
       (e=model_element(tx->root,PRO_E_CURVE_TYPE,1,PRO_CURVE_TYPE_FROM_EQUATION,NULL))||
       (e=fd_ref_element(tx,tx->root,PRO_E_CRV_FR_EQ_REF_CSYS,csys))||
       (e=model_element(tx->root,PRO_E_CRV_FR_EQ_CSYS_TYPE,1,PRO_CRV_FR_EQ_CSYS_TYPE_CARTESIAN,NULL))||
       (e=model_element(tx->root,PRO_E_CRV_FR_EQ_PARAM_MIN,2,0,NULL))||
       (e=model_element(tx->root,PRO_E_CRV_FR_EQ_PARAM_MAX,2,1,NULL))||
       (e=model_element(tx->root,PRO_E_CRV_ENTER_EQUATION,0,0,&equation)))return e;
    swprintf_s(formula,_countof(formula),L"x=%.15g*cos((%.15g)*t)\ny=%.15g*sin((%.15g)*t)\nz=%.15g*t",radius,angle,radius,angle,height);
    if((e=ProElementWstringSet(equation,formula))||(e=fd_create(tx,0)))return e;
    return fd_verify(tx);
}

static ProError sx_verify_helix(FdTxn *tx,FdRef *csys,const SxPath *path,double radius,double height,double angle){
    ProCsys frame;
    ProGeomitemdata *raw=NULL;
    ProCsysdata *d;
    ProCurve curve;
    ProVector xyz,delta;
    double length,expected=hypot(radius*angle*AS_PI/180,height),tol=fmax(radius*0.001,0.01*tx->scale);
    ProError e;
    if(path->count!=1)return PRO_TK_BAD_CONTEXT;
    if((e=ProGeomitemToCsys(&csys->item,&frame))||(e=ProCsysDataGet(frame,&raw))||
       (e=ProGeomitemToCurve((ProGeomitem *)&path->items[0].item,&curve))||(e=ProCurveLengthEval(curve,&length)))goto done;
    d=raw->data.p_csys_data;
    if(fabs(length-expected)>expected*0.003){e=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<=16;i++){
        double x,y,z,phase;
        if((e=ProCurveXyzdataEval(curve,i/16.0,xyz,NULL,NULL)))goto done;
        ad_sub(xyz,d->origin,delta);
        x=ad_dot(delta,d->x_vector);y=ad_dot(delta,d->y_vector);z=ad_dot(delta,d->z_vector);
        phase=angle*z/height*AS_PI/180;
        if(fabs(x-radius*cos(phase))>tol||fabs(y-radius*sin(phase))>tol||
           (i==0&&fabs(z)>tol)||(i==16&&fabs(z-height)>tol)){e=PRO_TK_GENERAL_ERROR;goto done;}
    }
done:
    if(raw)ProGeomitemdataFree(&raw);
    return e;
}

static ProError sx_helix(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn path_tx={0},solid={0};
    FdRef csys={0};
    SxPath path={0};
    SxProfile section;
    ProName path_name;
    int cut=!wcscmp(cmd->arg[3],L"CUT"),bodies=0,restore=1;
    double radius,pitch,turns=as_arg(cmd,7),angle,volume;
    ProError e;
    const char *stage="prepare helical sweep";
#define HX_TRY(label,call) do{stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    if(swprintf_s(path_name,_countof(path_name),L"%.24ls_HLX",cmd->arg[2])<0)return PRO_TK_BAD_INPUTS;
    HX_TRY("validate helical sweep name",fd_begin(&solid,model,cmd->arg[2],cut?PRO_FEAT_CUT:PRO_FEAT_PROTRUSION));
    HX_TRY("read helical sweep body count",sd_body_count(model,&bodies));
    if(bodies>1||(cut&&bodies!=1)){e=PRO_TK_BAD_CONTEXT;goto done;}
    section=sx_profile_command(cmd,9,solid.scale);
    radius=as_arg(cmd,5)*solid.scale/2;pitch=as_arg(cmd,6)*solid.scale;
    angle=turns*360*(!wcscmp(cmd->arg[8],L"RH")?1:-1);
    {
        double size=!wcscmp(section.type,L"RECT")?hypot(section.first,section.second):fmax(section.first,section.second);
        if(size>=pitch||size>=2*radius){stage="validate pitch and section clearance";e=PRO_TK_BAD_INPUTS;goto done;}
    }
    HX_TRY("prepare associated helix curve",fd_begin(&path_tx,model,path_name,PRO_FEAT_CURVE));
    HX_TRY("create editable analytic helical trajectory",sx_helix_curve(&path_tx,cmd,&csys,radius,pitch*turns,angle));
    HX_TRY("read created helix path",sx_path_resolve(model,path_name,solid.scale,&path));
    HX_TRY("verify actual helix radius, pitch and handedness",sx_verify_helix(&path_tx,&csys,&path,radius,pitch*turns,angle));
    /* The sweep transaction begins with the verified helper in the feature list. */
    fd_cleanup(&solid,0);
    HX_TRY("prepare helical material feature",fd_begin(&solid,model,cmd->arg[2],cut?PRO_FEAT_CUT:PRO_FEAT_PROTRUSION));
    e=sx_sweep_build(&solid,&path,&section,cut);
    if(e){stage=solid.stage;goto done;}
    HX_TRY("verify helical sweep body and trajectory association",sd_profile_verify_change(&solid,PRO_SWEEP,&path.parent,1,cut,&volume));
    swprintf_s(report,capacity,L"已创建螺旋扫描%ls %ls，关联轨迹 %ls，轨迹直径 %.9g mm，螺距 %.9g mm，%.9g 圈。零件体积 %.9g mm³。已核对旋向、关联和重生成，未保存。",
        cut?L"切除":L"加料",cmd->arg[2],path_name,2*radius/solid.scale,pitch/solid.scale,turns,volume);
done:
    sx_path_free(&path);
    if(solid.model){
        solid.stage=stage;
        sd_profile_finish(&solid,e,bodies,report,capacity);
        restore=solid.rollback_ok;
    }
    fd_ref_free(&csys);
    fd_cleanup(&path_tx,e);
    if(e&&(!restore||(path_tx.model&&!path_tx.rollback_ok)))
        swprintf_s(report,capacity,L"螺旋扫描未完成（%hs，错误码%d）。恢复状态未能核实，请检查模型。",stage,e);
    return e;
#undef HX_TRY
}
