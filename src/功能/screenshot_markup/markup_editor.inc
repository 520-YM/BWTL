#include <windowsx.h>
#include "markup_shared.inc"

static void native_markup_post(UINT message,LPARAM argument);
static HWND markup_window,markup_canvas,markup_input,markup_status,markup_send,markup_tools[3];
static MarkupImage *markup_display;
static MarkupMark markup_mark,markup_marks[MARKUP_MAX_MARKS];
static int markup_count,markup_retake,markup_thinking;
static MarkupTool markup_tool;
static int markup_dragging,markup_waiting;

enum {MARKUP_TOOL_CIRCLE=901, MARKUP_TOOL_ARROW, MARKUP_TOOL_BOX, MARKUP_SEND, MARKUP_CLOSE, MARKUP_UNDO, MARKUP_CLEAR, MARKUP_INPUT};
static const wchar_t markup_class[]=L"BWTLMarkupEditor";
static const wchar_t markup_canvas_class[]=L"BWTLMarkupCanvas";

static RECT markup_image_rect(HWND canvas){
    RECT client;
    GetClientRect(canvas,&client);
    if(!markup_display||client.right<1||client.bottom<1)return client;
    double scale=fmin((double)client.right/markup_display->width,(double)client.bottom/markup_display->height);
    LONG w=(LONG)(scale*markup_display->width),h=(LONG)(scale*markup_display->height);
    LONG x=(client.right-w)/2,y=(client.bottom-h)/2;
    return (RECT){x,y,x+w,y+h};
}

static void markup_mark_feedback(void){
    if(!markup_display||markup_waiting||markup_retake)return;
    wchar_t status[256];
    if(!markup_count){
        wcscpy_s(status,_countof(status),L"圈出位置后写下改图要求，也可以直接问截图里的问题。");
    }else{
        swprintf_s(status,_countof(status),L"已标注 %d / %d 处。",markup_count,MARKUP_MAX_MARKS);
        if(markup_count==1){
            const MarkupMark *mark=&markup_marks[0];
            const int hole=markup_match(markup_display,mark);
            const int opening=markup_match_opening(markup_display,mark);
            const double scale=markup_display->mm_per_native;
            const size_t used=wcslen(status);
            wchar_t input[256];
            GetWindowTextW(markup_input,input,_countof(input));
            double diameter;
            if(hole>=0&&scale>0&&(opening==-1||markup_named_hole(input))){
                const MarkupHole *target=&markup_display->holes[hole];
                if(target->dimension<0)
                    swprintf_s(status+used,_countof(status)-used,L"\r\n1 号圆孔 Ø%.8g mm。孔径没有独立驱动尺寸。",target->diameter*scale);
                else if(markup_parse_diameter(input,&diameter))
                    swprintf_s(status+used,_countof(status)-used,L"\r\n1 号孔径 Ø%.8g → Ø%.8g mm。",target->diameter*scale,diameter);
                else
                    swprintf_s(status+used,_countof(status)-used,L"\r\n1 号圆孔 Ø%.8g mm。",target->diameter*scale);
            }else if(opening>=0&&hole==-1&&scale>0){
                const MarkupOpening *target=&markup_display->openings[opening];
                swprintf_s(status+used,_countof(status)-used,L"\r\n1 号矩形开口 %.8g × %.8g mm。",
                    target->size[0]*scale,target->size[1]*scale);
            }else if(hole==-2||opening==-2||(hole>=0&&opening>=0)){
                wcscat_s(status,_countof(status),L"\r\n这里有多个候选。可缩小标注范围，或写明要改的是孔还是开口。");
            }
        }
    }
    SetWindowTextW(markup_status,status);
}

static void markup_draw_note(HDC dc,const RECT *image){
    wchar_t note[256];
    if(markup_count!=1||markup_dragging||image->right-image->left<100||
       image->bottom-image->top<48||!GetWindowTextW(markup_input,note,_countof(note)))return;
    const MarkupMark *mark=&markup_marks[0];
    const double px=mark->tool==MARKUP_ARROW?mark->x2:fmax(mark->x1,mark->x2);
    const double py=mark->tool==MARKUP_ARROW?mark->y2:fmin(mark->y1,mark->y2);
    const int anchor_x=image->left+(int)(px*(image->right-image->left)/markup_display->width);
    const int anchor_y=image->top+(int)(py*(image->bottom-image->top)/markup_display->height);
    HGDIOBJ font=SelectObject(dc,native_font);
    SIZE size={0};
    GetTextExtentPoint32W(dc,note,(int)wcslen(note),&size);
    const int width=min(size.cx+16,min(265,image->right-image->left-12));
    const int height=size.cy+8;
    const int left=max(image->left+6,min(anchor_x+9,image->right-width-6));
    const int top=max(image->top+4,min(anchor_y-height-6,image->bottom-height-4));
    RECT label={left,top,left+width,top+height};
    HBRUSH fill=CreateSolidBrush(RGB(17,51,69));
    if(fill){FillRect(dc,&label,fill);DeleteObject(fill);}
    InflateRect(&label,-8,0);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,RGB(255,255,255));
    DrawTextW(dc,note,-1,&label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    SelectObject(dc,font);
}

static void markup_pointer(HWND canvas,LPARAM position,double *x,double *y){
    RECT image=markup_image_rect(canvas);
    if(IsRectEmpty(&image))return;
    LONG px=GET_X_LPARAM(position),py=GET_Y_LPARAM(position);
    *x=fmin(markup_display->width,fmax(0,(double)(px-image.left)*markup_display->width/(image.right-image.left)));
    *y=fmin(markup_display->height,fmax(0,(double)(py-image.top)*markup_display->height/(image.bottom-image.top)));
}

static void markup_draw_marks(HDC dc,const RECT *bounds,int width,int height,const MarkupMark *marks,int count){
    RECT rect=*bounds;
    double sx=(double)(rect.right-rect.left)/width;
    double sy=(double)(rect.bottom-rect.top)/height;
    HPEN pen=CreatePen(PS_SOLID,3,RGB(231,66,53));
    HGDIOBJ old=SelectObject(dc,pen);
    HGDIOBJ brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    SetBkMode(dc,OPAQUE);
    SetBkColor(dc,RGB(231,66,53));
    SetTextColor(dc,RGB(255,255,255));
    for(int i=0;i<count;i++){
        const MarkupMark *mark=&marks[i];
        int ax=rect.left+(int)(mark->x1*sx),ay=rect.top+(int)(mark->y1*sy);
        int bx=rect.left+(int)(mark->x2*sx),by=rect.top+(int)(mark->y2*sy);
        if(mark->tool==MARKUP_ARROW){
            double angle=atan2(by-ay,bx-ax);
            MoveToEx(dc,ax,ay,NULL);
            LineTo(dc,bx,by);
            for(int direction=-1;direction<=1;direction+=2){
                double wing=angle+direction*0.48;
                MoveToEx(dc,bx,by,NULL);
                LineTo(dc,bx-(int)(14*cos(wing)),by-(int)(14*sin(wing)));
            }
        }else if(mark->tool==MARKUP_BOX)
            Rectangle(dc,min(ax,bx),min(ay,by),max(ax,bx),max(ay,by));
        else Ellipse(dc,min(ax,bx),min(ay,by),max(ax,bx),max(ay,by));
        wchar_t label=(wchar_t)(L'1'+i);
        TextOutW(dc,ax+4,ay-16,&label,1);
    }
    SelectObject(dc,brush);
    SelectObject(dc,old);
    DeleteObject(pen);
}

static LRESULT CALLBACK markup_canvas_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    switch(message){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT paint;
        HDC target=BeginPaint(window,&paint);
        RECT client,rect=markup_image_rect(window);
        GetClientRect(window,&client);
        HDC buffer=CreateCompatibleDC(target);
        HBITMAP frame=buffer?CreateCompatibleBitmap(target,client.right,client.bottom):NULL;
        HDC dc=frame?buffer:target;
        HGDIOBJ old_frame=frame?SelectObject(buffer,frame):NULL;
        FillRect(dc,&client,(HBRUSH)(COLOR_BTNFACE+1));
        if(markup_display){
            HDC source=CreateCompatibleDC(dc);
            if(source){
                HGDIOBJ old=SelectObject(source,markup_display->bitmap);
                SetStretchBltMode(dc,HALFTONE);
                SetBrushOrgEx(dc,0,0,NULL);
                StretchBlt(dc,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,
                    source,0,0,markup_display->width,markup_display->height,SRCCOPY);
                SelectObject(source,old);
                DeleteDC(source);
            }
            MarkupMark visible[MARKUP_MAX_MARKS];
            memcpy(visible,markup_marks,markup_count*sizeof(*visible));
            if(markup_dragging)visible[markup_count]=markup_mark;
            markup_draw_marks(dc,&rect,markup_display->width,markup_display->height,visible,markup_count+markup_dragging);
            markup_draw_note(dc,&rect);
        }
        if(frame){
            BitBlt(target,0,0,client.right,client.bottom,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old_frame);
            DeleteObject(frame);
        }
        if(buffer)DeleteDC(buffer);
        EndPaint(window,&paint);
        return 0;
    }
    case WM_LBUTTONDOWN:{
        if(!markup_display||markup_waiting||markup_retake)return 0;
        if(markup_count==MARKUP_MAX_MARKS){SetWindowTextW(markup_status,L"已到 8 处。可撤销一处或清空标注。");return 0;}
        RECT rect=markup_image_rect(window);
        POINT at={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        if(!PtInRect(&rect,at))return 0;
        markup_mark=(MarkupMark){.tool=markup_tool};
        markup_pointer(window,l,&markup_mark.x1,&markup_mark.y1);
        markup_mark.x2=markup_mark.x1;
        markup_mark.y2=markup_mark.y1;
        markup_dragging=1;
        SetCapture(window);
        return 0;
    }
    case WM_MOUSEMOVE:case WM_LBUTTONUP:
        if(markup_dragging){
            markup_pointer(window,l,&markup_mark.x2,&markup_mark.y2);
            if(message==WM_LBUTTONUP){
                markup_dragging=0;
                markup_mark.valid=hypot(markup_mark.x2-markup_mark.x1,markup_mark.y2-markup_mark.y1)>=4;
                if(markup_mark.valid)markup_marks[markup_count++]=markup_mark;
                ReleaseCapture();
                markup_mark_feedback();
            }
            InvalidateRect(window,NULL,FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if(markup_dragging){markup_dragging=0;InvalidateRect(window,NULL,FALSE);}
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}

static void markup_ui_send(void){
    if(markup_waiting||!markup_display||!IsWindowEnabled(markup_send))return;
    if(markup_retake){
        EnableWindow(markup_send,FALSE);
        native_markup_post(MARKUP_OPEN,0);
        SetWindowTextW(markup_status,L"正在重新截图…");
        return;
    }

    MarkupRequest *request=calloc(1,sizeof(*request));
    if(!request){SetWindowTextW(markup_status,L"无法提交标注，请重试。");return;}
    request->ticket=markup_display->ticket;
    request->count=markup_count;
    memcpy(request->marks,markup_marks,markup_count*sizeof(*markup_marks));
    if(!GetWindowTextW(markup_input,request->text,_countof(request->text))){
        SetWindowTextW(markup_status,L"说一下你想达到的效果，也可以请我帮你选尺寸。");
        SetFocus(markup_input);
        free(request);
        return;
    }
    markup_waiting=1;
    EnableWindow(markup_send,FALSE);
    SetWindowTextW(markup_status,L"正在处理你的要求…");
    native_markup_post(MARKUP_APPLY,(LPARAM)request);
}

static LRESULT CALLBACK markup_editor_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    switch(message){
    case WM_SIZE:{
        if(w==SIZE_MINIMIZED)return 0;
        int width=LOWORD(l),height=HIWORD(l);
        HWND controls[]={markup_tools[0],markup_tools[1],markup_tools[2],GetDlgItem(window,MARKUP_UNDO),
            GetDlgItem(window,MARKUP_CLEAR),markup_canvas,markup_input,markup_status,markup_send,GetDlgItem(window,MARKUP_CLOSE)};
        RECT layout[]={{12,10,68,26},{88,10,68,26},{164,10,68,26},{240,10,86,26},{334,10,86,26},
            {12,45,max(1,width-24),max(1,height-155)},
            {12,max(50,height-103),max(1,width-24),27},
            {12,max(80,height-69),max(1,width-255),60},
            {max(12,width-228),max(80,height-46),126,29},{max(12,width-92),max(80,height-46),80,29}};
        const UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW;
        HDWP positions=BeginDeferWindowPos(_countof(controls));
        for(int i=0;positions&&i<_countof(controls);i++)if(controls[i])
            positions=DeferWindowPos(positions,controls[i],NULL,layout[i].left,layout[i].top,layout[i].right,layout[i].bottom,flags);
        if(!positions||!EndDeferWindowPos(positions))for(int i=0;i<_countof(controls);i++)if(controls[i])
            SetWindowPos(controls[i],NULL,layout[i].left,layout[i].top,layout[i].right,layout[i].bottom,flags);
        RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
        return 0;
    }
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)l)->ptMinTrackSize=(POINT){500,340};
        return 0;
    case WM_COMMAND:{
        int id=LOWORD(w);
        if(id==MARKUP_INPUT&&HIWORD(w)==EN_CHANGE){
            markup_mark_feedback();
            if(markup_canvas)InvalidateRect(markup_canvas,NULL,FALSE);
            return 0;
        }
        if(id>=MARKUP_TOOL_CIRCLE&&id<=MARKUP_TOOL_BOX){
            markup_tool=(MarkupTool)(id-MARKUP_TOOL_CIRCLE);
            for(int i=0;i<3;i++)SendMessageW(markup_tools[i],BM_SETCHECK,i==(int)markup_tool?BST_CHECKED:BST_UNCHECKED,0);
            return 0;
        }
        if(id==MARKUP_UNDO||id==MARKUP_CLEAR){
            if(!markup_waiting&&!markup_retake){
                if(id==MARKUP_CLEAR)markup_count=0;
                else if(markup_count)markup_count--;
                InvalidateRect(markup_canvas,NULL,FALSE);
                markup_mark_feedback();
            }
            return 0;
        }
        if(id==MARKUP_SEND){markup_ui_send();return 0;}
        if(id==MARKUP_CLOSE){
            if(markup_thinking)native_markup_post(MARKUP_CANCEL,(LPARAM)markup_display->ticket);
            else if(!markup_waiting)DestroyWindow(window);
            return 0;
        }
        break;
    }
    case WM_CLOSE:SendMessageW(window,WM_COMMAND,MARKUP_CLOSE,0);return 0;
    case WM_DESTROY:
        markup_image_free(markup_display);
        markup_display=NULL;
        markup_window=markup_canvas=markup_input=markup_status=markup_send=NULL;
        memset(markup_tools,0,sizeof(markup_tools));
        markup_mark=(MarkupMark){0};
        markup_count=markup_dragging=markup_waiting=markup_retake=markup_thinking=0;
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}

static HWND markup_child(HWND owner,const wchar_t *type,const wchar_t *label,DWORD style,int id){
    HWND child=CreateWindowW(type,label,WS_CHILD|WS_VISIBLE|style,0,0,1,1,owner,
        (HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    if(child)SendMessageW(child,WM_SETFONT,(WPARAM)native_font,TRUE);
    return child;
}

static int markup_ui_open(HWND owner,MarkupImage *image){
    if(markup_waiting)return 0;
    if(markup_window)DestroyWindow(markup_window);
    HINSTANCE module=GetModuleHandleW(NULL);
    WNDCLASSW editor={.lpfnWndProc=markup_editor_proc,.hInstance=module,
        .hCursor=LoadCursor(NULL,IDC_ARROW),.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1),.lpszClassName=markup_class};
    WNDCLASSW canvas={.lpfnWndProc=markup_canvas_proc,.hInstance=module,
        .hCursor=LoadCursor(NULL,IDC_CROSS),.lpszClassName=markup_canvas_class};
    if(!RegisterClassW(&editor)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    if(!RegisterClassW(&canvas)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    wchar_t title[180];
    swprintf_s(title,_countof(title),L"截图改图 · %ls",image->name);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,markup_class,title,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,820,600,owner,NULL,module,NULL);
    if(!window)return 0;
    markup_window=window;
    const wchar_t *labels[]={L"圈选",L"箭头",L"框选"};
    for(int i=0;i<3;i++)markup_tools[i]=markup_child(window,L"BUTTON",labels[i],BS_RADIOBUTTON|WS_TABSTOP,MARKUP_TOOL_CIRCLE+i);
    HWND undo=markup_child(window,L"BUTTON",L"撤销一处",WS_TABSTOP,MARKUP_UNDO);
    HWND clear=markup_child(window,L"BUTTON",L"清空标注",WS_TABSTOP,MARKUP_CLEAR);
    markup_canvas=markup_child(window,markup_canvas_class,L"",0,0);
    markup_input=markup_child(window,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,MARKUP_INPUT);
    markup_status=markup_child(window,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL|WS_TABSTOP,0);
    markup_send=markup_child(window,L"BUTTON",L"发送并改图",WS_TABSTOP,MARKUP_SEND);
    HWND close=markup_child(window,L"BUTTON",L"关闭",WS_TABSTOP,MARKUP_CLOSE);
    if(!markup_tools[0]||!markup_tools[1]||!markup_tools[2]||!markup_canvas||!markup_input||
       !markup_status||!markup_send||!close||!undo||!clear){DestroyWindow(window);return 0;}
    markup_display=image;
    markup_tool=MARKUP_CIRCLE;
    SendMessageW(markup_tools[0],BM_SETCHECK,BST_CHECKED,0);
    SendMessageW(markup_input,EM_SETLIMITTEXT,255,0);
    SendMessageW(markup_input,EM_SETCUEBANNER,TRUE,(LPARAM)L"例如：这个孔改 Ø5，或这个开口的短边缩小 20 毫米");
    markup_mark_feedback();
    RECT rect;
    GetClientRect(window,&rect);
    SendMessageW(window,WM_SIZE,0,MAKELPARAM(rect.right,rect.bottom));
    ShowWindow(window,SW_SHOW);
    return 1;
}
