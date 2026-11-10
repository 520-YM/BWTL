static ProError md_stock_offset(FdTxn *tx,const PartCommand *cmd){
    FdRef faces[8]={{0}};ProCollection collection=NULL;ProElement surfaces,options;ProError e;
#define SO(call) do{if((e=(call)))goto done;}while(0)
    SO(ProSrfcollectionAlloc(&collection));
    for(int i=4;i<cmd->count;i++){SO(md_solid_face(tx,cmd->arg[i],&faces[i-4]));SO(rf_surface_add(tx,collection,&faces[i-4]));}
    SO(model_element(tx->root,PRO_E_STD_SURF_COLLECTION_APPL,0,0,&surfaces));SO(ProElementCollectionSet(surfaces,collection));
    SO(model_element(tx->root,PRO_E_OGF_OFFSET_VAL,2,as_arg(cmd,3)*tx->scale,NULL));
    SO(model_element(tx->root,PRO_E_OGF_DIR_OPT,1,PRO_FLXOGF_DIR_NORMAL,NULL));
    SO(model_element(tx->root,PRO_E_FLEX_OPTS_CMPND,0,0,&options));
    SO(model_element(options,PRO_E_FLEX_ATTACH_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));
    SO(model_element(options,PRO_E_FLEX_CR_RND_GEOM,1,PRO_FLEXMODEL_OPT_NO,NULL));
    SO(model_element(options,PRO_E_FLEX_PROPAGATE_TANGENCY,1,PRO_FLEXMODEL_OPT_NO,NULL));
    SO(model_element(options,PRO_E_FLEX_DFLT_CONDITIONS,1,PRO_FLEXMODEL_OPT_YES,NULL));
    SO(model_element(options,PRO_E_FLEX_TRF_SEL_ATT_GEOM,1,PRO_FLEXMODEL_OPT_YES,NULL));
    SO(model_element(options,PRO_E_FLEX_MAINTAIN_TOPO,1,PRO_FLEXMODEL_OPT_YES,NULL));
    SO(fd_create(tx,0));
done:if(collection)ProCollectionFree(&collection);for(int i=0;i<8;i++)fd_ref_free(&faces[i]);return e;
#undef SO
}
static ProError md_stock(const PartCommand *cmd,ProMdl source,wchar_t *report,size_t capacity){
    ProMdl copy=NULL;ProMdlName name;ProWVerstamp stamp=NULL,after=NULL;ProMassProperty before,mass;
    FdTxn tx={0};ProName feature_name;int bodies=0;ProError e;double volume;
    if(!part_target_available(source,cmd->arg[2],L"prt"))return PRO_TK_E_FOUND;
    wcscpy_s(name,PRO_MDLNAME_SIZE,cmd->arg[2]);
    swprintf_s(feature_name,PRO_NAME_SIZE,L"%.23ls_ALLOW",cmd->arg[2]);
    if((e=ProMdlVerstampGet(source,&stamp))||(e=ProSolidMassPropertyWithDensityGet((ProSolid)source,NULL,PRO_MP_DENS_USE_ALWAYS,1,&before))||
       (e=ProMdlnameCopy(source,name,&copy)))goto done;
    if((e=fd_begin(&tx,copy,feature_name,PRO_FEAT_FLX_OGF))||(e=sd_body_count(copy,&bodies)))goto done;
    if(bodies!=1||!tx.has_volume||tx.volume<=0){e=PRO_TK_BAD_CONTEXT;goto done;}
    if((e=md_stock_offset(&tx,cmd))||(e=mc_verify(&tx,PRO_FEAT_FLX_OGF,&volume))||
       (e=part_health(copy)))goto done;
    if(volume<=before.volume){e=PRO_TK_GENERAL_ERROR;goto done;}
    if((e=ProMdlVerstampGet(source,&after))||!ProVerstampEqual(stamp,after)||
       (e=ProSolidMassPropertyWithDensityGet((ProSolid)source,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass))||!part_close(mass.volume,before.volume)){if(!e)e=PRO_TK_GENERAL_ERROR;goto done;}
    swprintf_s(report,capacity,L"已从当前零件复制出毛坯 %ls，并向指定面增加 %.9g mm 余量。毛坯体积 %.9g mm³；原零件版本和体积保持，副本未保存、未切换窗口。副本为独立派生，不自动跟随原件后续修改。",name,as_arg(cmd,3),volume/pow(tx.scale,3));
done:
    am_finish(&tx,e,bodies,report,capacity);
    if(e&&copy){ProError erase=ProMdlErase(copy);trace("erase failed stock copy",erase);}
    if(stamp)ProVerstampFree(&stamp);if(after)ProVerstampFree(&after);return e;
}
