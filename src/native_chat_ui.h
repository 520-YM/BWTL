#ifndef CREO_NATIVE_CHAT_UI_H
#define CREO_NATIVE_CHAT_UI_H
#include <windows.h>
#include <wchar.h>
#include <stdlib.h>
#include <richedit.h>
#include "chat_style.h"
#include "native_attachments.h"
/* These callbacks execute in Creo's UI thread; no input simulation. */
static void native_send_action(void);
static void native_read_action(void);
static void native_close_action(void);
static void native_poll_action(void);
static void native_cancel_action(void);
static void native_apply_action(void);
static void native_details_action(void);
static void native_company_action(void);
static void native_workdir_action(UINT action);
static void native_feature_list_action(void);
static HWND native_window, native_history, native_prompt, native_title;
static HWND native_mode_control,native_stop_button,native_apply_button,native_preview;
static volatile LONG native_mode,native_busy;
static HWND native_buttons[3];
static HWND native_top_buttons[5];
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
#define CHAT_ATTACH 4105
#define CHAT_CLEAR_IMAGES 4106
#define CHAT_STOP 4107
#define CHAT_MODE 4108
#define CHAT_DETAILS 4109
#define CHAT_APPLY 4110
#define CHAT_PREVIEW 4111
#define CHAT_FEATURES 4112
#define CHAT_COMPANY 4113
#define CHAT_WORKDIR 4114
#define CHAT_RESERVED_2 4115
#define CHAT_RESERVED_3 4116
#define CHAT_HISTORY_TEXT (WM_APP+31)
#define CHAT_DESTROY (WM_APP+32)
#define CHAT_BUSY (WM_APP+34)
#define BWTL_WORKDIR_REQUEST (WM_APP+49)
enum {WORKDIR_VIEW=1,WORKDIR_OPEN,WORKDIR_COPY,WORKDIR_PICK};
static void native_set_busy(int busy){if(native_window)SendMessageW(native_window,CHAT_BUSY,busy,0);}
static void native_layout(void);
static void native_attachment_changed(void){if(native_preview)native_layout();}
static void native_attachment_error(const wchar_t *text){MessageBoxW(native_window,text,L"BWTL",MB_OK|MB_ICONWARNING);}
static void native_choose_images(void){
    OPENFILENAMEW ofn={0};wchar_t path[MAX_PATH]=L"",error[256];
    ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=native_window;ofn.lpstrFilter=L"图片 (PNG/JPEG/BMP/WebP)\0*.png;*.jpg;*.jpeg;*.bmp;*.webp\0所有文件\0*.*\0";
    ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;ofn.lpstrTitle=L"选择要发给BWTL分析的图片";
    if(GetOpenFileNameW(&ofn)){
        if(native_image_import(path,error,256))native_attachment_changed();
        else native_attachment_error(error);
    }
}

#include "功能/native_chat_ui/native_layout.inc"


#include "功能/native_chat_ui/native_ui_history.inc"


#include "功能/native_chat_ui/native_prompt_set.inc"


#include "功能/native_chat_ui/native_prompt_get.inc"


#include "功能/native_chat_ui/native_schedule_poll.inc"


#include "功能/native_chat_ui/native_stop_poll.inc"


#include "功能/native_chat_ui/native_prompt_proc.inc"

#include "native_feature_catalog.h"
#include "功能/native_chat_ui/native_workdir_menu.inc"

#include "功能/native_chat_ui/native_window_proc.inc"


#include "功能/native_chat_ui/native_ui_create.inc"

/* Keep text input on a Unicode message loop, separate from Creo's host pump.
   All Creo API work is posted back to its own UI thread by the callbacks. */

#include "功能/native_chat_ui/native_ui_thread_main.inc"


#include "功能/native_chat_ui/native_ui_open_threaded.inc"


#include "功能/native_chat_ui/native_ui_stop_thread.inc"

#endif
