static ProError sh_flange(FdTxn *tx,const PartCommand *cmd){
    FdRef edge={0};ProElement angle,section,el,height,relief,side;ProSection profile=NULL;ProError e;int attached=0;
#define FF(call) do{if((e=(call)))goto done;}while(0)
    FF(fd_ref_resolve(tx->model,cmd->arg[3],PRO_EDGE,&edge));
    FF(model_element(tx->root,PRO_E_SMT_WALL_TYPE,1,PRO_SMT_WALL_TYPE_FLAT,NULL));FF(fd_ref_element(tx,tx->root,PRO_E_SMT_FLAT_WALL_ATT_EDGE,&edge));
    FF(model_element(tx->root,PRO_E_SMT_FLAT_WALL_ANGLE,0,0,&angle));FF(model_element(angle,PRO_E_SMT_FLAT_WALL_ANGLE_TYPE,1,PRO_BND_ANGLE_VALUE,NULL));
    FF(model_element(angle,PRO_E_SMT_FLAT_WALL_ANGLE_VAL,2,as_arg(cmd,5),NULL));FF(model_element(angle,PRO_E_SMT_FLAT_WALL_ANGLE_FLIP,1,!wcscmp(cmd->arg[7],L"NEG"),NULL));
    FF(sh_stock_section(0,&profile));
    FF(model_element(tx->root,PRO_E_STD_SECTION,0,0,&section));FF(model_element(section,PRO_E_SKETCHER,0,0,&el));FF(ProElementSpecialvalueSet(el,profile));attached=1;
    FF(sh_fillet(tx,tx->root,as_arg(cmd,6)));FF(model_element(tx->root,PRO_E_SMT_WALL_HEIGHT,0,0,&height));FF(model_element(height,PRO_E_SMT_WALL_HEIGHT_TYPE,1,PRO_BEND_POSITION_PROF_ON_EDGE,NULL));
    FF(model_element(tx->root,PRO_E_SMT_BEND_RELIEF,0,0,&relief));for(int i=0;i<2;i++){FF(model_element(relief,i?PRO_E_SMT_BEND_RELIEF_SIDE2:PRO_E_SMT_BEND_RELIEF_SIDE1,0,0,&side));FF(model_element(side,PRO_E_BEND_RELIEF_TYPE,1,PRO_BEND_RLF_RIP,NULL));}
    FF(model_element(tx->root,PRO_E_SMT_WALL_THICKNESS_FLIP,1,PRO_B_FALSE,NULL));FF(sh_develop(tx));FF(sh_create(tx));FF(sh_profile_resize(tx,as_arg(cmd,4),0));
done:if(attached)ProElementSpecialvalueSet(el,NULL);fd_ref_free(&edge);return e;
#undef FF
}
