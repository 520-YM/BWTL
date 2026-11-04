__declspec(dllexport) int user_initialize(int argc, char **argv, char *version, char *build, wchar_t error_buffer[80]) {
    uiCmdCmdId command;
    ProError error;
    if(!runtime_init())return PRO_TK_CANT_WRITE;
    trace("user_initialize", 0);
    {
        WNDCLASSW cls={0};cls.lpfnWndProc=native_backend_proc;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"CreoCodexBackendV12";
        if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return PRO_TK_GENERAL_ERROR;
        native_backend_window=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,NULL,cls.hInstance,NULL);
        if(!native_backend_window)return PRO_TK_GENERAL_ERROR;
    }
    trace("Native assistant v21.2 self-contained ribbon and standard modeling", 0);
    store_ready = store_init(runtime_data);
    if (store_ready) {
        if (!store_read(L"recent-context.txt", conversation, 24000) || !store_read(L"memory.txt", memory, 12000)) store_ready = 0;
        {
            
            /* New auto mode never replays proposals left by an older run. */
            pending_plan.ready = 0;
            if (store_ready) store_write(L"pending-plan.txt", L"");
        }
    }
    trace("Persistent store ready", store_ready);
    {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        swprintf_s(ai_dir, MAX_PATH, L"%lsCreoCodex-%lu", temp, GetCurrentProcessId());
        CreateDirectoryW(ai_dir, NULL);
        swprintf_s(ai_output, MAX_PATH, L"%ls\\answer.txt", ai_dir);
        /* Per-process temporary files keep simultaneous Creo sessions separate. */
    
    }
    error = ProCmdActionAdd("CodexNativeOpen", open_window, uiProeImmediate, access_command, PRO_B_TRUE, PRO_B_TRUE, &command);
    trace("ProCmdActionAdd", error);
    if (error != PRO_TK_NO_ERROR) { swprintf_s(error_buffer,80,L"Command registration failed: %d",error); return error; }
    error = ProCmdDesignate(command, "CodexNativeLabel", "CodexNativeHelp", "CodexNativeDescription", L"codex_native_messages.txt");
    trace("ProCmdDesignate", error);
    error = ProCmdActionAdd("CodexNativeBuildPending", build_pending_command, uiProe2ndImmediate, access_command, PRO_B_TRUE, PRO_B_TRUE, &command);
    trace("Register native modeling command", error);
    if (error != PRO_TK_NO_ERROR) return error;
    error=ProCmdActionAdd("CodexNativeVerifyActive",verify_active_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    trace("Register active model verification",error);
    if(error!=PRO_TK_NO_ERROR)return error;
    error=ProCmdActionAdd("CodexNativeSend",send_native_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    error=ProCmdActionAdd("CodexNativeInspect",inspect_native_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    trace("Register native shortcut", ProConfigoptSet(L"mapkey", L"cdx @MAPKEY_LABELCodex Assistant;~ Command `CodexNativeOpen`;"));
    error=ProRibbonDefinitionfileLoad(L"toolkitribbonui.rbn");
    trace("Load self-contained BWTL ribbon",error);
    if(error!=PRO_TK_NO_ERROR&&error!=PRO_TK_E_IN_USE){
        swprintf_s(error_buffer,80,L"BWTL ribbon load failed: %d",error);
        return error;
    }
    return 0;
}
