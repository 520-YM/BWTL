__declspec(dllexport) void user_terminate(void) {
    markup_ai_cleanup();
    markup_capture_clear();
    foundation_reset();
    part_snapshot_clear(&request_snapshot);
    default_part_cancel();
    return_assembly_cancel();
    fastener_open_cancel();
    fastener_saved_clear();
    smart_cancel();native_attachments_clear();
    resident_shutdown();
    native_stop_poll();
    native_ui_stop_thread();
    if(native_backend_window)DestroyWindow(native_backend_window);
    UnregisterClassW(L"CreoCodexBackendV12",GetModuleHandleW(NULL));
    UnregisterClassW(L"CreoCodexNativeChatV11",GetModuleHandleW(NULL)); created=0;
    trace("user_terminate", 0);
    runtime_isolation_end();
}
