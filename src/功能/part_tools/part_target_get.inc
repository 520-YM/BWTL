static ProError part_target_get(ProMdl *target){
    ProMdl window=NULL,active=NULL;ProMdlType type;ProError error;
    if((error=ProMdlCurrentGet(&window)))return error;
    if((error=ProMdlTypeGet(window,&type)))return error;
    if(type!=PRO_MDL_ASSEMBLY){*target=window;return 0;}
    if(!ProMdlActiveGet(&active) && active){*target=active;return 0;}
    return PRO_TK_BAD_CONTEXT;
}
