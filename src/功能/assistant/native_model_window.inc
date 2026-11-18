static ProError native_activate_model_window(ProMdl model,const wchar_t *name,ProType type,int *window){
    ProMdl owner=NULL;
    int previous=-1;
    ProError error=ProWindowCurrentGet(&previous);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    error=ProMdlWindowGet(model,window);
    const int create=error==PRO_TK_E_NOT_FOUND;
    if(create)error=ProObjectwindowMdlnameCreate((wchar_t *)name,type,window);
    if(!error)error=ProWindowCurrentSet(*window);
    if(!error&&create)error=ProMdlDisplay(model);
    if(!error)error=ProWindowMdlGet(*window,&owner);
    if(!error&&owner!=model)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProWindowActivate(*window);
    if(error&&previous>=0)ProWindowCurrentSet(previous);
    return error;
}

