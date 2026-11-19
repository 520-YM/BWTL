/* Opening a component is a window operation, with success checked when
   Creo has handled the queued activation on its own command thread. */
static PartCommand component_open_pending;
static int component_open_waiting;

static void component_open_cancel(void){
    component_open_waiting=0;
    ax_open_cancel();
}

static int smart_navigation_binding(ProMdl target,const wchar_t *directory){
    if(!smart_binding_set(target,target,directory))return 0;
    if(part_snapshot_take(target,&request_snapshot)||request_snapshot.window_model!=target||
       _wcsicmp(request_snapshot.directory,directory)||!smart_binding_valid()){
        smart_binding_clear();
        part_snapshot_clear(&request_snapshot);
        return 0;
    }
    smart_target_model=target;
    request_requires_current=1;
    smart_failures_without_progress=0;
    smart_last_failure_brief[0]=0;
    return 1;
}

static int component_open_confirm(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(!component_open_waiting)return 0;
    component_open_waiting=0;
    if(!smart_active){ax_open_cancel();return 0;}
    const AxOpenPending pending=ax_open_pending;
    const int continuing=!smart_local_plan&&!smart_readonly&&(smart_chain||resident_tool_running());
    wchar_t result[1200]=L"";
    ProError error=ax_open_readback(result,_countof(result));
    if(!error&&continuing){
        AxReturnLink link={.root=pending.root,.child=pending.target,.root_window=pending.root_window,.child_window=pending.window};
        wcscpy_s(link.occurrence,_countof(link.occurrence),pending.occurrence);
        wcscpy_s(link.directory,_countof(link.directory),pending.directory);
        if(pending.root!=request_snapshot.model||pending.root!=request_snapshot.window_model||
           _wcsicmp(pending.directory,request_snapshot.directory)||!ax_return_link_live(&link)||
           !smart_navigation_binding(pending.target,pending.directory)){
            error=PRO_TK_BAD_CONTEXT;
            wcscpy_s(result,_countof(result),L"Creo 已切换窗口，但未能核对本轮父子关系、工作目录或子件快照。已保留之前完成的操作。");
        }else{
            smart_parent_link=link;
            wcscat_s(result,_countof(result),L" 已核对本轮的装配实例和工作目录，后续操作使用这个子件的新快照。需要回原装配继续时，调用 CREO_RETURN_PARENT。");
            trace("Verified and bound task component window",0);
        }
    }
    smart_step(&component_open_pending,L"打开装配组件",error,
               result[0]?result:L"Creo 没有返回可确认的目标窗口。",0,!continuing,
               continuing&&error&&!smart_binding_valid());
    return 0;
}
