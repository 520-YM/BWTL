#include <ProSmtFlattenForm.h>

static ProError smy_count_form(ProFeature *feature,ProError status,ProAppData context){
    if(status)return status;
    ProFeattype type;
    ProFeatStatus state;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error&&(type==PRO_FEAT_SMT_SKETCH_FORM||type==PRO_FEAT_FORM||type==PRO_FEAT_GEN_MERGE)){
        error=ProFeatureStatusGet(feature,&state);
        if(!error&&state==PRO_FEAT_ACTIVE)(*(int *)context)++;
    }
    return error;
}

static ProError smy_flatten_formed_feature(ProMdl model,const wchar_t *feature,int project){
    FdTxn tx={0};
    ProFeature formed;
    ProFeattype type;
    ProFeatStatus state;
    int count=0;
    ProError error=cm_feature_find(model,feature,&formed);
    if(!error)error=ProFeatureTypeGet(&formed,&type);
    if(!error&&type!=PRO_FEAT_SMT_SKETCH_FORM&&type!=PRO_FEAT_FORM)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProFeatureStatusGet(&formed,&state);
    if(!error&&state!=PRO_FEAT_ACTIVE)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProSolidFeatVisit((ProSolid)model,smy_count_form,NULL,&count);
    if(!error&&count!=1)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=sh_begin(&tx,model,L"BWTL_FORM_FLAT",PRO_FEAT_FLATTEN);
    if(!error)error=model_element(tx.root,PRO_E_SMT_FLATTEN_REF_TYPE,1,PRO_FLATTEN_FORM_ALLSEL,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SMT_FLATTEN_PROJ_CUTS,1,project,NULL);
    if(!error){trace("workshop form auto collector ready",0);error=fd_create(&tx,0);trace("workshop native flatten form result",error);}
    if(!error)error=smx_verify_changed_feature(&tx);
    if(!error)error=ProFeatureTypeGet(&tx.feature,&type);
    if(!error&&type!=PRO_FEAT_FLATTEN)error=PRO_TK_BAD_CONTEXT;
    fd_cleanup(&tx,error);
    return error;
}

static ProError smy_form_flat(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    wchar_t before[256],after[256];
    int project=!wcscmp(cmd->arg[5],L"CUTS");
    ProError error=smy_stamp(model,before);
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    ProMdl copy=NULL;
    if(!error)error=smx_copy_scratch(model,&copy);
    if(!error)error=smy_flatten_formed_feature(copy,cmd->arg[4],project);
    if(error)part_text_add(out,L"所选成形在隔离副本未能展平（Creo %d）。\n",error);
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d&&!error)error=PRO_TK_OUT_OF_MEMORY;
    ProPath dxf;
    if(!error)error=fab_flat_read(copy,work.stage,L"formed-flat",cmd->arg[3],d,dxf);
    if(copy){
        ProError erased=ProMdlErase(copy);
        if(!error)error=erased;
        if(erased)sh_recovery_uncertain=1;
    }
    if(!error)error=smy_stamp(model,after);
    if(!error&&wcscmp(before,after))error=PRO_TK_BAD_CONTEXT;
    FILE *log=NULL;
    if(!error)error=fab_csv_open(work.stage,L"formed-flat.csv",&log);
    if(!error&&fwprintf(log,L"成形,模式,外轮廓图元,孔槽图元,折弯线\n\"%ls\",%ls,%d,%d,%d\n",
        cmd->arg[4],cmd->arg[5],d->outline,d->holes,d->bends)<0)error=PRO_TK_CANT_WRITE;
    free(d);
    error=dv_stage_finish(&work,fab_close(log,error),out);
    if(!error)part_text_add(out,L"已在副本原生展平 %ls，%ls。下料 DXF 与图层统计已回读：%ls。\n",cmd->arg[4],
        project?L"启用成形切口投影":L"关闭成形切口投影",work.target);
    return error;
}
