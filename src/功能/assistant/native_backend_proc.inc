static LRESULT CALLBACK native_backend_proc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_APP+41){trace("Queue chat dispatch",ProMacroLoad(L"~ Command `CodexNativeSend`;"));return 0;}
 if(message==WM_APP+42){trace("Queue model inspection",ProMacroLoad(L"~ Command `CodexNativeInspect`;"));return 0;}
 if(message==WM_APP+43){on_closed(NULL,NULL,NULL);return 0;}
 if(message==WM_APP+44){ai_poll(NULL,NULL,NULL);return 0;}
 return DefWindowProcW(window,message,w,l);
}
