/* Opening a component is a window operation, with success checked when
   Creo has handled the queued activation on its own command thread. */
static PartCommand component_open_pending;
static int component_open_waiting;

static void component_open_cancel(void){
    component_open_waiting=0;
    ax_open_cancel();
}

static int component_open_confirm(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(!component_open_waiting)return 0;
    component_open_waiting=0;
    if(!smart_active){ax_open_cancel();return 0;}
    wchar_t result[1200]=L"";
    ProError error=ax_open_readback(result,_countof(result));
    if(!error){
        smart_target_model=NULL;
        smart_binding_clear();
        smart_failures_without_progress=0;
        smart_last_failure_brief[0]=0;
    }
    smart_step(&component_open_pending,L"打开装配组件",error,
               result[0]?result:L"Creo 没有返回可确认的目标窗口。",0,1,0);
    return 0;
}
