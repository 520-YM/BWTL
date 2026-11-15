#define user_initialize production_industrial_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int save_chat_preview(HWND window,const char *path){
    RECT rect;
    if(!GetWindowRect(window,&rect))return 0;
    int width=rect.right-rect.left,height=rect.bottom-rect.top;
    BITMAPINFO bitmap={0};
    bitmap.bmiHeader.biSize=sizeof(bitmap.bmiHeader);
    bitmap.bmiHeader.biWidth=width;
    bitmap.bmiHeader.biHeight=-height;
    bitmap.bmiHeader.biPlanes=1;
    bitmap.bmiHeader.biBitCount=32;
    bitmap.bmiHeader.biCompression=BI_RGB;
    void *pixels=NULL;
    HDC dc=CreateCompatibleDC(NULL);
    if(!dc)return 0;
    HBITMAP image=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&pixels,NULL,0);
    if(!image){DeleteDC(dc);return 0;}
    HGDIOBJ previous=SelectObject(dc,image);
    int printed=PrintWindow(window,dc,2);
    BITMAPFILEHEADER header={0};
    DWORD bytes=(DWORD)width*(DWORD)height*4;
    header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(bitmap.bmiHeader);
    header.bfSize=header.bfOffBits+bytes;
    FILE *file=printed?fopen(path,"wb"):NULL;
    int saved=0;
    if(file){
        saved=fwrite(&header,sizeof(header),1,file)==1&&
            fwrite(&bitmap.bmiHeader,sizeof(bitmap.bmiHeader),1,file)==1&&
            fwrite(pixels,1,bytes,file)==bytes;
        if(fclose(file))saved=0;
    }
    SelectObject(dc,previous);
    DeleteObject(image);
    DeleteDC(dc);
    return saved;
}

static const UINT footer_ids[]={CHAT_READ,CHAT_SEND,CHAT_STOP,CHAT_CLOSE};
static volatile LONG footer_painted;

static LRESULT CALLBACK watch_footer_paint(int code,WPARAM w,LPARAM l){
    if(code>=0){
        const CWPSTRUCT *message=(const CWPSTRUCT *)l;
        if(message->message==WM_DRAWITEM&&message->hwnd==native_window){
            const DRAWITEMSTRUCT *item=(const DRAWITEMSTRUCT *)message->lParam;
            for(int i=0;i<_countof(footer_ids);i++)
                if(item->CtlID==footer_ids[i]&&(item->itemAction&ODA_DRAWENTIRE))
                    InterlockedOr(&footer_painted,1L<<i);
        }
    }
    return CallNextHookEx(NULL,code,w,l);
}

static int check_footer_resize(FILE *log){
    HWND window=native_window;
    MINMAXINFO limits={0};
    SendMessageW(window,WM_GETMINMAXINFO,0,(LPARAM)&limits);
    const int widths[]={900,800,640,710,640,980,640};
    const int heights[]={820,820,820,680,620,820,820};
    HHOOK hook=SetWindowsHookExW(WH_CALLWNDPROC,watch_footer_paint,NULL,native_ui_thread_id);
    if(!hook)return 0;
    int passed=1;
    for(int i=0;i<_countof(widths);i++){
        int width=MulDiv(widths[i],native_dpi,96),height=MulDiv(heights[i],native_dpi,96);
        if(width<limits.ptMinTrackSize.x)width=limits.ptMinTrackSize.x;
        if(height<limits.ptMinTrackSize.y)height=limits.ptMinTrackSize.y;
        InterlockedExchange(&footer_painted,0);
        int layout=SetWindowPos(window,NULL,0,0,width,height,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
        RECT client;
        GetClientRect(window,&client);
        int right=0;
        for(int j=0;j<_countof(footer_ids);j++){
            HWND button=GetDlgItem(window,footer_ids[j]);
            RECT rect={0};
            layout=layout&&GetWindowRect(button,&rect);
            MapWindowPoints(NULL,window,(POINT *)&rect,2);
            layout=layout&&IsWindowVisible(button)&&rect.left>right&&rect.right<=client.right&&
                rect.top==client.bottom-MulDiv(40,native_dpi,96)&&
                rect.bottom-rect.top==MulDiv(31,native_dpi,96);
            right=rect.right;
            UpdateWindow(button);
        }
        const LONG painted=InterlockedCompareExchange(&footer_painted,0,0);
        fprintf(log,"RESIZE=%dx%d LAYOUT=%d PAINT_MASK=%ld\n",width,height,layout,painted);
        passed=passed&&layout&&painted==((1L<<_countof(footer_ids))-1);
    }
    UnhookWindowsHookEx(hook);
    return passed;
}

static int inspect_industrial_chat(uiCmdCmdId command,uiCmdValue *value,void *data){
    FILE *log=fopen("industrial-chat-native.log","w");
    if(!log){ProEngineerEnd();return 0;}
    const wchar_t *sample=L"已保存验收通过的独立零件。\nE:\\project\\20261111\\m_222_bwtl.prt.1\n\n"
        L"【你】\n你现在仿照这个222，画一个600*200*300的模型。\n\n"
        L"【AI】\n本轮按你的分析或查看要求处理，模型保持原样。\n\n"
        L"【你】\n叫你画一个，就是叫你执行，而不是叫你分析。\n\n"
        L"【AI】\n本轮按你的分析或查看要求处理，模型保持原样。\n\n"
        L"【提示】\n——以上为历史记录。当前任务以本次原生回执为准。——\n\n"
        L"【你】\n现在我是不是可以和你聊天了？\n\n"
        L"【AI】\n可以，直接在这里和我聊天就行。可以聊设计思路、尺寸方案、建模问题，也可以直接让我执行建模；我会区分聊天和操作，不会把普通聊天当成建模指令。\n";
    int opened=native_ui_open_threaded(NULL);
    int text=opened&&native_ui_history(sample);
    const wchar_t *buttons[]={L"插件功能",L"公司标准",L"工作目录",L"仿照模型",L"预留 3",
        L"读取当前模型",L"发送给 AI",L"停止",L"关闭"};
    const UINT ids[]={CHAT_FEATURES,CHAT_COMPANY,CHAT_WORKDIR,CHAT_IMITATION,CHAT_RESERVED_3,
        CHAT_READ,CHAT_SEND,CHAT_STOP,CHAT_CLOSE};
    int controls=opened;
    for(int i=0;controls&&i<_countof(ids);i++){
        wchar_t label[40];
        controls=GetWindowTextW(GetDlgItem(native_window,ids[i]),label,_countof(label))&&
            !wcscmp(label,buttons[i]);
    }
    wchar_t actual[2000];
    int content=text&&GetWindowTextW(native_history,actual,_countof(actual))&&
        wcsstr(actual,L"600*200*300")&&wcsstr(actual,L"当前任务以本次原生回执为准")&&
        wcsstr(actual,L"不会把普通聊天当成建模指令");
    int preview=opened&&save_chat_preview(native_window,"industrial-chat.bmp");
    int resized=opened&&check_footer_resize(log);
    int narrow_preview=opened&&save_chat_preview(native_window,"industrial-chat-narrow.bmp");
    fprintf(log,"CONTROLS=%d\nCONTENT=%d\nPREVIEW=%d\nRESIZE_PASS=%d\nNARROW_PREVIEW=%d\nPASS=%d\n",
        controls,content,preview,resized,narrow_preview,controls&&content&&preview&&resized&&narrow_preview);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    SetEnvironmentVariableW(L"BWTL_INSTANCE_NUMBER",L"1");
    ProError error=production_industrial_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId command;
    if(!error)error=ProCmdActionAdd("BWTLIndustrialCheck",inspect_industrial_chat,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLIndustrialCheck`;");
}
