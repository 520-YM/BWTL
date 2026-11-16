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
static void native_workdir_action(UINT action);
static void native_imitation_action(UINT action);
static void native_feature_list_action(void);
static HWND native_window, native_history, native_prompt, native_title;
static HWND native_stop_button,native_preview;
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
#define CHAT_HISTORY_TEXT (WM_APP+31)
#define CHAT_DESTROY (WM_APP+32)
#define CHAT_BUSY (WM_APP+34)
#define BWTL_WORKDIR_REQUEST (WM_APP+49)
#define BWTL_IMITATION_REQUEST (WM_APP+50)
enum {WORKDIR_VIEW=1,WORKDIR_OPEN,WORKDIR_COPY,WORKDIR_PICK};
enum {IMITATION_CURRENT=1,IMITATION_FILE,IMITATION_STRICT,IMITATION_DIAGNOSE,
      IMITATION_COMPARE,IMITATION_BATCH,IMITATION_LIBRARY,IMITATION_BATCH_LIBRARY,IMITATION_BATCH_PREVIEW,
      IMITATION_COVERAGE,IMITATION_OPEN_STANDARD,IMITATION_LEARN,IMITATION_COMPARE_FLAT,IMITATION_NEUTRAL,IMITATION_MANUAL,IMITATION_ASSEMBLY_LEARN,IMITATION_MEMORY,
      IMITATION_CSYS_METHOD,IMITATION_SHEET_CUT_METHOD};
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
#include "功能/native_chat_ui/native_industrial_theme.inc"

static HMENU native_imitation_menu_create(void){
    HMENU menu=CreatePopupMenu();
    if(!menu)return NULL;
    AppendMenuW(menu,MF_STRING,IMITATION_CURRENT,L"按原步骤重建当前模型");
    AppendMenuW(menu,MF_STRING,IMITATION_FILE,L"选择模型文件并按原步骤重建…");
    AppendMenuW(menu,MF_STRING,IMITATION_STRICT,L"按原步骤重建并试改尺寸");
    AppendMenuW(menu,MF_STRING,IMITATION_MANUAL,L"按原步骤重建（参照不明确时手选）");
    AppendMenuW(menu,MF_STRING,IMITATION_DIAGNOSE,L"诊断当前模型与参照");
    AppendMenuW(menu,MF_STRING,IMITATION_BATCH_PREVIEW,L"批量预检模型与参照…");
    AppendMenuW(menu,MF_STRING,IMITATION_COMPARE,L"与另一零件比对…");
    AppendMenuW(menu,MF_STRING,IMITATION_COMPARE_FLAT,L"比对钣金展开…");
    AppendMenuW(menu,MF_STRING,IMITATION_NEUTRAL,L"从 STEP／IGES 推断并重建…");
    AppendMenuW(menu,MF_STRING,IMITATION_BATCH,L"批量按原步骤重建…");
    AppendMenuW(menu,MF_STRING,IMITATION_LEARN,L"保存最近的建模方法");
    AppendMenuW(menu,MF_STRING,IMITATION_ASSEMBLY_LEARN,L"装配学习");
    AppendMenuW(menu,MF_STRING,IMITATION_MEMORY,L"查看学习记忆");
    HMENU methods=CreatePopupMenu();
    if(methods){
        AppendMenuW(methods,MF_STRING,IMITATION_CSYS_METHOD,L"装配坐标系定位");
        AppendMenuW(methods,MF_STRING,IMITATION_SHEET_CUT_METHOD,L"钣金首壁与独立切口");
        if(!AppendMenuW(menu,MF_POPUP,(UINT_PTR)methods,L"已验证的做法"))DestroyMenu(methods);
    }
    AppendMenuW(menu,MF_SEPARATOR,0,NULL);
    AppendMenuW(menu,MF_STRING,IMITATION_LIBRARY,L"将上次结果存为公司标准件");
    AppendMenuW(menu,MF_STRING,IMITATION_BATCH_LIBRARY,L"将上次批量通过项存为公司标准件");
    AppendMenuW(menu,MF_STRING,IMITATION_OPEN_STANDARD,L"打开已验收标准件…");
    AppendMenuW(menu,MF_SEPARATOR,0,NULL);
    AppendMenuW(menu,MF_STRING,IMITATION_COVERAGE,L"已验收范围");
    return menu;
}

static void native_imitation_menu(HWND button){
    HMENU menu=native_imitation_menu_create();
    if(!menu)return;
    RECT at;
    if(GetWindowRect(button,&at)){
        UINT action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_LEFTALIGN,at.left,at.bottom,0,native_window,NULL);
        const wchar_t *draft=NULL;
        if(action==IMITATION_CSYS_METHOD)
            draft=L"用已学的坐标系定位法，把当前总装里的组件〔待填：组件型号或当前实例 ID〕"
                L"按案例〔待填：源装配名，或上次学习的案例〕定位。核对新组件和当前总装的两个坐标系后直接执行。";
        else if(action==IMITATION_SHEET_CUT_METHOD)
            draft=L"按已学的钣金首壁与独立切口建模方法，新建一块矩形钣金板。"
                L"长〔待填：毫米〕，宽〔待填：毫米〕，板厚〔待填：毫米〕，"
                L"圆形贯穿切口直径〔待填：毫米〕，圆心〔待填：草绘坐标 X、Y，或居中〕。直接建模。";
        if(draft){native_prompt_set(draft);SetFocus(native_prompt);}
        else if(action)native_imitation_action(action);
    }
    DestroyMenu(menu);
}

#include "功能/screenshot_markup/markup_editor.inc"
#include "功能/native_chat_ui/native_window_proc.inc"


#include "功能/native_chat_ui/native_ui_create.inc"

/* Keep text input on a Unicode message loop, separate from Creo's host pump.
   All Creo API work is posted back to its own UI thread by the callbacks. */

#include "功能/native_chat_ui/native_ui_thread_main.inc"


#include "功能/native_chat_ui/native_ui_open_threaded.inc"


#include "功能/native_chat_ui/native_ui_stop_thread.inc"

#endif
