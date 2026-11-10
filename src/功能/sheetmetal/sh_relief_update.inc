static ProError sh_relief_update(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn context={0};ProFeature feature;ProFeattype type;ProElement original=NULL,tree=NULL,relief,side;
    ProFeatureCreateOptions *options=NULL;ProErrorlist errors={0};ProError e;int changed=0;ProName name;
#define RU(call) do{if((e=(call)))goto done;}while(0)
    RU(sh_begin(&context,model,L"BWTL_RELIEF_GUARD",PRO_FEAT_DATUM));wcscpy_s(name,PRO_NAME_SIZE,cmd->arg[2]);
    RU(ProModelitemByNameInit(model,PRO_FEATURE,name,(ProModelitem *)&feature));RU(ProFeatureTypeGet(&feature,&type));
    if(type!=PRO_FEAT_BEND&&type!=PRO_FEAT_WALL){e=PRO_TK_BAD_INPUTS;goto done;}
    RU(ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&original));RU(ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree));
    if(sh_child(tree,PRO_E_SMT_BEND_RELIEF,&relief))RU(model_element(tree,PRO_E_SMT_BEND_RELIEF,0,0,&relief));
    for(int i=0;i<2;i++){
        ProElemId id=i?PRO_E_SMT_BEND_RELIEF_SIDE2:PRO_E_SMT_BEND_RELIEF_SIDE1;
        if(sh_child(relief,id,&side))RU(model_element(relief,id,0,0,&side));
        RU(sh_int(side,PRO_E_BEND_RELIEF_TYPE,!wcscmp(cmd->arg[4],L"RECT")?PRO_BEND_RLF_RECTANGULAR:PRO_BEND_RLF_OBROUND));
        RU(sh_double(side,PRO_E_BEND_RELIEF_WIDTH,as_arg(cmd,5)*context.scale));RU(sh_int(side,PRO_E_BEND_RELIEF_DEPTH_TYPE,PRO_BEND_RLF_DEPTH_BLIND));
        RU(sh_double(side,PRO_E_BEND_RELIEF_DEPTH,as_arg(cmd,6)*context.scale));RU(sh_int(side,PRO_E_BEND_RELIEF_LENGTH_TYPE,PRO_BEND_RLF_LENGTH_BLIND));
        RU(sh_double(side,PRO_E_BEND_RELIEF_LENGTH,as_arg(cmd,7)*context.scale));
    }
    RU(ProArrayAlloc(1,sizeof(ProFeatureCreateOptions),1,(ProArray *)&options));options[0]=PRO_FEAT_CR_NO_OPTS;changed=1;
    RU(ProFeatureWithoptionsRedefine(NULL,&feature,tree,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors));RU(part_regenerate(model));RU(part_health(model));
    for(int i=0;i<2;i++){
        ProElemId path[]={PRO_E_SMT_BEND_RELIEF,i?PRO_E_SMT_BEND_RELIEF_SIDE2:PRO_E_SMT_BEND_RELIEF_SIDE1,PRO_E_BEND_RELIEF_TYPE};
        RU(mc_tree_value(&feature,path,3,!wcscmp(cmd->arg[4],L"RECT")?PRO_BEND_RLF_RECTANGULAR:PRO_BEND_RLF_OBROUND,1));
        path[2]=PRO_E_BEND_RELIEF_WIDTH;RU(mc_tree_value(&feature,path,3,as_arg(cmd,5)*context.scale,0));
        path[2]=PRO_E_BEND_RELIEF_DEPTH;RU(mc_tree_value(&feature,path,3,as_arg(cmd,6)*context.scale,0));
        path[2]=PRO_E_BEND_RELIEF_LENGTH;RU(mc_tree_value(&feature,path,3,as_arg(cmd,7)*context.scale,0));
    }
    swprintf_s(report,capacity,L"已将折弯／板壁特征 %ls 两端的避让设为%ls，宽 %.9g、深 %.9g、长度 %.9g mm。原生重定义和重生成通过，未保存。",name,cmd->arg[4],as_arg(cmd,5),as_arg(cmd,6),as_arg(cmd,7));
done:
    if(e&&changed&&(ProFeatureWithoptionsRedefine(NULL,&feature,original,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors)||part_regenerate(model)||part_health(model)))sh_recovery_uncertain=1;
    if(original)ProFeatureElemtreeFree(&feature,original);if(tree)ProFeatureElemtreeFree(&feature,tree);if(options)ProArrayFree((ProArray *)&options);fd_cleanup(&context,0);return e;
#undef RU
}
