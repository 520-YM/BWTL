static void native_layout(void){
    RECT r;int w,h,has_images,prompt_top,i,button_width;
    if(!native_window)return;
    GetClientRect(native_window,&r);w=r.right;h=r.bottom;
    if(w<40||h<300)return;
    AcquireSRWLockShared(&native_image_lock);
    has_images=native_image_count>0;
    ReleaseSRWLockShared(&native_image_lock);
    prompt_top=h-158-(has_images?58:0);
    MoveWindow(native_title,20,12,w-40,28,TRUE);
    MoveWindow(native_mode_control,20,44,206,160,TRUE);
    button_width=(w-286)/5;
    if(button_width>112)button_width=112;
    if(button_width>0)for(i=0;i<5;i++)
        MoveWindow(native_top_buttons[i],234+i*(button_width+8),44,button_width,26,TRUE);
    MoveWindow(native_history,20,76,w-40,prompt_top-86,TRUE);
    MoveWindow(native_prompt,20,prompt_top,w-40,106,TRUE);
    if(has_images)MoveWindow(native_preview,20,h-100,w-40,50,TRUE);
    ShowWindow(native_preview,has_images?SW_SHOWNA:SW_HIDE);
    MoveWindow(native_buttons[0],20,h-40,128,28,TRUE);
    MoveWindow(native_apply_button,158,h-40,112,28,TRUE);
    MoveWindow(native_buttons[1],w-292,h-40,128,28,TRUE);MoveWindow(native_stop_button,w-154,h-40,64,28,TRUE);MoveWindow(native_buttons[2],w-80,h-40,60,28,TRUE);
    if(native_render_source)native_ui_history(native_render_source);InvalidateRect(native_window,NULL,TRUE);
}
