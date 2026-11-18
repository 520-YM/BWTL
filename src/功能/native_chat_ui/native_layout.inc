static void native_layout_footer(int width,int height){
    HWND buttons[]={native_buttons[0],native_buttons[1],native_stop_button,native_buttons[2],native_attach_button,native_clear_button};
    const int left[]={MulDiv(20,native_dpi,96),width-MulDiv(292,native_dpi,96),
        width-MulDiv(154,native_dpi,96),width-MulDiv(80,native_dpi,96),MulDiv(156,native_dpi,96),MulDiv(242,native_dpi,96)};
    const int widths[]={MulDiv(128,native_dpi,96),MulDiv(128,native_dpi,96),
        MulDiv(64,native_dpi,96),MulDiv(60,native_dpi,96),MulDiv(78,native_dpi,96),MulDiv(74,native_dpi,96)};
    const int top=height-MulDiv(40,native_dpi,96);
    const int button_height=MulDiv(31,native_dpi,96);
    const UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW;
    HDWP positions=BeginDeferWindowPos(_countof(buttons));
    for(int i=0;positions&&i<_countof(buttons);i++)
        positions=DeferWindowPos(positions,buttons[i],NULL,left[i],top,widths[i],button_height,flags);
    if(!positions||!EndDeferWindowPos(positions)){
        for(int i=0;i<_countof(buttons);i++)
            SetWindowPos(buttons[i],NULL,left[i],top,widths[i],button_height,flags);
    }
}

static void native_layout(void){
    RECT r;
    int w,h,has_images,prompt_top,button_width;
    if(!native_window)return;
    GetClientRect(native_window,&r);w=r.right;h=r.bottom;
#define UI(px) MulDiv((px),native_dpi,96)
    if(w<UI(40)||h<UI(300))return;
    has_images=native_attachment_count()>0;
    prompt_top=h-UI(has_images?232:158);
    const int gutter=UI(20), width=w-2*gutter, history_top=UI(94);
    const int title_space=width-UI(224);
    const int title_width=title_space<UI(320)?title_space:UI(320);
    native_layout_footer(w,h);
    MoveWindow(native_title,gutter,UI(9),title_width,UI(32),TRUE);
    MoveWindow(native_elapsed_label,w-gutter-UI(212),UI(17),UI(212),UI(24),TRUE);
    button_width=(width-4*UI(8))/5;
    if(button_width>UI(112))button_width=UI(112);
    if(button_width>0)for(int i=0;i<5;i++)
        MoveWindow(native_top_buttons[i],gutter+i*(button_width+UI(8)),UI(44),button_width,UI(30),TRUE);
    MoveWindow(native_history,gutter,history_top,width,prompt_top-history_top-UI(10),TRUE);
    MoveWindow(native_prompt,gutter,prompt_top,width,UI(106),TRUE);
    GetClientRect(native_prompt,&r);
    r.left+=UI(10);
    r.top+=UI(8);
    r.right-=UI(10);
    r.bottom-=UI(8);
    if(r.right>r.left&&r.bottom>r.top)SendMessageW(native_prompt,EM_SETRECT,0,(LPARAM)&r);
    if(has_images)MoveWindow(native_preview,gutter,h-UI(120),width,UI(68),TRUE);
    ShowWindow(native_preview,has_images?SW_SHOWNA:SW_HIDE);
    if(native_render_source)native_ui_history(native_render_source);
    // The footer buttons are owner-drawn. Repaint all of them at their new positions.
    RECT footer={0,h-UI(49),w,h};
    RedrawWindow(native_window,&footer,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
    InvalidateRect(native_window,NULL,FALSE);
#undef UI
}
