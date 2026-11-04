static void native_layout(void) {
    RECT r; int w,h,history_height;
    if (!native_window) return;
    GetClientRect(native_window,&r); w=r.right; h=r.bottom;
    history_height=h-270; if(history_height<60)history_height=60;
    MoveWindow(native_title,20,14,w-40,28,TRUE);
    MoveWindow(native_heading,20,46,w-40,22,TRUE);
    MoveWindow(native_history,20,82,w-40,history_height,TRUE);
    MoveWindow(native_input_label,20,92+history_height,w-40,20,TRUE);
    MoveWindow(native_prompt,20,116+history_height,w-40,100,TRUE);
    MoveWindow(native_buttons[0],20,h-42,150,30,TRUE);
    MoveWindow(native_buttons[1],w-252,h-42,136,30,TRUE);
    MoveWindow(native_buttons[2],w-108,h-42,88,30,TRUE);
    if(native_render_source)native_ui_history(native_render_source);
    InvalidateRect(native_window,NULL,TRUE);
}
