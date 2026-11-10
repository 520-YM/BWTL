static ProError sf_verify(FdTxn *tx,int before_bodies,int material,const wchar_t *quilt_token,SfArea *area){
    ProFeatStatus status;
    ProName name;
    ProMassProperty mass;
    int bodies,*parents=NULL,n=0;
    ProError e;
    FdRef result={0};
    wchar_t token[64];
    if((e=part_regenerate(tx->model))||(e=part_health(tx->model))||
       (e=ProFeatureStatusGet(&tx->feature,&status))||
       (e=ProModelitemNameGet((ProModelitem *)&tx->feature,name))||
       (e=sd_body_count(tx->model,&bodies)))return e;
    if(status!=PRO_FEAT_ACTIVE||_wcsicmp(name,tx->name))return PRO_TK_GENERAL_ERROR;
    if((e=ProFeatureParentsGet(&tx->feature,&parents,&n)))return e;
    for(int i=0;i<tx->parent_count;i++){
        int j;
        for(j=0;j<n&&parents[j]!=tx->parents[i];j++){}
        if(j==n){e=PRO_TK_E_NOT_FOUND;goto done;}
    }
    if(material){
        if(bodies!=material){e=PRO_TK_GENERAL_ERROR;goto done;}
        e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);
        if(!e&&(!isfinite(mass.volume)||mass.volume<=0))e=PRO_TK_GENERAL_ERROR;
        if(!e&&material>before_bodies&&before_bodies&&mass.volume<=tx->volume)e=PRO_TK_GENERAL_ERROR;
    }else{
        if(bodies!=before_bodies){e=PRO_TK_GENERAL_ERROR;goto done;}
        if(bodies){
            if((e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)))goto done;
            if(!part_close(mass.volume,tx->volume)){e=PRO_TK_GENERAL_ERROR;goto done;}
        }
        if(quilt_token)wcscpy_s(token,_countof(token),quilt_token);
        else swprintf_s(token,_countof(token),L"QUILT:%ls",tx->name);
        if((e=sf_ref(tx->model,token,&result))||(e=sf_area(&result,area)))goto done;
    }
done:
    fd_ref_free(&result);
    if(parents)ProArrayFree((ProArray *)&parents);
    return e;
}
static void sf_finish(FdTxn *tx,ProError e,int before_bodies,wchar_t *report,size_t capacity){
    sd_profile_finish(tx,e,before_bodies,report,capacity);
    if(e)swprintf_s(report,capacity,L"曲面／实体操作未完成：%hs，错误码%d。%ls，未保存。",tx->stage,e,
        tx->rollback_ok?L"未新增或已核对移除本次特征":L"恢复状态未能核实，请检查模型");
}
