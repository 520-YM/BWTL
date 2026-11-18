static LRESULT CALLBACK native_backend_proc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_APP+41){trace("Queue chat dispatch",ProMacroLoad(L"~ Command `CodexNativeSend`;"));return 0;}
 if(message==WM_APP+42){trace("Queue model inspection",ProMacroLoad(L"~ Command `CodexNativeInspect`;"));return 0;}
 if(message==WM_APP+43){on_closed(NULL,NULL,NULL);return 0;}
 if(message==WM_APP+44){ai_poll(NULL,NULL,NULL);return 0;}
 if(message==WM_APP+45){fastener_open_cancel();if(!default_part_stop()){default_part_cancel();return_assembly_cancel();smart_stop();}return 0;}
 if(message==WM_APP+48){
    if(!smart_details[0])append(L"本次会话暂无原生执行明细。");
    else if(wcslen(smart_details)<=21000)append_card(L"执行结果",smart_details);
    else {
        static wchar_t view[22000];
        swprintf_s(view,_countof(view),L"以下为最近的回执。完整记录位于 %ls\\last-native-details.txt\n\n%ls",
            store_session_root,smart_details_tail(21000));
        append_card(L"执行结果",view);
    }
    return 0;
 }
 if(message==BWTL_FASTENER_MENU_REQUEST){fastener_menu_receive((wchar_t *)l);return 0;}
 if(message==BWTL_MODELING_RULE_VIEW){
    wchar_t description[1500]=L"";
    modeling_rule_describe(description,_countof(description));
    append_card(L"提示",description);
    return 0;
 }
 if(message==BWTL_WORKDIR_REQUEST){queue_workdir_command((UINT)w,(wchar_t *)l);return 0;}
 if(message==WM_TIMER&&w==MARKUP_PROGRESS){markup_poll_intent();return 0;}
 if(message==MARKUP_CANCEL){markup_cancel_intent((unsigned long)l);return 0;}
 if(message==MARKUP_OPEN){markup_queue(1,NULL);return 0;}
 if(message==MARKUP_APPLY){markup_queue(2,(MarkupRequest *)l);return 0;}
 if(message==BWTL_IMITATION_REQUEST){imitation_queue_action((UINT)w);return 0;}
 return DefWindowProcW(window,message,w,l);
}
