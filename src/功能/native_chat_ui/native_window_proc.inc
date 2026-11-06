static LRESULT CALLBACK native_window_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    switch(message){
    case CHAT_HISTORY_TEXT:return native_ui_history((const wchar_t *)l);
    case CHAT_BUSY:
        InterlockedExchange(&native_busy,(LONG)w);
        SetWindowTextW(native_buttons[1],w?L"处理中…":L"发送给 AI");
        EnableWindow(native_buttons[1],!w);EnableWindow(native_mode_control,!w);
        EnableWindow(native_apply_button,!w);
        EnableWindow(native_top_buttons[2],!w);
        EnableWindow(native_stop_button,w==1);return 0;
    case CHAT_DESTROY:DestroyWindow(window);PostQuitMessage(0);return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)w,TRANSPARENT);SetTextColor((HDC)w,RGB(43,64,76));return (LRESULT)native_background;
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT *)l;RECT r=d->rcItem;wchar_t label[40];
        if(d->CtlID==CHAT_PREVIEW){
            int i,cell=180;HGDIOBJ font=SelectObject(d->hDC,native_font);HBRUSH bg=CreateSolidBrush(RGB(245,247,249));FillRect(d->hDC,&r,bg);DeleteObject(bg);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,RGB(70,85,95));
            AcquireSRWLockShared(&native_image_lock);
            if(native_image_count&&cell*native_image_count>r.right-r.left)cell=(r.right-r.left)/native_image_count;
            for(i=0;i<native_image_count;i++){
                BITMAP bm;HDC memory=CreateCompatibleDC(d->hDC);HGDIOBJ old;RECT text={i*cell+72,5,(i+1)*cell-2,47};
                GetObject(native_images[i].thumbnail,sizeof(bm),&bm);old=SelectObject(memory,native_images[i].thumbnail);
                SetStretchBltMode(d->hDC,HALFTONE);StretchBlt(d->hDC,i*cell+6,5,60,40,memory,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);
                SelectObject(memory,old);DeleteDC(memory);DrawTextW(d->hDC,native_images[i].name,-1,&text,DT_WORDBREAK|DT_END_ELLIPSIS);
            }ReleaseSRWLockShared(&native_image_lock);SelectObject(d->hDC,font);return TRUE;
        }
        COLORREF color=d->CtlID==CHAT_SEND?RGB(18,120,104):RGB(225,234,239);
        int muted=(d->CtlID>=CHAT_RESERVED_2&&d->CtlID<=CHAT_RESERVED_3)||
            ((d->CtlID==CHAT_APPLY||d->CtlID==CHAT_WORKDIR)&&(d->itemState&ODS_DISABLED));
        HBRUSH brush;HPEN pen;HGDIOBJ oldbrush,oldpen,oldfont;
        if(d->itemState&ODS_SELECTED)color=d->CtlID==CHAT_SEND?RGB(13,96,83):RGB(208,221,229);
        if(muted)color=RGB(237,241,244);
        brush=CreateSolidBrush(color);pen=CreatePen(PS_SOLID,1,color);
        oldbrush=SelectObject(d->hDC,brush);oldpen=SelectObject(d->hDC,pen);
        RoundRect(d->hDC,r.left,r.top,r.right,r.bottom,10,10);
        oldfont=SelectObject(d->hDC,native_font);SetBkMode(d->hDC,TRANSPARENT);
        SetTextColor(d->hDC,d->CtlID==CHAT_SEND?RGB(255,255,255):muted?RGB(115,126,134):RGB(36,57,69));
        GetWindowTextW(d->hwndItem,label,40);DrawTextW(d->hDC,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(d->hDC,oldfont);SelectObject(d->hDC,oldbrush);SelectObject(d->hDC,oldpen);DeleteObject(brush);DeleteObject(pen);return TRUE;
    }
    case WM_SIZE:native_layout();return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO *)l)->ptMinTrackSize.x=640;((MINMAXINFO *)l)->ptMinTrackSize.y=620;return 0;
    case WM_DROPFILES:{
        UINT i,n=DragQueryFileW((HDROP)w,0xffffffff,NULL,0);
        wchar_t path[MAX_PATH],error[256],failure[256]=L"";
        if(!native_busy)for(i=0;i<n&&i<NATIVE_IMAGE_MAX;i++){
            if(!DragQueryFileW((HDROP)w,i,path,MAX_PATH))continue;
            if(!native_image_import(path,error,256)&&!failure[0])wcscpy_s(failure,256,error);
        }
        DragFinish((HDROP)w);native_attachment_changed();
        if(failure[0])native_attachment_error(failure);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==CHAT_FEATURES){native_feature_menu(native_top_buttons[0]);return 0;}
        if(LOWORD(w)==CHAT_COMPANY){native_company_action();return 0;}
        if(LOWORD(w)==CHAT_WORKDIR){if(!native_busy)native_workdir_menu(native_top_buttons[2]);return 0;}
        if(LOWORD(w)==CHAT_MODE&&HIWORD(w)==CBN_SELCHANGE){InterlockedExchange(&native_mode,(LONG)SendMessageW(native_mode_control,CB_GETCURSEL,0,0));return 0;}
        if(LOWORD(w)==CHAT_ATTACH){if(!native_busy)native_choose_images();return 0;}
        if(LOWORD(w)==CHAT_CLEAR_IMAGES){if(!native_busy){native_images_clear();native_attachment_changed();}return 0;}
        if(LOWORD(w)==CHAT_STOP){if(native_busy!=2)native_cancel_action();return 0;}
        if(LOWORD(w)==CHAT_DETAILS){native_details_action();return 0;}
        if(LOWORD(w)==CHAT_APPLY){if(!native_busy)native_apply_action();return 0;}
        if(LOWORD(w)==CHAT_SEND){if(native_busy)return 0;native_force_follow=1;native_send_action();return 0;}
        if(LOWORD(w)==CHAT_READ){native_force_follow=1;native_read_action();return 0;}
        if(LOWORD(w)==CHAT_CLOSE){native_close_action();return 0;}
        break;
    case WM_TIMER:if(w==CHAT_POLL){KillTimer(window,CHAT_POLL);native_poll_action();return 0;}break;
    case WM_CLOSE:native_close_action();return 0;
    case WM_DESTROY:
        free(native_render_source);native_render_source=NULL;
        KillTimer(window,CHAT_POLL);native_window=native_history=native_prompt=NULL;
        if(native_font){DeleteObject(native_font);native_font=NULL;}return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
