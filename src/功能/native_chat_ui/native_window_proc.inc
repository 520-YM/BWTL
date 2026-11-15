static LRESULT CALLBACK native_window_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    switch(message){
    case CHAT_HISTORY_TEXT:return native_ui_history((const wchar_t *)l);
    case CHAT_BUSY:
        InterlockedExchange(&native_busy,(LONG)w);
        SetWindowTextW(native_buttons[1],w?L"处理中…":L"发送给 AI");
        EnableWindow(native_buttons[1],!w);
        EnableWindow(native_top_buttons[2],!w);
        SetWindowTextW(native_top_buttons[3],w==3?L"仿照中…":L"仿照模型");
        EnableWindow(native_top_buttons[3],!w);
        EnableWindow(native_stop_button,w!=0&&w!=3);return 0;
    case CHAT_DESTROY:DestroyWindow(window);PostQuitMessage(0);return 0;
    case WM_PAINT:{
        PAINTSTRUCT paint;
        HDC dc=BeginPaint(window,&paint);
        native_theme_paint(window,dc);
        EndPaint(window,&paint);
        return 0;
    }
    case WM_PRINTCLIENT:native_theme_paint(window,(HDC)w);return 0;
    case WM_CTLCOLORSTATIC:
        if((HWND)l==native_title){
            SetBkMode((HDC)w,TRANSPARENT);
            SetTextColor((HDC)w,CHAT_INPUT_COLOR);
            return (LRESULT)native_header_brush;
        }
        break;
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT *)l;RECT r=d->rcItem;
        if(d->CtlID==CHAT_PREVIEW){
            int i,cell=180;HGDIOBJ font=SelectObject(d->hDC,native_font);native_theme_fill(d->hDC,&r,CHAT_FOOTER_COLOR);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,CHAT_TEXT_COLOR);
            AcquireSRWLockShared(&native_image_lock);
            if(native_image_count&&cell*native_image_count>r.right-r.left)cell=(r.right-r.left)/native_image_count;
            for(i=0;i<native_image_count;i++){
                BITMAP bm;HDC memory=CreateCompatibleDC(d->hDC);HGDIOBJ old;RECT text={i*cell+72,5,(i+1)*cell-2,47};
                GetObject(native_images[i].thumbnail,sizeof(bm),&bm);old=SelectObject(memory,native_images[i].thumbnail);
                SetStretchBltMode(d->hDC,HALFTONE);StretchBlt(d->hDC,i*cell+6,5,60,40,memory,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);
                SelectObject(memory,old);DeleteDC(memory);DrawTextW(d->hDC,native_images[i].name,-1,&text,DT_WORDBREAK|DT_END_ELLIPSIS);
            }ReleaseSRWLockShared(&native_image_lock);SelectObject(d->hDC,font);return TRUE;
        }
        native_theme_button(d);
        return TRUE;
    }
    case WM_SIZE:native_layout();return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)l)->ptMinTrackSize.x=MulDiv(640,native_dpi,96);
        ((MINMAXINFO *)l)->ptMinTrackSize.y=MulDiv(620,native_dpi,96);
        return 0;
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
    case MARKUP_READY:
        if(!markup_ui_open(window,(MarkupImage *)l))markup_image_free((MarkupImage *)l);
        return 0;
    case MARKUP_PROGRESS:
        if(markup_window&&markup_display&&markup_display->ticket==(unsigned long)l){
            markup_thinking=1;
            SetWindowTextW(markup_status,L"Codex 正在结合截图、标注和当前模型分析…");
            SetWindowTextW(GetDlgItem(markup_window,MARKUP_CLOSE),L"停止分析");
        }
        return 0;
    case MARKUP_RESULT:{
        MarkupResult *result=(MarkupResult *)l;
        if(markup_window&&markup_display&&(!result->ticket||markup_display->ticket==result->ticket)){
            markup_waiting=markup_thinking=0;
            SetWindowTextW(GetDlgItem(markup_window,MARKUP_CLOSE),L"关闭");
            if(result->success||result->retake){
                markup_retake=1;
                SetWindowTextW(markup_send,L"重新截图");
            }
            EnableWindow(markup_send,TRUE);
            SetWindowTextW(markup_status,result->text);
        }
        free(result);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==CHAT_RESERVED_3){if(!native_busy&&!markup_waiting)native_markup_post(MARKUP_OPEN,0);return 0;}
        if(LOWORD(w)==CHAT_FEATURES){native_feature_menu(native_top_buttons[0]);return 0;}
        if(LOWORD(w)==CHAT_COMPANY){native_company_action();return 0;}
        if(LOWORD(w)==CHAT_WORKDIR){if(!native_busy)native_workdir_menu(native_top_buttons[2]);return 0;}
        if(LOWORD(w)==CHAT_IMITATION){if(!native_busy)native_imitation_menu(native_top_buttons[3]);return 0;}
        if(LOWORD(w)==CHAT_ATTACH){if(!native_busy)native_choose_images();return 0;}
        if(LOWORD(w)==CHAT_CLEAR_IMAGES){if(!native_busy){native_images_clear();native_attachment_changed();}return 0;}
        if(LOWORD(w)==CHAT_STOP){if(native_busy)native_cancel_action();return 0;}
        if(LOWORD(w)==CHAT_DETAILS){native_details_action();return 0;}
        if(LOWORD(w)==CHAT_SEND){if(native_busy)return 0;native_force_follow=1;native_send_action();return 0;}
        if(LOWORD(w)==CHAT_READ){if(native_busy!=3){native_force_follow=1;native_read_action();}return 0;}
        if(LOWORD(w)==CHAT_CLOSE){native_close_action();return 0;}
        break;
    case WM_TIMER:if(w==CHAT_POLL){KillTimer(window,CHAT_POLL);native_poll_action();return 0;}break;
    case WM_CLOSE:native_close_action();return 0;
    case WM_DESTROY:
        if(markup_window)DestroyWindow(markup_window); free(native_render_source);native_render_source=NULL;
        KillTimer(window,CHAT_POLL);native_window=native_history=native_prompt=NULL;
        if(native_font){DeleteObject(native_font);native_font=NULL;}return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
