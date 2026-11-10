static ProError ad_intersection(FdTxn *tx,const PartCommand *cmd,FdRef ref[3]){
    const ProElemId compound[]={PRO_E_CRV_IP_COMP_REF1,PRO_E_CRV_IP_COMP_REF2};
    const ProElemId selector[]={PRO_E_CRV_IP_REF_SEL1_TYPE,PRO_E_CRV_IP_REF_SEL2_TYPE};
    const ProElemId reference[]={PRO_E_CRV_IP_REF1,PRO_E_CRV_IP_REF2};
    ProElement entry=NULL;
    int i;
    ProError e;
    for(i=0;i<2;i++)if((e=fd_ref_resolve(tx->model,cmd->arg[3+i],PRO_SURFACE,&ref[i])))return e;
    if(ref[0].item.id==ref[1].item.id)return PRO_TK_BAD_INPUTS;
    if((e=model_element(tx->root,PRO_E_CURVE_TYPE,1,PRO_CURVE_TYPE_INTSRF,NULL))||
       (e=model_element(tx->root,PRO_E_CRV_IP_REF_TYPE,1,PRO_CURVE_TYPE_INTSRF,NULL)))return e;
    for(i=0;i<2;i++)if((e=model_element(tx->root,compound[i],0,0,&entry))||
       (e=model_element(entry,selector[i],1,PRO_CURVE_TYPE_MULTIPLE_SEL,NULL))||
       (e=fd_ref_element(tx,entry,reference[i],&ref[i])))return e;
    return PRO_TK_NO_ERROR;
}

typedef struct {ProMdl model;ProSurface surfaces[2];double tolerance;int count;} AdIntersectCheck;

static ProError ad_intersection_curve(ProGeomitem *item,ProError status,ProAppData data){
    AdIntersectCheck *check=data;
    ProCurve curve;
    ProEnttype type;
    ProVector point,projected,d1[2],d2[3],normal;
    ProUvParam uv;
    double length;
    int i,j;
    ProError e;
    if(status)return status;
    if((e=ProGeomitemToCurve(item,&curve))||(e=ProCurveTypeGet(curve,&type)))return e;
    if(type==PRO_ENT_CMP_CRV)return PRO_TK_NO_ERROR;
    if((e=ProCurveLengthEval(curve,&length)))return e;
    if(!isfinite(length)||length<=check->tolerance)return PRO_TK_BAD_CONTEXT;
    for(i=0;i<=4;i++){
        if((e=ProCurveXyzdataEval(curve,i/4.0,point,NULL,NULL)))return e;
        for(j=0;j<2;j++){
            if((e=ProSurfaceParamEval((ProSolid)check->model,check->surfaces[j],point,uv))||
               (e=ProSurfaceXyzdataEval(check->surfaces[j],uv,projected,d1,d2,normal)))return e;
            if(!ad_near(point,projected,check->tolerance))return PRO_TK_GENERAL_ERROR;
        }
    }
    check->count++;
    return PRO_TK_NO_ERROR;
}

static ProError ad_verify_intersection(FdTxn *tx,FdRef ref[3]){
    AdIntersectCheck check={tx->model,{NULL,NULL},fmax(1e-7,tx->scale*0.001),0};
    ProError e;
    if((e=ProGeomitemToSurface(&ref[0].item,&check.surfaces[0]))||(e=ProGeomitemToSurface(&ref[1].item,&check.surfaces[1]))||
       (e=ProFeatureGeomitemVisit(&tx->feature,PRO_CURVE,ad_intersection_curve,NULL,&check)))return e;
    return check.count?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}
