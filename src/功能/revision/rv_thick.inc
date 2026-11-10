typedef struct {ProMdl model;double expected;int pairs;} RevisionThickness;

static ProError rv_thickness_face(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    RevisionThickness *check=data;
    ProSmtSurfType type;
    ProSrftype shape;
    ProError error=ProSmtSurfaceTypeGet((ProPart)check->model,surface,&type);
    if(error||type!=PRO_SMT_SURF_FACE)return error;
    if((error=ProSurfaceTypeGet(surface,&shape))||shape!=PRO_SRF_PLANE)return error;
    ProSurface opposite;
    error=ProSmtOppsurfGet((ProPart)check->model,surface,&opposite);
    if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    if(error)return error;
    ProGeomitem a,b;
    ProVector p,q,n,m,delta;
    if((error=ProSurfaceToGeomitem((ProSolid)check->model,surface,&a))||
       (error=ProSurfaceToGeomitem((ProSolid)check->model,opposite,&b))||
       (error=fd_plane_data(&a,p,n))||(error=fd_plane_data(&b,q,m)))return error;
    ad_sub(q,p,delta);
    if(!ad_unit(n)||!ad_unit(m)||fabs(ad_dot(n,m))<1-1e-6||
       !part_close(fabs(ad_dot(delta,n)),check->expected))return PRO_TK_BAD_CONTEXT;
    check->pairs++;
    return PRO_TK_NO_ERROR;
}

static ProError rv_thickness_read(ProMdl model,double expected){
    ProDimension dim;
    ProSolidBody body;
    double value;
    ProError error=ProSmtPartThicknessGet((ProPart)model,&dim);
    if(!error)error=ProDimensionValueGet(&dim,&value);
    if(error||!part_close(value,expected))return error?error:PRO_TK_BAD_CONTEXT;
    RevisionThickness check={model,expected,0};
    error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,rv_thickness_face,&check);
    return error?error:check.pairs?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError rv_thick(Revision *tx,const PartCommand *cmd,PartText *out){
    ProModelitem owner;
    ProParameter param;
    ProDimension dim;
    ProFeature feature;
    ProParamvalue original,next;
    ProError error;
    double old=as_arg(cmd,2)/tx->mm,wanted=as_arg(cmd,3)/tx->mm;
    if(part_close(old,wanted))return PRO_TK_NO_CHANGE;
    if((error=rv_thickness_read(tx->model,old))||(error=ProMdlToModelitem(tx->model,&owner)))return error;
    error=ProParameterInit(&owner,L"SMT_THICKNESS",&param);
    if(error==PRO_TK_E_NOT_FOUND){
        if((error=ProSmtPartThicknessGet((ProPart)tx->model,&dim))||
           (error=ProDimensionOwnerfeatureGet(&dim,&feature))||
           (error=rv_add(tx,dim,feature.id,old,wanted))||(error=rv_apply(tx))||
           (error=rv_thickness_read(tx->model,wanted)))return error;
    }else{
        if(!error)error=ProParameterValueWithUnitsGet(&param,&original,NULL);
        if(error)return error;
        if(original.type!=PRO_PARAM_DOUBLE||!part_close(original.value.d_val,old))return PRO_TK_BAD_CONTEXT;
        next=original;
        next.value.d_val=wanted;
        error=ProParameterScaledvalueSet(&param,&next,NULL);
        if(!error)error=part_regenerate(tx->model);
        if(!error)error=rv_thickness_read(tx->model,wanted);
        ProOutline bounds;
        ProMassProperty mass;
        if(!error)error=rv_geometry(tx->model,bounds,&mass);
        if(error){
            ProParamvalue current;
            int ok=!ProParameterValueWithUnitsGet(&param,&current,NULL)&&current.type==PRO_PARAM_DOUBLE&&
                part_close(current.value.d_val,old);
            if(!ok)ok=!ProParameterScaledvalueSet(&param,&original,NULL);
            if(!ok||part_regenerate(tx->model)||rv_thickness_read(tx->model,old)||!rv_geometry_restored(tx))
                rv_recovery_uncertain=1;
            part_text_add(out,rv_recovery_uncertain?L"板厚恢复未能核实，已停止本会话改规格。":L"原板厚与实体状态已恢复。");
            return error;
        }
    }
    part_text_add(out,L"钣金板厚已从 %.8g 改为 %.8g mm，实际板厚读回一致。",as_arg(cmd,2),as_arg(cmd,3));
    return PRO_TK_NO_ERROR;
}
