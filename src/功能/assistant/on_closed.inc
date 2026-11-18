static void on_closed(char *dialog, char *component, ProAppData data) {
    memset(&pending_part,0,sizeof(pending_part));pending_plan.ready=0;
    component_open_cancel();
    return_assembly_cancel();
    fastener_open_cancel();
    default_part_cancel();
    smart_cancel();
    native_stop_poll();
    if (native_window) ShowWindow(native_window, SW_HIDE);
}
