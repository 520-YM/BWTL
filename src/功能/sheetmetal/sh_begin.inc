/* Sheetmetal has its own entry; existing solid-only tool guards stay intact. */
static int sh_recovery_uncertain=0;
static ProError sh_create(FdTxn *tx){
    ProError e;tx->options[0]=PRO_FEAT_CR_NO_OPTS;
    e=ProFeatureWithoptionsCreate(tx->model_selection,tx->root,tx->options,PRO_REGEN_NO_FLAGS,&tx->feature,&tx->errors);
    if(tx->feature.owner==tx->model&&tx->feature.id>0&&!fd_was_feature(tx,tx->feature.id))tx->created=1;return e;
}
static ProError sh_begin(FdTxn *tx,ProMdl model,const wchar_t *name,ProFeattype type){
    ProModelitem item;ProMdlType mt;ProMassProperty mass;ProUnitsystem us;ProUnititem mm,native;ProUnitConversion conv;ProError e;
    memset(tx,0,sizeof(*tx));tx->model=model;tx->stage="prepare sheetmetal feature";tx->rollback_ok=1;
    if(!fd_name(name))return PRO_TK_BAD_INPUTS;wcscpy_s(tx->name,PRO_NAME_SIZE,name);
    if((e=ProMdlTypeGet(model,&mt)))return e;if(mt!=PRO_MDL_PART)return PRO_TK_BAD_CONTEXT;
    if(!ProModelitemByNameInit(model,PRO_FEATURE,tx->name,&item))return PRO_TK_E_FOUND;
    if((e=part_health(model)))return e;
    e=ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,tx);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;
    if((e=ProMdlPrincipalunitsystemGet(model,&us))||(e=ProUnitsystemUnitGet(&us,PRO_UNITTYPE_LENGTH,&native))||
       (e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&conv)))return e;
    if(!isfinite(conv.scale)||conv.scale<=0||conv.offset!=0)return PRO_TK_BAD_CONTEXT;tx->scale=conv.scale;
    tx->was_empty=!template_empty((ProSolid)model);
    if(!ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)){tx->volume=mass.volume;tx->has_volume=1;}
    if((e=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root))||(e=model_element(tx->root,PRO_E_FEATURE_TYPE,1,type,NULL)))return e;
    {ProElement el;if((e=model_element(tx->root,PRO_E_STD_FEATURE_NAME,0,0,&el))||(e=ProElementWstringSet(el,tx->name)))return e;}
    if((e=ProMdlToModelitem(model,&item))||(e=ProSelectionAlloc(NULL,&item,&tx->model_selection)))return e;
    return ProArrayAlloc(1,sizeof(ProFeatureCreateOptions),1,(ProArray *)&tx->options);
}
static ProError sh_fillet(FdTxn *tx,ProElement parent,double radius){
    ProElement f;ProError e;
    if((e=model_element(parent,PRO_E_SMT_FILLETS,0,0,&f))||
       (e=model_element(f,PRO_E_SMT_FILLETS_USE_RAD,1,PRO_B_TRUE,NULL))||
       (e=model_element(f,PRO_E_SMT_FILLETS_SIDE,1,PRO_BEND_RAD_INSIDE,NULL)))return e;
    return model_element(f,PRO_E_SMT_FILLETS_VALUE,2,radius*tx->scale,NULL);
}
static ProError sh_develop(FdTxn *tx){
    ProElement dev;ProError e=model_element(tx->root,PRO_E_SMT_DEV_LEN_CALCULATION,0,0,&dev);
    return e?e:model_element(dev,PRO_E_SMT_DEV_LEN_SOURCE,1,PRO_DVL_SRC_PART_YF_AND_BTAB,NULL);
}
static ProError sh_chain(ProElement element,FdRef *edge){
    ProCrvcollinstr instruction=NULL;ProCollection collection=NULL;ProReference reference=NULL;ProError e;
    if(!(e=ProSelectionToReference(edge->selection,&reference))&&!(e=ProCrvcollinstrAlloc(PRO_CURVCOLL_TAN_INSTR,&instruction))&&
       !(e=ProCrvcollinstrReferenceAdd(instruction,reference))&&!(e=ProCrvcollectionAlloc(&collection))&&!(e=ProCrvcollectionInstructionAdd(collection,instruction)))e=ProElementCollectionSet(element,collection);
    if(collection)ProCollectionFree(&collection);if(instruction)ProCrvcollinstrFree(instruction);if(reference)ProReferenceFree(reference);return e;
}
