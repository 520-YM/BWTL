#ifndef CREO_NATIVE_CHAT_UI_H
#define CREO_NATIVE_CHAT_UI_H
#include <windows.h>
#include <wchar.h>
#include <stdlib.h>
#include <richedit.h>
#include "chat_style.h"
/* These callbacks execute in Creo's UI thread; no input simulation. */
static void native_send_action(void);
static void native_read_action(void);
static void native_close_action(void);
static void native_poll_action(void);
static HWND native_window, native_history, native_prompt, native_heading;
static HWND native_title, native_input_label;
static HWND native_buttons[3];
static HFONT native_font, native_title_font;
static HBRUSH native_background;
static HMODULE native_rich_module;
static HANDLE native_ui_thread, native_ui_ready;
static DWORD native_ui_thread_id;
static WNDPROC native_prompt_original;
static int native_composing, native_force_follow = 1;
static wchar_t *native_render_source;
static int native_ui_history(const wchar_t *text);
#define CHAT_SEND 4101
#define CHAT_READ 4102
#define CHAT_CLOSE 4103
#define CHAT_POLL 4104
#define CHAT_HISTORY_TEXT (WM_APP+31)
#define CHAT_DESTROY (WM_APP+32)
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
static int native_ui_history(const wchar_t *text) {
    SCROLLINFO info={sizeof(info),SIF_ALL};
    int first,follow; LRESULT length;
    size_t count;
    if(!IsWindow(native_history))return 0;
    if(native_ui_thread_id && GetCurrentThreadId()!=native_ui_thread_id)
        return (int)SendMessageW(native_window,CHAT_HISTORY_TEXT,0,(LPARAM)text);
    first=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);
    GetScrollInfo(native_history,SB_VERT,&info);
    follow=native_force_follow || info.nMax<=0 || info.nPos >= info.nMax-(int)info.nPage-1;
    if(text!=native_render_source){
        wchar_t *copy;count=wcslen(text);copy=(wchar_t *)calloc(count+1,sizeof(wchar_t));if(!copy)return 0;
        wcscpy_s(copy,count+1,text);free(native_render_source);native_render_source=copy;
    }
    if(!chat_render(native_history,native_render_source))return 0;
    if(follow){
        length=GetWindowTextLengthW(native_history);
        SendMessageW(native_history,EM_SETSEL,length,length);
        SendMessageW(native_history,EM_SCROLLCARET,0,0);
        SendMessageW(native_history,WM_VSCROLL,SB_BOTTOM,0);
    }else{
        int now=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);
        SendMessageW(native_history,EM_LINESCROLL,0,first-now);
    }
    native_force_follow=0;
    return 1;
}
static void native_prompt_set(const wchar_t *text){if(native_prompt)SetWindowTextW(native_prompt,text);}
static int native_prompt_get(wchar_t **value){
    int length;
    *value=NULL;if(!IsWindow(native_prompt))return 0;
    length=GetWindowTextLengthW(native_prompt);
    *value=(wchar_t *)calloc((size_t)length+1,sizeof(wchar_t));
    if(!*value)return 0;
    GetWindowTextW(native_prompt,*value,length+1);return 1;
}
static int native_schedule_poll(void){return native_window && SetTimer(native_window,CHAT_POLL,500,NULL)!=0;}
static void native_stop_poll(void){if(native_window)KillTimer(native_window,CHAT_POLL);}
static LRESULT CALLBACK native_prompt_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_IME_STARTCOMPOSITION)native_composing=1;
    if(message==WM_IME_ENDCOMPOSITION)native_composing=0;
    if(message==WM_KEYDOWN && w==VK_RETURN && !native_composing && !(GetKeyState(VK_SHIFT)&0x8000)){
        SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);return 0;
    }
    if(message==WM_CHAR && w==L'\r' && !native_composing && !(GetKeyState(VK_SHIFT)&0x8000))return 0;
    return CallWindowProcW(native_prompt_original,window,message,w,l);
}
static LRESULT CALLBACK native_window_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    switch(message){
    case CHAT_HISTORY_TEXT:return native_ui_history((const wchar_t *)l);
    case CHAT_DESTROY:DestroyWindow(window);PostQuitMessage(0);return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)w,TRANSPARENT);SetTextColor((HDC)w,RGB(43,64,76));return (LRESULT)native_background;
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT *)l;RECT r=d->rcItem;wchar_t label[40];
        COLORREF color=d->CtlID==CHAT_SEND?RGB(18,120,104):RGB(225,234,239);
        HBRUSH brush;HPEN pen;HGDIOBJ oldbrush,oldpen,oldfont;
        if(d->itemState&ODS_SELECTED)color=d->CtlID==CHAT_SEND?RGB(13,96,83):RGB(208,221,229);
        brush=CreateSolidBrush(color);pen=CreatePen(PS_SOLID,1,color);
        oldbrush=SelectObject(d->hDC,brush);oldpen=SelectObject(d->hDC,pen);
        RoundRect(d->hDC,r.left,r.top,r.right,r.bottom,10,10);
        oldfont=SelectObject(d->hDC,native_font);SetBkMode(d->hDC,TRANSPARENT);
        SetTextColor(d->hDC,d->CtlID==CHAT_SEND?RGB(255,255,255):RGB(36,57,69));
        GetWindowTextW(d->hwndItem,label,40);DrawTextW(d->hDC,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(d->hDC,oldfont);SelectObject(d->hDC,oldbrush);SelectObject(d->hDC,oldpen);DeleteObject(brush);DeleteObject(pen);return TRUE;
    }
    case WM_SIZE:native_layout();return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO *)l)->ptMinTrackSize.x=460;((MINMAXINFO *)l)->ptMinTrackSize.y=400;return 0;
    case WM_COMMAND:
        if(LOWORD(w)==CHAT_SEND){native_force_follow=1;native_send_action();return 0;}
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
static int native_ui_create(HWND owner,int show){
    WNDCLASSW cls={0};HINSTANCE module=GetModuleHandleW(NULL);int i;HDC dc;int dpi;
    const wchar_t *labels[3]={L"读取当前模型",L"发送给 AI",L"关闭"};
    int ids[3]={CHAT_READ,CHAT_SEND,CHAT_CLOSE};
    if(native_window){if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);return 1;}
    cls.lpfnWndProc=native_window_proc;cls.hInstance=module;cls.lpszClassName=L"CreoCodexNativeChatV11";
    cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    cls.lpszClassName=L"CreoCodexNativeChatV14";
    native_background=CreateSolidBrush(RGB(244,247,249));cls.hbrBackground=native_background;
    if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    native_window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"霸威天龙 v18",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,880,660,owner,NULL,module,NULL);
    if(!native_window)return 0;
    native_title=CreateWindowW(L"STATIC",L"霸威天龙 v18",WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_heading=CreateWindowW(L"STATIC",L"原生助手  ·  对话、记忆与建模",WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_input_label=CreateWindowW(L"STATIC",L"消息    Enter 发送  /  Shift+Enter 换行",WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_rich_module=LoadLibraryW(L"Msftedit.dll");
    if(!native_rich_module){DestroyWindow(native_window);return 0;}
    native_history=CreateWindowExW(WS_EX_CLIENTEDGE,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    native_prompt=CreateWindowExW(WS_EX_CLIENTEDGE,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    for(i=0;i<3;i++)native_buttons[i]=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        0,0,0,0,native_window,(HMENU)(INT_PTR)ids[i],module,NULL);
    if(!native_history||!native_prompt||!native_heading||!native_buttons[0]||!native_buttons[1]||!native_buttons[2]){DestroyWindow(native_window);return 0;}
    SendMessageW(native_history,EM_EXLIMITTEXT,0,150000);
    dc=GetDC(native_window);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(native_window,dc);
    native_font=CreateFontW(-MulDiv(10,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    native_title_font=CreateFontW(-MulDiv(17,dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    SendMessageW(native_title,WM_SETFONT,(WPARAM)native_title_font,TRUE);
    SendMessageW(native_input_label,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_history,EM_SETBKGNDCOLOR,0,RGB(248,250,252));
    SendMessageW(native_prompt,EM_SETBKGNDCOLOR,0,RGB(255,255,255));
    SendMessageW(native_heading,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_history,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_prompt,WM_SETFONT,(WPARAM)native_font,TRUE);
    for(i=0;i<3;i++)SendMessageW(native_buttons[i],WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_history,EM_SETLIMITTEXT,12000,0);SendMessageW(native_prompt,EM_SETLIMITTEXT,6000,0);
    native_prompt_original=(WNDPROC)SetWindowLongPtrW(native_prompt,GWLP_WNDPROC,(LONG_PTR)native_prompt_proc);
    native_layout();if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);
    return 1;
}
/* Keep text input on a Unicode message loop, separate from Creo's host pump.
   All Creo API work is posted back to its own UI thread by the callbacks. */
static DWORD WINAPI native_ui_thread_main(void *unused){
    MSG message;
    native_ui_thread_id=GetCurrentThreadId();
    native_ui_create(NULL,0);SetEvent(native_ui_ready);
    if(native_window)while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    UnregisterClassW(L"CreoCodexNativeChatV14",GetModuleHandleW(NULL));
    if(native_title_font){DeleteObject(native_title_font);native_title_font=NULL;}
    if(native_background){DeleteObject(native_background);native_background=NULL;}
    if(native_rich_module){FreeLibrary(native_rich_module);native_rich_module=NULL;}
    return 0;
}
static int native_ui_open_threaded(HWND owner){
    if(!native_ui_thread){
        native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);if(!native_ui_ready)return 0;
        native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);if(!native_ui_thread)return 0;
        if(WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0)return 0;
        CloseHandle(native_ui_ready);native_ui_ready=NULL;
    }
    if(!IsWindow(native_window))return 0;
    if(owner)SetWindowLongPtrW(native_window,GWLP_HWNDPARENT,(LONG_PTR)owner);
    ShowWindowAsync(native_window,SW_SHOWNOACTIVATE);return 1;
}
static void native_ui_stop_thread(void){
    if(native_window)SendMessageW(native_window,CHAT_DESTROY,0,0);
    if(native_ui_thread){WaitForSingleObject(native_ui_thread,5000);CloseHandle(native_ui_thread);native_ui_thread=NULL;}
    native_ui_thread_id=0;
}
#endif
