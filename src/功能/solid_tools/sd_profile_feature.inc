static ProError sd_profile_collect(ProGeomitem *item,ProError status,ProAppData data){
    FdCollect *found=data;
    ProCurve curve=NULL;
    ProEnttype type;
    ProError e;
    if(status)return status;
    if((e=ProCurveInit((ProSolid)item->owner,item->id,&curve))||(e=ProCurveTypeGet(curve,&type)))return e;
    if(type==PRO_ENT_LINE||type==PRO_ENT_ARC||type==PRO_ENT_CIRCLE){
        found->item=*item;
        found->count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError sd_profile_curve(ProMdl model,const wchar_t *name,FdRef *out){
    ProModelitem feature;
    FdCollect found={0};
    ProError e;
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,&feature))||
       (e=ProFeatureGeomitemVisit((ProFeature *)&feature,PRO_CURVE,sd_profile_collect,NULL,&found)))return e;
    if(found.count!=1)return found.count?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;
    out->item=found.item;
    out->owner_feature=feature.id;
    return ProSelectionAlloc(NULL,(ProModelitem *)&out->item,&out->selection);
}

static ProError sd_curve_collection(ProElement element,const FdRef *ref){
    ProCrvcollinstr instruction=NULL;
    ProCollection collection=NULL;
    ProReference reference=NULL;
    ProError e;
    if(!(e=ProSelectionToReference(ref->selection,&reference))&&
       !(e=ProCrvcollinstrAlloc(PRO_CURVCOLL_ADD_ONE_INSTR,&instruction))&&
       !(e=ProCrvcollinstrReferenceAdd(instruction,reference))&&
       !(e=ProCrvcollectionAlloc(&collection))&&
       !(e=ProCrvcollectionInstructionAdd(collection,instruction)))
        e=ProElementCollectionSet(element,collection);
    if(collection)ProCollectionFree(&collection);
    if(instruction)ProCrvcollinstrFree(instruction);
    if(reference)ProReferenceFree(reference);
    return e;
}

static ProError sd_profile_body(FdTxn *tx,int *before){
    ProElement body=NULL;
    ProError e=sd_body_count(tx->model,before);
    if(e)return e;
    if(*before>1||(*before==1&&(!tx->has_volume||!isfinite(tx->volume)||tx->volume<=0)))return PRO_TK_BAD_CONTEXT;
    if((e=model_element(tx->root,PRO_E_BODY,0,0,&body))||
       (e=model_element(body,PRO_E_BODY_USE,1,*before?PRO_BODY_USE_SELECTED:PRO_BODY_USE_NEW,NULL)))return e;
    return *before?sd_existing_body(tx,body):PRO_TK_NO_ERROR;
}

static ProError sd_profile_verify_change(FdTxn *tx,int form,const int *references,int count,int cut,double *volume){
    ProMassProperty mass;
    ProFeatStatus status;
    ProName name;
    ProElement tree=NULL,form_element=NULL;
    ProElempath path=NULL;
    ProElempathItem item={PRO_ELEM_PATH_ITEM_TYPE_ID,{PRO_E_FEATURE_FORM}};
    int *parents=NULL,n=0,i,j,bodies=0,actual_form;
    ProError e;
    if((e=part_regenerate(tx->model))||(e=part_health(tx->model))||
       (e=ProFeatureStatusGet(&tx->feature,&status))||
       (e=ProModelitemNameGet((ProModelitem *)&tx->feature,name))||
       (e=sd_body_count(tx->model,&bodies)))return e;
    if(status!=PRO_FEAT_ACTIVE||_wcsicmp(name,tx->name)||bodies!=1)return PRO_TK_GENERAL_ERROR;
    if(form){
        if((e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||
           (e=ProElempathAlloc(&path))||(e=ProElempathDataSet(path,&item,1))||
           (e=ProElemtreeElementGet(tree,path,&form_element))||
           (e=ProElementIntegerGet(form_element,NULL,&actual_form)))goto done;
        if(actual_form!=form){e=PRO_TK_GENERAL_ERROR;goto done;}
    }
    if((e=ProFeatureParentsGet(&tx->feature,&parents,&n)))goto done;
    for(i=0;i<count;i++){
        for(j=0;j<n&&parents[j]!=references[i];j++){}
        if(j==n){e=PRO_TK_E_NOT_FOUND;goto done;}
    }
    if((e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)))goto done;
    if(!isfinite(mass.volume)||mass.volume<=0||
       (cut?(!tx->has_volume||mass.volume>=tx->volume):mass.volume<=(tx->has_volume?tx->volume:0))){e=PRO_TK_GENERAL_ERROR;goto done;}
    *volume=mass.volume/(tx->scale*tx->scale*tx->scale);
done:
    if(path)ProElempathFree(&path);
    if(tree)ProFeatureElemtreeFree(&tx->feature,tree);
    if(parents)ProArrayFree((ProArray *)&parents);
    return e;
}

static ProError sd_profile_verify(FdTxn *tx,int form,const int *references,int count,double *volume){
    return sd_profile_verify_change(tx,form,references,count,0,volume);
}

static void sd_profile_finish(FdTxn *tx,ProError error,int bodies_before,wchar_t *report,size_t capacity){
    ProMassProperty mass;
    int i,bodies;
    if(error&&tx->errors.error_list){
        for(i=0;i<tx->errors.error_number&&i<32;i++){
            trace("profile feature element id",tx->errors.error_list[i].err_item_id);
            trace("profile feature element error",tx->errors.error_list[i].error);
        }
    }
    fd_cleanup(tx,error);
    if(error&&tx->created&&(part_health(tx->model)||sd_body_count(tx->model,&bodies)||bodies!=bodies_before||
       (bodies_before&&(ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)||
                        !part_close(mass.volume,tx->volume)))))tx->rollback_ok=0;
    if(error)swprintf_s(report,capacity,L"实体操作未完成：%hs，错误码%d。%ls，未保存。",tx->stage,error,
        tx->rollback_ok?L"未新增或已核对移除本次特征":L"恢复状态未能核实，请检查模型");
}
