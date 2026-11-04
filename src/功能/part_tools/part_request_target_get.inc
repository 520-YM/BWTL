static ProError part_request_target_get(const wchar_t *request,ProMdl *target){
    if(wcsstr(request,L"当前装配")||wcsstr(request,L"顶层装配")||wcsstr(request,L"总装配")){
        ProMdl window=NULL;ProMdlType type;ProError e=ProMdlCurrentGet(&window);
        if(!e&&!ProMdlTypeGet(window,&type)&&type==PRO_MDL_ASSEMBLY){*target=window;return 0;}
    }
    return part_target_get(target);
}
