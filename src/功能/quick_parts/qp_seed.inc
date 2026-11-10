#include <ProDtmPln.h>
#include <ProDtmCsys.h>

static ProError quick_metric(ProMdl model){
    double mm;
    ProError error=qa_mm_per_unit(model,&mm);
    if(error||fabs(mm-1)<1e-10)return error;
    ProUnitsystem *systems=NULL;
    ProUnititem millimeter;
    if((error=ProUnitInit(model,L"mm",&millimeter)))return error;
    if((error=ProMdlUnitsystemsCollect(model,&systems)))return error;
    int count;
    error=ProArraySizeGet((ProArray)systems,&count);
    if(!error){
        error=PRO_TK_E_NOT_FOUND;
        for(int i=0;i<count;i++){
            ProUnititem unit;
            ProUnitConversion convert;
            if(!ProUnitsystemUnitGet(&systems[i],PRO_UNITTYPE_LENGTH,&unit)&&
               !ProUnitConversionCalculate(&unit,&millimeter,&convert)&&fabs(convert.scale-1)<1e-10&&convert.offset==0){
                error=ProMdlPrincipalunitsystemSet(model,&systems[i],PRO_UNITCONVERT_SAME_DIMS,PRO_B_FALSE,PRO_VALUE_UNUSED);
                break;
            }
        }
    }
    ProArrayFree((ProArray *)&systems);
    return error;
}

static ProError quick_datum(ProMdl model,const wchar_t *name,int plane){
    FdTxn tx={0};
    ProModelitem item;
    tx.model=model;
    ProError error=ProElementAlloc(PRO_E_FEATURE_TREE,&tx.root);
    if(!error)error=model_element(tx.root,PRO_E_FEATURE_TYPE,1,plane?PRO_FEAT_DATUM:PRO_FEAT_CSYS,NULL);
    if(!error){
        if(plane){
            ProElement array,entry;
            error=model_element(tx.root,PRO_E_DTMPLN_CONSTRAINTS,0,0,&array);
            if(!error)error=model_element(array,PRO_E_DTMPLN_CONSTRAINT,0,0,&entry);
            if(!error)error=model_element(entry,PRO_E_DTMPLN_CONSTR_TYPE,1,plane,NULL);
        }else error=model_element(tx.root,PRO_E_CSYS_METHOD,1,PRO_CSYS_DEFAULT,NULL);
    }
    if(!error)error=ProMdlToModelitem(model,&item);
    if(!error)error=ProSelectionAlloc(NULL,&item,&tx.model_selection);
    if(!error)error=ProArrayAlloc(1,sizeof(ProFeatureCreateOptions),1,(ProArray *)&tx.options);
    if(!error){
        tx.options[0]=PRO_FEAT_CR_NO_OPTS;
        error=ProFeatureWithoptionsCreate(tx.model_selection,tx.root,tx.options,PRO_REGEN_NO_FLAGS,&tx.feature,&tx.errors);
        if(error&&tx.errors.error_list)for(int i=0;i<tx.errors.error_number&&i<8;i++)
            trace("quick datum element",tx.errors.error_list[i].err_item_id);
    }
    if(!error)error=ProModelitemNameSet((ProModelitem *)&tx.feature,(wchar_t *)name);
    if(!error){
        tx.was_empty=1;
        wcscpy_s(tx.name,PRO_NAME_SIZE,name);
        error=fd_verify(&tx);
    }
    if(error)trace(plane?"quick default plane":"quick default csys",error);
    fd_cleanup(&tx,0);
    return error;
}

static ProError quick_seed(ProMdlName name,ProMdl *model){
    ProSolid solid=NULL;
    ProError error=ProSolidMdlnameCreate(name,PRO_MDLFILE_PART,&solid);
    if(error)return error;
    *model=(ProMdl)solid;
    error=quick_metric(*model);trace("quick metric system",error);if(error)return error;
    if((error=quick_datum(*model,L"PRT_CSYS_DEF",0)))return error;
    const wchar_t *const names[]={L"RIGHT",L"TOP",L"FRONT"};
    const int planes[]={PRO_DTMPLN_DEF_X,PRO_DTMPLN_DEF_Y,PRO_DTMPLN_DEF_Z};
    for(int i=0;i<_countof(planes);i++)if((error=quick_datum(*model,names[i],planes[i])))return error;
    return PRO_TK_NO_ERROR;
}
