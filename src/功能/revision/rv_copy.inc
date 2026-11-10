static ProError rv_copy(Revision *source,const PartCommand *cmd,PartText *out){
    ProMdl model=source->model,copy=NULL,active=NULL;
    ProMdlName name,actual;
    ProMdlsubtype before,after;
    ProWVerstamp initial=NULL,final=NULL;
    Revision duplicate;
    if(!part_target_available(model,cmd->arg[2],L"prt"))return PRO_TK_E_FOUND;
    wcscpy_s(name,PRO_NAME_SIZE,cmd->arg[2]);
    ProError error=ProMdlSubtypeGet(model,&before);
    if(!error)error=ProMdlVerstampGet(model,&initial);
    if(!error)error=ProMdlnameCopy(model,name,&copy);
    if(!error)error=ProMdlMdlnameGet(copy,actual);
    if(!error)error=ProMdlSubtypeGet(copy,&after);
    if(!error&&(copy==model||_wcsicmp(actual,name)||before!=after))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=rv_begin(&duplicate,copy);
    if(!error&&(!part_close(source->mass.volume,duplicate.mass.volume)||
        !part_close(source->mass.surface_area,duplicate.mass.surface_area)))error=PRO_TK_GENERAL_ERROR;
    if(!error&&cmd->count>3)error=rv_outline(&duplicate,cmd,3);
    if(!error)error=ProMdlVerstampGet(model,&final);
    if(!error)error=ProMdlCurrentGet(&active);
    if(!error&&(!ProVerstampEqual(initial,final)||active!=model||!rv_geometry_restored(source)))error=PRO_TK_GENERAL_ERROR;
    if(initial)ProVerstampFree(&initial);
    if(final)ProVerstampFree(&final);
    if(error&&copy&&copy!=model){
        if(ProMdlErase(copy)){
            rv_recovery_uncertain=1;
            part_text_add(out,L"未完成的副本未能从会话移除，请检查名称 %ls。",name);
        }else part_text_add(out,L"已从会话移除未完成的副本，原零件保留。");
    }
    if(!error)part_text_add(out,L"已创建%ls副本 %ls.prt，可在 Creo 的「在会话中」打开。原零件仍为当前模型。",
        cmd->count>3?L"并按目标外形改好":L"改型",name);
    return error;
}
