static ProError sh_bend_native(FdTxn *tx,const PartCommand *cmd,int roll){
    FdRef curve={0};ProElement line,collection,angle;ProError e;
#define SB(call) do{if((e=(call)))goto done;}while(0)
    SB(sd_profile_curve(tx->model,cmd->arg[3]+6,&curve));
    {ProCurve native_curve;ProEnttype type;SB(ProGeomitemToCurve(&curve.item,&native_curve));SB(ProCurveTypeGet(native_curve,&type));if(type!=PRO_ENT_LINE){e=PRO_TK_BAD_INPUTS;goto done;}}
    SB(model_element(tx->root,PRO_E_SMT_BEND_FORM,1,roll?PRO_SMT_BEND_FORM_ROLL:PRO_SMT_BEND_FORM_ANGLE,NULL));
    SB(model_element(tx->root,PRO_E_SMT_BEND_LINE,0,0,&line));SB(model_element(line,PRO_E_SMT_BEND_LINE_TYPE,1,PRO_SMT_BEND_LINE_CURVE,NULL));
    SB(model_element(line,PRO_E_SMT_BEND_CURVE,0,0,&collection));{ProElement el;SB(model_element(collection,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&el));SB(sd_curve_collection(el,&curve));}
    tx->parents[tx->parent_count++]=curve.owner_feature;SB(model_element(collection,PRO_E_SMT_BEND_CURVE_USE_OFFSET,1,PRO_B_FALSE,NULL));
    SB(model_element(tx->root,PRO_E_SMT_BEND_FIXED_SIDE,1,!wcscmp(cmd->arg[7],L"FIX1")?PRO_SMT_BEND_SIDE_ONE:PRO_SMT_BEND_SIDE_TWO,NULL));
    SB(model_element(tx->root,PRO_E_SMT_BEND_LOCATION,1,PRO_SMT_BEND_SIDE_TWO,NULL));SB(model_element(tx->root,PRO_E_SMT_BEND_DIRECTION,1,!wcscmp(cmd->arg[6],L"POS")?PRO_SMT_BEND_SIDE_ONE:PRO_SMT_BEND_SIDE_TWO,NULL));
    if(!roll){SB(model_element(tx->root,PRO_E_SMT_BEND_ANGLE,0,0,&angle));SB(model_element(angle,PRO_E_SMT_BEND_ANGLE_TYPE,1,PRO_SMT_BEND_ANGLE_EXTERNAL,NULL));SB(model_element(angle,PRO_E_SMT_BEND_ANGLE_VALUE,2,as_arg(cmd,4),NULL));}
    SB(sh_fillet(tx,tx->root,as_arg(cmd,5)));SB(sh_develop(tx));SB(sh_create(tx));
done:fd_ref_free(&curve);return e;
#undef SB
}
static ProError sh_bend(FdTxn *tx,const PartCommand *cmd){return sh_bend_native(tx,cmd,0);}
