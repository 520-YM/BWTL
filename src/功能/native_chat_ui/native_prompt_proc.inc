static LRESULT CALLBACK native_prompt_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_DROPFILES)return SendMessageW(native_window,message,w,l);
    if(message==WM_IME_STARTCOMPOSITION)native_composing=1;
    if(message==WM_IME_ENDCOMPOSITION)native_composing=0;
    if(message==WM_PASTE&&!native_busy){wchar_t error[256];if(native_images_clipboard(native_window,error,256)){native_attachment_changed();return 0;}if(error[0]){native_attachment_error(error);return 0;}}
    if(message==WM_KEYDOWN && w==VK_RETURN && !native_composing && !(GetKeyState(VK_SHIFT)&0x8000)){
        SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);return 0;
    }
    if(message==WM_CHAR && w==L'\r' && !native_composing && !(GetKeyState(VK_SHIFT)&0x8000))return 0;
    return CallWindowProcW(native_prompt_original,window,message,w,l);
}
