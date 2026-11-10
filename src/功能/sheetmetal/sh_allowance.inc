static ProError sh_child(ProElement parent,ProElemId id,ProElement *element){
    ProElemId ids[]={id};return mc_tree_element(parent,ids,1,element);
}
static ProError sh_int(ProElement parent,ProElemId id,int value){
    ProElement el;ProError e=sh_child(parent,id,&el);return e==PRO_TK_NO_ERROR?ProElementIntegerSet(el,value):model_element(parent,id,1,value,NULL);
}
static ProError sh_double(ProElement parent,ProElemId id,double value){
    ProElement el;ProError e=sh_child(parent,id,&el);return e==PRO_TK_NO_ERROR?ProElementDoubleSet(el,value):model_element(parent,id,2,value,NULL);
}
typedef struct {ProMdl model;double radius,angle,developed;int count,inside;ProSmtDvlLenCalcInfo calc;} ShBendInfo;
static ProError sh_bend_info(ProGeomitem *item,ProError status,ProAppData data){
    ShBendInfo *info=data;ProSurface surface;ProSmtSurfType type;ProBool inside;double r,a,d;ProSmtDvlLenCalcInfo calc;
    if(status)return status;
    if(ProGeomitemToSurface(item,&surface)||ProSmtSurfaceTypeGet((ProPart)info->model,surface,&type)||type!=PRO_SMT_SURF_FACE||
       ProSmtBendsrfInfoGet((ProPart)info->model,surface,&r,&inside,&a,&d,&calc))return 0;
    info->radius=r;info->inside=inside;info->angle=a;info->developed=d;info->calc=calc;info->count++;return 0;
}
static ProError sh_table_check(ProMdl source,const wchar_t *feature_name,const wchar_t *table_name){
    ProMdl copy=NULL;ProMdlName name;ProFeature feature;ProName table;ProDimension *dims=NULL;ProGeomitem *surfaces=NULL;
    ProMassProperty before,after;ShBendInfo info={0};ProError e;int n=0,captured=0;
#define TC(call) do{if((e=(call)))goto done;}while(0)
    TC(ProSolidMassPropertyWithDensityGet((ProSolid)source,NULL,PRO_MP_DENS_USE_ALWAYS,1,&before));captured=1;
    swprintf_s(name,PRO_MDLNAME_SIZE,L"bwtl_table_%08lx_%08lx",GetCurrentProcessId(),GetTickCount());TC(ProMdlnameCopy(source,name,&copy));
    TC(ProModelitemByNameInit(copy,PRO_FEATURE,(wchar_t *)feature_name,(ProModelitem *)&feature));
    TC(ProSmtFeatureDevldimsGet(&feature,&dims,&surfaces));TC(ProArraySizeGet((ProArray)dims,&n));
    for(int i=0;i<n;i++)TC(ProSmtDevldimDrivenSet(&dims[i],PRO_B_TRUE));
    wcscpy_s(table,PRO_NAME_SIZE,table_name);TC(ProSmtFeatureBendtableApply(&feature,table,PRO_B_TRUE));TC(part_regenerate(copy));TC(part_health(copy));
    info.model=copy;TC(ProFeatureGeomitemVisit(&feature,PRO_SURFACE,sh_bend_info,NULL,&info));
    if(!info.count||_wcsicmp(info.calc.bend_table,table_name)||info.developed<=0)e=PRO_TK_BAD_CONTEXT;
done:
    if(dims)ProArrayFree((ProArray *)&dims);if(surfaces)ProArrayFree((ProArray *)&surfaces);
    if(copy&&ProMdlErase(copy)){sh_recovery_uncertain=1;if(!e)e=PRO_TK_GENERAL_ERROR;}
    if(captured&&(ProSolidMassPropertyWithDensityGet((ProSolid)source,NULL,PRO_MP_DENS_USE_ALWAYS,1,&after)||!part_close(before.volume,after.volume))){sh_recovery_uncertain=1;e=PRO_TK_GENERAL_ERROR;}
    return e;
#undef TC
}
static ProError sh_allowance(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    ProFeature feature;ProElement original=NULL,tree=NULL,dev,factor;ProFeatureCreateOptions *options=NULL;ProErrorlist errors={0};
    ProDimension *dims=NULL,thickness;ProGeomitem *surfaces=NULL;ProBoolean driven[64]={0};double olddim[64]={0},t,expected=0,scale;
    ShBendInfo before={0},after={0};ProError e;int n=0,changed=0,is_bd=!wcscmp(cmd->arg[3],L"BD"),is_table=!wcscmp(cmd->arg[3],L"TABLE");ProName feature_name;
    FdTxn context={0};before.model=after.model=model;
#define BA(call) do{if((e=(call))){trace("sheetmetal allowance: " #call,e);goto done;}}while(0)
    /* A temporary transaction name only reads units and snapshots; no feature is created. */
    BA(sh_begin(&context,model,L"BWTL_ALLOWANCE_GUARD",PRO_FEAT_DATUM));scale=context.scale;
    wcscpy_s(feature_name,PRO_NAME_SIZE,cmd->arg[2]);BA(ProModelitemByNameInit(model,PRO_FEATURE,feature_name,(ProModelitem *)&feature));
    BA(ProFeatureGeomitemVisit(&feature,PRO_SURFACE,sh_bend_info,NULL,&before));if(!before.count){e=PRO_TK_BAD_INPUTS;goto done;}
    /* Native table loading may alter a feature even when it returns an error.
       Validate the requested table on an expendable model copy before any write. */
    if(is_table)BA(sh_table_check(model,feature_name,cmd->arg[4]));
    BA(ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&original));
    BA(ProArrayAlloc(1,sizeof(ProFeatureCreateOptions),1,(ProArray *)&options));options[0]=PRO_FEAT_CR_NO_OPTS;
    BA(ProSmtFeatureDevldimsGet(&feature,&dims,&surfaces));BA(ProArraySizeGet((ProArray)dims,&n));if(n<1||n>64){e=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<n;i++){BA(ProDimensionValueGet(&dims[i],&olddim[i]));BA(ProSmtDevldimIsDriven(&dims[i],&driven[i]));trace("sheetmetal allowance previous driven",driven[i]);}
    /* A prior explicit DEV.L (BD) must stop driving when selecting K or TABLE. */
    if(!is_bd){changed=1;for(int i=0;i<n;i++)BA(ProSmtDevldimDrivenSet(&dims[i],PRO_B_TRUE));}
    if(is_table){ProName table;wcscpy_s(table,PRO_NAME_SIZE,cmd->arg[4]);changed=1;BA(ProSmtFeatureBendtableApply(&feature,table,PRO_B_TRUE));}
    else if(is_bd){
        if(before.count!=1||fabs(before.angle)<=.01||fabs(before.angle)>=179){e=PRO_TK_BAD_INPUTS;goto done;}
        BA(ProSmtPartThicknessGet((ProPart)model,&thickness));BA(ProDimensionValueGet(&thickness,&t));
        expected=2*(before.radius+(before.inside?t:0))*tan(fabs(before.angle)*AS_PI/360)-as_arg(cmd,4)*scale;
        if(expected<=0){e=PRO_TK_BAD_INPUTS;goto done;}
        if(n!=1){e=PRO_TK_BAD_CONTEXT;goto done;}changed=1;
        BA(ProSmtDevldimDrivenSet(&dims[0],PRO_B_FALSE));BA(ProDimensionValueSet(&dims[0],expected));BA(part_regenerate(model));
    }else{
        BA(ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree));
        if(sh_child(tree,PRO_E_SMT_DEV_LEN_CALCULATION,&dev))BA(model_element(tree,PRO_E_SMT_DEV_LEN_CALCULATION,0,0,&dev));
        BA(sh_int(dev,PRO_E_SMT_DEV_LEN_SOURCE,PRO_DVL_SRC_FEAT_YF_ONLY));
        if(sh_child(dev,PRO_E_SMT_DEV_LEN_Y_FACTOR,&factor))BA(model_element(dev,PRO_E_SMT_DEV_LEN_Y_FACTOR,0,0,&factor));
        BA(sh_int(factor,PRO_E_SMT_DEV_LEN_Y_FACTOR_TYPE,PRO_FACTOR_K));BA(sh_double(factor,PRO_E_SMT_DEV_LEN_Y_FACTOR_VALUE,as_arg(cmd,4)));
        changed=1;BA(ProFeatureWithoptionsRedefine(NULL,&feature,tree,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors));
    }
    BA(part_regenerate(model));BA(part_health(model));BA(ProFeatureGeomitemVisit(&feature,PRO_SURFACE,sh_bend_info,NULL,&after));
    trace("sheetmetal allowance native method",after.calc.method);trace("sheetmetal allowance table name match",!_wcsicmp(after.calc.bend_table,cmd->arg[4]));
    if(!after.count||(is_bd&&!part_close(after.developed,expected))||(is_table&&_wcsicmp(after.calc.bend_table,cmd->arg[4]))||
       (!is_bd&&!is_table&&fabs(after.calc.y_factor_value-as_arg(cmd,4)*AS_PI/2)>1e-6)){e=PRO_TK_GENERAL_ERROR;goto done;}
    swprintf_s(report,capacity,L"已更新折弯特征 %ls 的 %ls。实际展开弧长 %.9g mm，重生成及原生读回通过；仅作用于该特征，未保存。",feature_name,cmd->arg[3],after.developed/scale);
done:
    if(e&&changed){
        ProError restore=ProFeatureWithoptionsRedefine(NULL,&feature,original,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);
        /* The table registry is model state beyond the extracted feature tree. */
        if(before.calc.bend_table[0]&&
           ProSmtFeatureBendtableApply(&feature,before.calc.bend_table,PRO_B_FALSE))restore=PRO_TK_GENERAL_ERROR;
        for(int i=0;i<n;i++){
            /* Table application can retain a stale stored DEV.L from an earlier BD.
               Restore a dimension value only when it actually controlled the bend. */
            if(before.calc.method==PRO_DVL_LEN_DRIVEN_BY_DIMENSION){
                if(ProSmtDevldimDrivenSet(&dims[i],PRO_B_FALSE)||ProDimensionValueSet(&dims[i],olddim[i])||ProSmtDevldimDrivenSet(&dims[i],driven[i]))restore=PRO_TK_GENERAL_ERROR;
            }else if(ProSmtDevldimDrivenSet(&dims[i],PRO_B_TRUE))restore=PRO_TK_GENERAL_ERROR;
        }
        if(restore||part_regenerate(model)||part_health(model))sh_recovery_uncertain=1;
        {ProMassProperty mass;ShBendInfo recovered={0};recovered.model=model;
         if(ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)||!part_close(context.volume,mass.volume)||
            ProFeatureGeomitemVisit(&feature,PRO_SURFACE,sh_bend_info,NULL,&recovered)||recovered.count!=before.count||!part_close(recovered.developed,before.developed))sh_recovery_uncertain=1;}
        swprintf_s(report,capacity,L"折弯扣除更新失败，错误码%d；%ls。",e,sh_recovery_uncertain?L"恢复未核实，请检查模型":L"已恢复原定义，未保存");
    }
    if(tree)ProFeatureElemtreeFree(&feature,tree);if(original)ProFeatureElemtreeFree(&feature,original);if(options)ProArrayFree((ProArray *)&options);
    if(dims)ProArrayFree((ProArray *)&dims);if(surfaces)ProArrayFree((ProArray *)&surfaces);fd_cleanup(&context,0);return e;
#undef BA
}
