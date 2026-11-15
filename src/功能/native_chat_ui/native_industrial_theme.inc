#include <dwmapi.h>

static void native_theme_caption(HWND window){
    typedef HRESULT (WINAPI *SetWindowAttribute)(HWND,DWORD,LPCVOID,DWORD);
    HMODULE module=LoadLibraryExW(L"dwmapi.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module)return;
    SetWindowAttribute set=(SetWindowAttribute)GetProcAddress(module,"DwmSetWindowAttribute");
    if(set){
        const COLORREF background=CHAT_HEADER_COLOR,text=CHAT_INPUT_COLOR;
        set(window,DWMWA_CAPTION_COLOR,&background,sizeof(background));
        set(window,DWMWA_TEXT_COLOR,&text,sizeof(text));
    }
    FreeLibrary(module);
}

static void native_theme_fill(HDC dc,const RECT *rect,COLORREF color){
    HBRUSH brush=CreateSolidBrush(color);
    if(brush){FillRect(dc,rect,brush);DeleteObject(brush);}
}

static void native_theme_round(HDC dc,const RECT *rect,COLORREF fill,COLORREF border,int radius){
    HBRUSH brush=CreateSolidBrush(fill);
    HPEN pen=CreatePen(PS_SOLID,1,border);
    if(brush&&pen){
        HGDIOBJ old_brush=SelectObject(dc,brush);
        HGDIOBJ old_pen=SelectObject(dc,pen);
        RoundRect(dc,rect->left,rect->top,rect->right,rect->bottom,radius,radius);
        SelectObject(dc,old_pen);
        SelectObject(dc,old_brush);
    }
    if(pen)DeleteObject(pen);
    if(brush)DeleteObject(brush);
}

static void native_theme_wave(HDC dc,int width,int height,int offset,COLORREF color){
    const int left=width/2;
    POINT curve[]={{width*3/4,height-offset},{width*4/5,offset},{width,offset}};
    HBRUSH brush=CreateSolidBrush(color);
    if(!brush)return;
    HGDIOBJ old_brush=SelectObject(dc,brush);
    BeginPath(dc);
    MoveToEx(dc,left,height,NULL);
    PolyBezierTo(dc,curve,_countof(curve));
    LineTo(dc,width,height);
    CloseFigure(dc);
    EndPath(dc);
    FillPath(dc);
    SelectObject(dc,old_brush);
    DeleteObject(brush);
}

static void native_theme_edit_frame(HDC dc,HWND parent,HWND edit,COLORREF fill){
    RECT rect;
    if(!GetWindowRect(edit,&rect))return;
    MapWindowPoints(HWND_DESKTOP,parent,(POINT *)&rect,2);
    InflateRect(&rect,3,3);
    native_theme_round(dc,&rect,fill,CHAT_FRAME_COLOR,7);
}

static void native_theme_paint(HWND window,HDC dc){
    RECT rect;
    GetClientRect(window,&rect);
    native_theme_fill(dc,&rect,CHAT_CANVAS_COLOR);
    int header=MulDiv(84,native_dpi,96);
    int footer=rect.bottom-MulDiv(49,native_dpi,96);
    RECT top={0,0,rect.right,header};
    RECT rule={0,header-2,rect.right,header};
    RECT bottom={0,footer,rect.right,rect.bottom};
    RECT divider={0,footer,rect.right,footer+1};
    native_theme_fill(dc,&top,CHAT_HEADER_COLOR);
    native_theme_wave(dc,rect.right,header,MulDiv(10,native_dpi,96),RGB(16,82,102));
    native_theme_wave(dc,rect.right,header,MulDiv(35,native_dpi,96),RGB(17,71,91));
    native_theme_fill(dc,&rule,RGB(34,147,140));
    native_theme_fill(dc,&bottom,CHAT_FOOTER_COLOR);
    native_theme_fill(dc,&divider,RGB(216,225,230));
    native_theme_edit_frame(dc,window,native_history,CHAT_CANVAS_COLOR);
    native_theme_edit_frame(dc,window,native_prompt,CHAT_INPUT_COLOR);
}

static void native_theme_button(const DRAWITEMSTRUCT *item){
    const int top=item->CtlID>=CHAT_FEATURES&&item->CtlID<=CHAT_RESERVED_3;
    const int disabled=(item->itemState&ODS_DISABLED)!=0;
    const int pressed=(item->itemState&ODS_SELECTED)!=0;
    const int send=item->CtlID==CHAT_SEND;
    COLORREF fill,outline,text;
    if(disabled){
        fill=top?RGB(188,204,214):RGB(227,234,238);
        outline=top?RGB(176,195,205):RGB(205,217,224);
        text=RGB(79,103,117);
    }else if(send){
        fill=pressed?RGB(12,85,78):CHAT_ACTION_COLOR;
        outline=fill;
        text=CHAT_INPUT_COLOR;
    }else{
        fill=pressed?RGB(191,214,226):top?RGB(218,232,240):RGB(225,234,239);
        outline=top?RGB(167,190,203):RGB(178,195,205);
        text=CHAT_TEXT_COLOR;
    }
    RECT rect=item->rcItem;
    int saved=SaveDC(item->hDC);
    native_theme_fill(item->hDC,&rect,top?CHAT_HEADER_COLOR:CHAT_FOOTER_COLOR);
    native_theme_round(item->hDC,&rect,fill,outline,MulDiv(8,native_dpi,96));
    SelectObject(item->hDC,native_font);
    SetBkMode(item->hDC,TRANSPARENT);
    SetTextColor(item->hDC,text);
    wchar_t label[40];
    GetWindowTextW(item->hwndItem,label,_countof(label));
    DrawTextW(item->hDC,label,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if((item->itemState&(ODS_FOCUS|ODS_NOFOCUSRECT))==ODS_FOCUS){
        InflateRect(&rect,-3,-3);
        DrawFocusRect(item->hDC,&rect);
    }
    if(saved)RestoreDC(item->hDC,saved);
}
