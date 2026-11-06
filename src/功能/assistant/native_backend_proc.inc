static LRESULT CALLBACK native_backend_proc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_APP+41){trace("Queue chat dispatch",ProMacroLoad(L"~ Command `CodexNativeSend`;"));return 0;}
 if(message==WM_APP+42){trace("Queue model inspection",ProMacroLoad(L"~ Command `CodexNativeInspect`;"));return 0;}
 if(message==WM_APP+43){on_closed(NULL,NULL,NULL);return 0;}
 if(message==WM_APP+44){if(!development_poll())ai_poll(NULL,NULL,NULL);return 0;}
 if(message==WM_APP+45){development_cancel();smart_cancel();append(L"已停止当前任务；已经完成的模型操作保持，未自动保存。");return 0;}
 if(message==WM_APP+46){development_apply();return 0;}
 if(message==WM_APP+48){if(smart_details[0])append_card(L"执行结果",smart_details);else append(L"本次会话暂无原生执行明细。");return 0;}
 if(message==BWTL_WORKDIR_REQUEST){queue_workdir_command((UINT)w);return 0;}
 return DefWindowProcW(window,message,w,l);
}
