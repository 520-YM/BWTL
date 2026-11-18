__declspec(dllexport) int user_initialize(int argc, char **argv, char *version, char *build, wchar_t error_buffer[80]) {
    uiCmdCmdId command;
    ProError error;
    if(!runtime_isolation_verify()){wcscpy_s(error_buffer,80,L"Use the isolated BWTL launcher; legacy bridge co-loading is refused.");return PRO_TK_BAD_CONTEXT;}
    if(!runtime_init())return PRO_TK_CANT_WRITE;
    if(!runtime_isolation_begin()){wcscpy_s(error_buffer,80,L"BWTL data is already in use or its isolation log is not writable.");return PRO_TK_E_IN_USE;}
    trace("user_initialize", 0);
    {
        WNDCLASSW cls={0};cls.lpfnWndProc=native_backend_proc;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"CreoCodexBackendV12";
        if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return PRO_TK_GENERAL_ERROR;
        native_backend_window=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,NULL,cls.hInstance,NULL);
        if(!native_backend_window)return PRO_TK_GENERAL_ERROR;
    }
    trace("BWTL v23 direct collaboration and vision", 0);
    wcscpy_s(smx_data_directory,_countof(smx_data_directory),runtime_data);
    wcscpy_s(ax_standard_data_directory,_countof(ax_standard_data_directory),runtime_data);
    store_ready = storage_output_directory(runtime_data)&&store_init(runtime_data)&&store_session_begin();
    if(!store_ready){
        wcscpy_s(error_buffer,80,L"BWTL 无法归档聊天，请检查 E 盘数据目录权限。");
        trace("Create process chat archive failed",(int)GetLastError());
        runtime_isolation_end();
        return PRO_TK_CANT_WRITE;
    }
    if (store_ready) {
        if (!store_read(L"recent-context.txt", conversation, 24000) || !store_read(L"memory.txt", memory, 12000)) store_ready = 0;
        {
            
            /* New auto mode never replays proposals left by an older run. */
            pending_plan.ready = 0;
            if (store_ready) store_write(L"pending-plan.txt", L"");
        }
    }
    trace("Persistent store ready", store_ready);
    trace("Modeling rule seed",modeling_rule_seed());
    if(store_ready)store_read(L"last-native-details.txt",smart_details,SMART_DETAILS_CAP);
    swprintf_s(native_attachment_root,MAX_PATH,L"%ls\\attachments",store_session_root);
    CreateDirectoryW(native_attachment_root,NULL);
    if(swprintf_s(ai_dir,MAX_PATH,L"%ls\\codex-session-%lu-%llu",runtime_data,GetCurrentProcessId(),GetTickCount64())<0||
       !CreateDirectoryW(ai_dir,NULL)||!storage_output_directory(ai_dir)||
       !resident_path(ai_output,L"answer.txt"))return PRO_TK_CANT_WRITE;
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
    error=ProCmdActionAdd("BWTLSmartContinue",smart_continue_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLComponentOpenConfirm",component_open_confirm,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLReturnAssemblyConfirm",return_assembly_confirm,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLFastenerMenu",fastener_menu_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLFastenerOpenConfirm",fastener_open_confirm,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLDefaultPartContinue",default_part_continue,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLAttachmentCleanup",attachment_cleanup_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLResidentTool",resident_tool_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);if(error)return error;
    error=ProCmdActionAdd("BWTLWorkingDirectory",working_directory_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    error=ProCmdActionAdd("BWTLImitateModel",imitation_ui_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    trace("Register model imitation",error);
    if(error)return error;
    error=ProCmdActionAdd("BWTLScreenshotMarkup",markup_native_command,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    trace("Register native shortcut", ProConfigoptSet(L"mapkey", L"cdx @MAPKEY_LABELBWTL;~ Command `CodexNativeOpen`;"));
    error=ProRibbonDefinitionfileLoad(L"toolkitribbonui.rbn");
    trace("Load self-contained BWTL ribbon",error);
    if(error!=PRO_TK_NO_ERROR&&error!=PRO_TK_E_IN_USE){
        swprintf_s(error_buffer,80,L"BWTL ribbon load failed: %d",error);
        return error;
    }
    return 0;
}
