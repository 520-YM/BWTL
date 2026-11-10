#ifdef BWTL_CABINET_FAULT_TEST
static int cm_test_fail_bend_after;
static int cm_test_bend_fault_hit;
#endif

typedef struct {
    ProFeature feature;
    ProElement original,edited;
    double angle,radius,new_angle,new_radius;
} CabinetBendEdit;

static ProError cm_bend_elements(ProFeature *feature,ProElement tree,ProElement *angle,ProElement *radius){
    ProFeattype type;
    ProElement state;
    int mode;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(error)return error;
    if(type==PRO_FEAT_WALL){
        if((error=sh_child(tree,PRO_E_SMT_WALL_TYPE,&state))||(error=ProElementIntegerGet(state,NULL,&mode)))return error;
        if(mode!=PRO_SMT_WALL_TYPE_FLAT)return PRO_TK_BAD_CONTEXT;
        if((error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_FLAT_WALL_ANGLE,PRO_E_SMT_FLAT_WALL_ANGLE_TYPE},2,&state))||
           (error=ProElementIntegerGet(state,NULL,&mode)))return error;
        if(mode!=PRO_BND_ANGLE_VALUE)return PRO_TK_BAD_CONTEXT;
        error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_FLAT_WALL_ANGLE,PRO_E_SMT_FLAT_WALL_ANGLE_VAL},2,angle);
    }else if(type==PRO_FEAT_BEND){
        if((error=sh_child(tree,PRO_E_SMT_BEND_FORM,&state))||(error=ProElementIntegerGet(state,NULL,&mode)))return error;
        if(mode!=PRO_SMT_BEND_FORM_ANGLE)return PRO_TK_BAD_CONTEXT;
        error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_BEND_ANGLE,PRO_E_SMT_BEND_ANGLE_VALUE},2,angle);
    }else return PRO_TK_BAD_CONTEXT;
    if(error)return error;
    if((error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_FILLETS,PRO_E_SMT_FILLETS_SIDE},2,&state))||
       (error=ProElementIntegerGet(state,NULL,&mode)))return error;
    if(mode!=PRO_BEND_RAD_INSIDE)return PRO_TK_BAD_CONTEXT;
    return mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_FILLETS,PRO_E_SMT_FILLETS_VALUE},2,radius);
}

static ProError cm_bend_verified(ProFeature *feature,double angle,double radius,double scale){
    ProElement tree=NULL,a,r;
    double measured_angle,measured_radius;
    ProError error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=cm_bend_elements(feature,tree,&a,&r);
    if(!error)error=ProElementDoubleGet(a,NULL,&measured_angle);
    if(!error)error=ProElementDoubleGet(r,NULL,&measured_radius);
    if(tree)ProFeatureElemtreeFree(feature,tree);
    if(error)return error;
    if(!cm_near(measured_angle,angle,1)||!cm_near(measured_radius,radius,scale))return PRO_TK_GENERAL_ERROR;
    ShBendInfo actual={0};
    actual.model=feature->owner;
    if((error=ProFeatureGeomitemVisit(feature,PRO_SURFACE,sh_bend_info,NULL,&actual)))return error;
    if(!actual.count||!cm_near(fabs(actual.angle),angle,1))return PRO_TK_GENERAL_ERROR;
    if(!actual.inside){
        ProDimension thickness;
        double value;
        if((error=ProSmtPartThicknessGet((ProPart)feature->owner,&thickness))||
           (error=ProDimensionValueGet(&thickness,&value)))return error;
        actual.radius-=value;
    }
    return cm_near(actual.radius,radius,scale)?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError cm_bends(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetBendEdit edits[4]={0};
    int count=(cmd->count-3)/5,attempted=0;
    ProError error=PRO_TK_NO_ERROR;
#define CM_B(call) do{if((error=(call))){trace("bend edit " #call,error);goto done;}}while(0)
    for(int i=0;i<count;i++){
        CabinetBendEdit *edit=&edits[i];
        int at=3+5*i;
        edit->angle=as_arg(cmd,at+1);
        edit->radius=as_arg(cmd,at+2)*tx->scale;
        edit->new_angle=as_arg(cmd,at+3);
        edit->new_radius=as_arg(cmd,at+4)*tx->scale;
        if(cm_near(edit->angle,edit->new_angle,1)&&cm_near(edit->radius,edit->new_radius,tx->scale)){
            error=PRO_TK_NO_CHANGE;goto done;
        }
        CM_B(cm_feature_find(tx->model,cmd->arg[at],&edit->feature));
        CM_B(cm_bend_verified(&edit->feature,edit->angle,edit->radius,tx->scale));
        CM_B(ProFeatureElemtreeExtract(&edit->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&edit->original));
        CM_B(ProFeatureElemtreeExtract(&edit->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&edit->edited));
        ProElement a,r;
        CM_B(cm_bend_elements(&edit->feature,edit->edited,&a,&r));
        CM_B(ProElementDoubleSet(a,edit->new_angle));
        CM_B(ProElementDoubleSet(r,edit->new_radius));
    }
    for(int i=0;i<count;i++){
        attempted=i+1;
        CM_B(cm_apply_tree(&edits[i].feature,edits[i].edited));
#ifdef BWTL_CABINET_FAULT_TEST
        if(cm_test_fail_bend_after==i+1){cm_test_bend_fault_hit=1;error=PRO_TK_GENERAL_ERROR;goto done;}
#endif
    }
    for(int i=0;i<count;i++)CM_B(cm_bend_verified(&edits[i].feature,edits[i].new_angle,edits[i].new_radius,tx->scale));
    part_text_add(out,L"已调整 %d 道折弯，Creo 定义和实际折弯面均已核对。",count);
done:
    if(error&&attempted){
        for(int i=attempted-1;i>=0;i--)if(cm_apply_tree(&edits[i].feature,edits[i].original))cm_recovery_uncertain=1;
        for(int i=0;i<count;i++)if(cm_bend_verified(&edits[i].feature,edits[i].angle,edits[i].radius,tx->scale))cm_recovery_uncertain=1;
        ProMassProperty mass;
        if(qa_geometry_mass(tx->model,&mass)||!part_close(tx->volume,mass.volume))cm_recovery_uncertain=1;
    }
    for(int i=0;i<count;i++){
        if(edits[i].original)ProFeatureElemtreeFree(&edits[i].feature,edits[i].original);
        if(edits[i].edited)ProFeatureElemtreeFree(&edits[i].feature,edits[i].edited);
    }
    return error;
#undef CM_B
}
