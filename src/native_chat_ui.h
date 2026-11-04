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

#include "功能/native_chat_ui/native_layout.inc"


#include "功能/native_chat_ui/native_ui_history.inc"


#include "功能/native_chat_ui/native_prompt_set.inc"


#include "功能/native_chat_ui/native_prompt_get.inc"


#include "功能/native_chat_ui/native_schedule_poll.inc"


#include "功能/native_chat_ui/native_stop_poll.inc"


#include "功能/native_chat_ui/native_prompt_proc.inc"


#include "功能/native_chat_ui/native_window_proc.inc"


#include "功能/native_chat_ui/native_ui_create.inc"

/* Keep text input on a Unicode message loop, separate from Creo's host pump.
   All Creo API work is posted back to its own UI thread by the callbacks. */

#include "功能/native_chat_ui/native_ui_thread_main.inc"


#include "功能/native_chat_ui/native_ui_open_threaded.inc"


#include "功能/native_chat_ui/native_ui_stop_thread.inc"

#endif
