static LRESULT CALLBACK native_backend_proc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_APP+41){trace("Queue chat dispatch",ProMacroLoad(L"~ Command `CodexNativeSend`;"));return 0;}
 if(message==WM_APP+42){trace("Queue model inspection",ProMacroLoad(L"~ Command `CodexNativeInspect`;"));return 0;}
 if(message==WM_APP+43){on_closed(NULL,NULL,NULL);return 0;}
 if(message==WM_APP+44){ai_poll(NULL,NULL,NULL);return 0;}
 if(message==WM_APP+45){smart_stop();return 0;}
 if(message==WM_APP+48){
    if(!smart_details[0])append(L"本次会话暂无原生执行明细。");
    else if(wcslen(smart_details)<=21000)append_card(L"执行结果",smart_details);
    else {
        static wchar_t view[22000];
        swprintf_s(view,_countof(view),L"以下为最近的回执。完整记录位于 %ls\\last-native-details.txt\n\n%ls",
            runtime_data,smart_details_tail(21000));
        append_card(L"执行结果",view);
    }
    return 0;
 }
 if(message==BWTL_WORKDIR_REQUEST){queue_workdir_command((UINT)w);return 0;}
 if(message==WM_TIMER&&w==MARKUP_PROGRESS){markup_poll_intent();return 0;}
 if(message==MARKUP_CANCEL){markup_cancel_intent((unsigned long)l);return 0;}
 if(message==MARKUP_OPEN){markup_queue(1,NULL);return 0;}
 if(message==MARKUP_APPLY){markup_queue(2,(MarkupRequest *)l);return 0;}
 if(message==BWTL_IMITATION_REQUEST){imitation_queue_action((UINT)w);return 0;}
 return DefWindowProcW(window,message,w,l);
}
