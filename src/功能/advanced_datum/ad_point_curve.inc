static ProError ad_point_curve(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],AdGeometry *g){
    ProElement points=NULL,point=NULL,name=NULL,array=NULL,entry=NULL;
    double amount,length,distance,parameter;
    const wchar_t *mode=cmd->arg[4];
    int percent=!wcscmp(mode,L"PERCENT"),is_end=!wcscmp(mode,L"END");
    ProError e;
    if((e=ad_curve_ref(tx->model,cmd->arg[3],&ref[0],&g->curve))||(e=ProCurveLengthEval(g->curve,&length)))return e;
    part_number(cmd->arg[5],&amount);
    distance=is_end?length:percent?amount*length/100:amount*tx->scale;
    if(!isfinite(length)||length<1e-5*tx->scale||distance>length+1e-8*tx->scale)return PRO_TK_BAD_INPUTS;
    distance=fmin(distance,length);
    if(distance==0||distance==length)e=ProCurveXyzdataEval(g->curve,distance==0?0:1,g->point,NULL,NULL);
    else e=ProCurveParamByLengthEval(g->curve,0,distance,&parameter,g->point);
    if(e)return e;
    if((e=model_element(tx->root,PRO_E_DPOINT_TYPE,1,PRO_DPOINT_TYPE_GENERAL,NULL))||
       (e=model_element(tx->root,PRO_E_DPOINT_POINTS_ARRAY,0,0,&points))||
       (e=model_element(points,PRO_E_DPOINT_POINT,0,0,&point))||
       (e=model_element(point,PRO_E_DPOINT_POINT_NAME,0,0,&name))||
       (e=ProElementWstringSet(name,(wchar_t *)cmd->arg[2]))||
       (e=model_element(point,PRO_E_DPOINT_PLA_CONSTRAINTS,0,0,&array))||
       (e=model_element(array,PRO_E_DPOINT_PLA_CONSTRAINT,0,0,&entry))||
       (e=fd_ref_element(tx,entry,PRO_E_DPOINT_PLA_CONSTR_REF,&ref[0]))||
       (e=model_element(entry,PRO_E_DPOINT_PLA_CONSTR_TYPE,1,PRO_DTMPNT_CONSTR_TYPE_ON,NULL))||
       (e=model_element(point,PRO_E_DPOINT_DIM_CONSTRAINTS,0,0,&array))||
       (e=model_element(array,PRO_E_DPOINT_DIM_CONSTRAINT,0,0,&entry))||
       (e=fd_ref_element(tx,entry,PRO_E_DPOINT_DIM_CONSTR_REF,&ref[0]))||
       (e=model_element(entry,PRO_E_DPOINT_DIM_CONSTR_TYPE,1,percent||is_end?PRO_DTMPNT_CONSTR_TYPE_RATIO:PRO_DTMPNT_CONSTR_TYPE_LENGTH,NULL)))return e;
    return model_element(entry,PRO_E_DPOINT_DIM_CONSTR_VAL,2,is_end?1:percent?amount/100:distance,NULL);
}
