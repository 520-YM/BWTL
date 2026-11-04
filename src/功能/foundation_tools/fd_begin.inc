static ProError fd_begin(FdTxn *tx,ProMdl model,const wchar_t *name,ProFeattype type){
    ProModelitem item;ProMdlType mt;ProMdlsubtype subtype;ProMassProperty mass;ProUnitsystem us;ProUnititem mm,native;ProUnitConversion conv;ProError e;
    memset(tx,0,sizeof(*tx));tx->model=model;tx->stage="validate feature request";tx->rollback_ok=1;
    if(!fd_name(name))return PRO_TK_BAD_INPUTS;wcscpy_s(tx->name,PRO_NAME_SIZE,name);
    if((e=ProMdlTypeGet(model,&mt))||(e=ProMdlSubtypeGet(model,&subtype)))return e;
    if(mt!=PRO_MDL_PART||subtype==PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_BAD_CONTEXT;
    if(!ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,&item))return PRO_TK_E_FOUND;
    if((e=part_health(model)))return e;
    e=ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,tx);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;
    if((e=ProMdlPrincipalunitsystemGet(model,&us))||(e=ProUnitsystemUnitGet(&us,PRO_UNITTYPE_LENGTH,&native))||
       (e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&conv)))return e;
    if(!isfinite(conv.scale)||conv.scale<=0||conv.offset!=0)return PRO_TK_BAD_CONTEXT;tx->scale=conv.scale;
    tx->was_empty=!template_empty((ProSolid)model);
    if(!ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)){tx->volume=mass.volume;tx->has_volume=1;}
    if((e=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root)))return e;
    if((e=model_element(tx->root,PRO_E_FEATURE_TYPE,1,type,NULL)))return e;
    {ProElement el=NULL;if((e=model_element(tx->root,PRO_E_STD_FEATURE_NAME,0,0,&el))||(e=ProElementWstringSet(el,(wchar_t *)name)))return e;}
    if((e=ProMdlToModelitem(model,&item))||(e=ProSelectionAlloc(NULL,&item,&tx->model_selection)))return e;
    return ProArrayAlloc(1,sizeof(ProFeatureCreateOptions),1,(ProArray *)&tx->options);
}
