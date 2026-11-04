__declspec(dllexport) void user_terminate(void) {
    foundation_reset();
    part_snapshot_clear(&request_snapshot);
    ai_cleanup();
    native_stop_poll();
    native_ui_stop_thread();
    if(native_backend_window)DestroyWindow(native_backend_window);
    UnregisterClassW(L"CreoCodexBackendV12",GetModuleHandleW(NULL));
    UnregisterClassW(L"CreoCodexNativeChatV11",GetModuleHandleW(NULL)); created=0;
    trace("user_terminate", 0);
}
