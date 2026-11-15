static ProError replay_validate_pick(ProMdl model,ProType type,const ReplayGeometry *search,ProSelection pick,ProModelitem *target)
{
    ProModelitem selected;
    ProError error=ProSelectionModelitemGet(pick,&selected);
    if(error)return error;
    if(selected.owner!=model||selected.type!=type)return PRO_TK_INVALID_ITEM;
    for(int i=0;i<search->count;i++)if(search->ids[i]==selected.id){
        *target=selected;
        return PRO_TK_NO_ERROR;
    }
    return PRO_TK_INVALID_ITEM;
}

static ProError replay_manual_reference(ImitationReplay *replay,ProFeature *feature,ProType type,
    const ReplayGeometry *search,ProModelitem *target)
{
    int old_window=-1,window;
    ProError error=ProWindowCurrentGet(&old_window);
    if(error)return error;
    error=ProMdlWindowGet(replay->target,&window);
    const int existing=!error;
    if(!existing){
        error=dv_display_new(replay->target);
        if(!error)error=ProMdlWindowGet(replay->target,&window);
    }else error=ProWindowActivate(window);
    if(error)return error;

    wchar_t message[256];
    swprintf_s(message,_countof(message),L"源特征 %d 需要指定参照。请在新件中，从上游特征 ID %d 的 %d 个候选中选择一个%ls。",
        replay->current_feature,feature->id,search->count,type==PRO_EDGE?L"边":L"面");
    const int visible=native_window&&IsWindowVisible(native_window);
    if(visible)ShowWindow(native_window,SW_HIDE);
    int proceed=MessageBoxW(NULL,message,L"BWTL · 指定参照",MB_OKCANCEL|MB_ICONINFORMATION|MB_SETFOREGROUND);
    error=PRO_TK_USER_ABORT;
    if(proceed==IDOK){
        ProSelection *items=NULL;
        int count=0;
        error=ProSelect(type==PRO_EDGE?"edge":"surface",1,NULL,NULL,NULL,NULL,&items,&count);
        if(!error)error=count==1?replay_validate_pick(replay->target,type,search,items[0],target):PRO_TK_USER_ABORT;
    }
    ProError restore=ProWindowCurrentSet(old_window);
    if(!restore)restore=ProWindowActivate(old_window);
    if(!restore&&!existing&&window!=old_window)restore=ProWindowDelete(window);
    if(!error)error=restore;
    if(visible)ShowWindow(native_window,SW_SHOWNA);
    if(!error)part_text_add(replay->out,L"已在新件指定参照 ID %d。\n",target->id);
    if(error==PRO_TK_INVALID_ITEM)part_text_add(replay->out,L"所选几何不在本步骤的候选范围内。\n");
    return error;
}
