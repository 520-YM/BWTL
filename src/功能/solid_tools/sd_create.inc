static ProError sd_create(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};FdRef sketch={0},axis={0};ProElement section=NULL,body=NULL,depth=NULL,from=NULL,to=NULL;
    ProSectionShape shape;ProMassProperty after;ProError e;ProFeatStatus status;
    int revolve_cut=!wcscmp(cmd->arg[0],L"SOLID_REVOLVE_CUT");
    int revolve=revolve_cut||!wcscmp(cmd->arg[0],L"SOLID_REVOLVE");
    int cut=revolve_cut||(!revolve&&!wcscmp(cmd->arg[4],L"CUT"));
    const wchar_t *operation=revolve?(cut?L"旋转切除":L"旋转加料"):(cut?L"拉伸切除":L"标准拉伸加料");
    int *parents=NULL,np=0,i,j,body_before=0,body_after=0;double amount=0,old_volume=0;
#define SD_TRY(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    SD_TRY("validate standard solid feature",fd_begin(&tx,model,cmd->arg[2],cut?PRO_FEAT_CUT:PRO_FEAT_PROTRUSION));
    SD_TRY("read active body count",sd_body_count(model,&body_before));if(body_before>1||(cut&&body_before!=1)){e=PRO_TK_BAD_CONTEXT;goto done;}
    if(!tx.was_empty){if(!tx.has_volume){e=PRO_TK_BAD_CONTEXT;goto done;}old_volume=tx.volume;}
    SD_TRY("resolve closed associative sketch",sd_sketch_ref(model,cmd->arg[3],&sketch,&shape));
    SD_TRY("solid feature form",model_element(tx.root,PRO_E_FEATURE_FORM,1,revolve?PRO_REVOLVE:PRO_EXTRUDE,NULL));
    SD_TRY("solid type",model_element(tx.root,PRO_E_EXT_SURF_CUT_SOLID_TYPE,1,PRO_EXT_FEAT_TYPE_SOLID,NULL));
    SD_TRY("material operation",model_element(tx.root,PRO_E_REMOVE_MATERIAL,1,cut?PRO_EXT_MATERIAL_REMOVE:PRO_EXT_MATERIAL_ADD,NULL));
    SD_TRY("solid not thin",model_element(tx.root,PRO_E_FEAT_FORM_IS_THIN,1,PRO_EXT_FEAT_FORM_NO_THIN,NULL));
    SD_TRY("extrusion direction",model_element(tx.root,PRO_E_STD_DIRECTION,1,!wcscmp(cmd->arg[revolve?6:7],L"POS")?PRO_EXT_CR_IN_SIDE_ONE:PRO_EXT_CR_IN_SIDE_TWO,NULL));
    SD_TRY("body options",model_element(tx.root,PRO_E_BODY,0,0,&body));
    SD_TRY("use solid body",model_element(body,PRO_E_BODY_USE,1,cut?PRO_BODY_USE_ALL:body_before?PRO_BODY_USE_SELECTED:PRO_BODY_USE_NEW,NULL));
    if(!cut&&body_before)SD_TRY("reference existing active body",sd_existing_body(&tx,body));
    SD_TRY("section reference",model_element(tx.root,PRO_E_STD_SECTION,0,0,&section));
    SD_TRY("link external sketch",fd_ref_element(&tx,section,PRO_E_SEC_USE_SKETCH,&sketch));
    if(revolve){
#include "solid_revolve.inc"
}else{
#include "solid_extrude.inc"
}
    SD_TRY("create standard solid",fd_create(&tx,0));
    SD_TRY("regenerate standard solid",part_regenerate(model));
    SD_TRY("read feature state",ProFeatureStatusGet(&tx.feature,&status));if(status!=PRO_FEAT_ACTIVE){e=PRO_TK_GENERAL_ERROR;goto done;}
    if(revolve_cut){
        ProFeattype type;ProName name;
        SD_TRY("read revolved cut feature type",ProFeatureTypeGet(&tx.feature,&type));
        SD_TRY("verify revolved cut feature name",ProModelitemNameGet((ProModelitem *)&tx.feature,name));
        if(type!=PRO_FEAT_CUT||_wcsicmp(name,tx.name)){e=PRO_TK_GENERAL_ERROR;goto done;}
    }
    SD_TRY("read solid parents",ProFeatureParentsGet(&tx.feature,&parents,&np));
    for(i=0;i<tx.parent_count;i++){for(j=0;j<np;j++)if(parents[j]==tx.parents[i])break;if(j==np){e=PRO_TK_GENERAL_ERROR;goto done;}}
    SD_TRY("verify resulting body count",sd_body_count(model,&body_after));if(body_after!=1){e=PRO_TK_GENERAL_ERROR;goto done;}
    SD_TRY("read resulting solid volume",ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&after));
    if(!isfinite(after.volume)||after.volume<=0||(cut?after.volume>=old_volume:after.volume<=old_volume)){e=PRO_TK_GENERAL_ERROR;goto done;}
    swprintf_s(report,capacity,L"已在当前零件创建%ls %ls，引用草绘%ls。特征ID%d，体积 %.9g → %.9g mm³；单实体、父参照及重生成核对通过。未另建零件，未保存。",operation,cmd->arg[2],cmd->arg[3],tx.feature.id,old_volume/(tx.scale*tx.scale*tx.scale),after.volume/(tx.scale*tx.scale*tx.scale));
done:
    if(e&&tx.errors.error_list){for(i=0;i<tx.errors.error_number&&i<32;i++){trace("solid element error id",tx.errors.error_list[i].err_item_id);trace("solid element error code",tx.errors.error_list[i].error);}}
    if(parents)ProArrayFree((ProArray *)&parents);fd_ref_free(&sketch);fd_ref_free(&axis);fd_cleanup(&tx,e);
    if(e&&revolve_cut&&tx.created&&
       (part_health(model)||sd_body_count(model,&body_after)||body_after!=body_before||
        ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&after)||
        !part_close(after.volume,old_volume)))tx.rollback_ok=0;
    if(e)swprintf_s(report,capacity,L"实体操作未完成：%hs，错误码%d。%ls，未保存。",tx.stage,e,tx.rollback_ok?L"未新增或已核对移除本次特征":L"恢复状态未能核实，请检查模型");return e;
#undef SD_TRY
}
