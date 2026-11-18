#ifndef CREO_NATIVE_CHAT_UI_H
#define CREO_NATIVE_CHAT_UI_H
#include <windows.h>
#include <wchar.h>
#include <stdlib.h>
#include <richedit.h>
#include "chat_style.h"
#include "native_attachments.h"
#include <math.h>

/* These callbacks execute in Creo's UI thread; no input simulation. */
static void native_send_action(void);
static void native_read_action(void);
static void native_close_action(void);
static void native_poll_action(void);
static void native_cancel_action(void);
static void native_details_action(void);
static void native_company_action(void);
static void native_workdir_action(UINT action,wchar_t *project_name);
static void native_imitation_action(UINT action);
static void native_feature_list_action(void);
static HWND native_window, native_history, native_prompt, native_title;
static HWND native_stop_button,native_preview,native_elapsed_label,native_attach_button,native_clear_button;
static volatile LONG native_busy;
static HWND native_buttons[3];
static HWND native_top_buttons[5];
static HFONT native_font, native_title_font;
static HBRUSH native_background, native_header_brush;
static int native_dpi=96;
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
#define CHAT_DETAILS 4109
#define CHAT_PREVIEW 4111
#define CHAT_FEATURES 4112
#define CHAT_COMPANY 4113
#define CHAT_WORKDIR 4114
#define CHAT_IMITATION 4115
#define CHAT_RESERVED_3 4116
#define CHAT_TIME_TICK 4117
#define CHAT_TIME_BEGIN (WM_APP+35)
#define CHAT_TIME_END (WM_APP+36)
#define CHAT_ATTACHMENTS_SENT (WM_APP+37)
#define CHAT_HISTORY_TEXT (WM_APP+31)
#define CHAT_DESTROY (WM_APP+32)
#define CHAT_BUSY (WM_APP+34)
#define BWTL_WORKDIR_REQUEST (WM_APP+49)
#define BWTL_IMITATION_REQUEST (WM_APP+50)
enum {WORKDIR_VIEW=1,WORKDIR_OPEN,WORKDIR_COPY,WORKDIR_PICK,WORKDIR_CREATE};
enum {IMITATION_CURRENT=1,IMITATION_FILE,IMITATION_STRICT,IMITATION_DIAGNOSE,
      IMITATION_COMPARE,IMITATION_BATCH,IMITATION_LIBRARY,IMITATION_BATCH_LIBRARY,IMITATION_BATCH_PREVIEW,
      IMITATION_COVERAGE,IMITATION_OPEN_STANDARD,IMITATION_LEARN,IMITATION_COMPARE_FLAT,IMITATION_NEUTRAL,IMITATION_MANUAL,IMITATION_ASSEMBLY_LEARN,IMITATION_MEMORY,
      IMITATION_CSYS_METHOD,IMITATION_SHEET_CUT_METHOD,IMITATION_STUDY,IMITATION_GO_COMPANY};
static void native_set_busy(int busy){if(native_window)SendMessageW(native_window,CHAT_BUSY,busy,0);}
static void native_layout(void);
static void native_attachment_changed(void){if(native_preview)native_layout();}
static void native_attachment_error(const wchar_t *text){MessageBoxW(native_window,text,L"BWTL",MB_OK|MB_ICONWARNING);}
static void native_choose_attachments(void){
    OPENFILENAMEW dialog={0};
    wchar_t files[2048]=L"",path[MAX_PATH],error[256],first_error[256]=L"";
    dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=native_window;
    dialog.lpstrFilter=L"可分析文件\0*.pdf;*.stp;*.step;*.prt;*.prt.*;*.asm;*.asm.*;*.png;*.jpg;*.jpeg;*.bmp;*.webp;*.gif;*.tif;*.tiff;*.txt;*.csv;*.tsv;*.md;*.json;*.log\0所有文件\0*.*\0";
    dialog.lpstrFile=files;
    dialog.nMaxFile=_countof(files);
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER|OFN_ALLOWMULTISELECT;
    dialog.lpstrTitle=L"添加分析附件（PDF、Creo 模型、图片或文本）";
    if(!GetOpenFileNameW(&dialog))return;
    const wchar_t *leaf=files+wcslen(files)+1;
    if(!*leaf){
        if(!native_attachment_import(files,error,_countof(error)))wcscpy_s(first_error,_countof(first_error),error);
    }else for(;*leaf;leaf+=wcslen(leaf)+1){
        if(swprintf_s(path,_countof(path),L"%ls\\%ls",files,leaf)<0)
            wcscpy_s(error,_countof(error),L"文件路径太长。");
        else if(native_attachment_import(path,error,_countof(error)))continue;
        if(!first_error[0])wcscpy_s(first_error,_countof(first_error),error);
        if(native_attachment_count()>=NATIVE_ATTACHMENT_MAX)break;
    }
    native_attachment_changed();
    if(first_error[0])native_attachment_error(first_error);
}

#include "功能/native_chat_ui/native_request_timer.inc"
#include "功能/native_chat_ui/native_layout.inc"


#include "功能/native_chat_ui/native_ui_history.inc"


#include "功能/native_chat_ui/native_prompt_set.inc"


#include "功能/native_chat_ui/native_prompt_get.inc"


#include "功能/native_chat_ui/native_schedule_poll.inc"


#include "功能/native_chat_ui/native_stop_poll.inc"


#include "功能/native_chat_ui/native_prompt_proc.inc"

#include "功能/native_chat_ui/native_grouped_menu.inc"
#include "native_feature_catalog.h"
#include "功能/native_chat_ui/native_workdir_menu.inc"
#include "功能/native_chat_ui/native_company_menu.inc"
#include "功能/native_chat_ui/native_industrial_theme.inc"

#include "功能/native_chat_ui/native_imitation_menu.inc"

#include "功能/screenshot_markup/markup_editor.inc"
#include "功能/native_chat_ui/native_window_proc.inc"


#include "功能/native_chat_ui/native_ui_create.inc"

/* Keep text input on a Unicode message loop, separate from Creo's host pump.
   All Creo API work is posted back to its own UI thread by the callbacks. */

#include "功能/native_chat_ui/native_ui_thread_main.inc"


#include "功能/native_chat_ui/native_ui_open_threaded.inc"


#include "功能/native_chat_ui/native_ui_stop_thread.inc"

#endif
