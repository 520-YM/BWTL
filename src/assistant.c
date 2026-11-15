#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <windows.h>
#include "runtime_paths.h"
#include "chat_store.h"
#include "prompt_encoding.h"
#include "history_context.h"
#include "model_plan.h"
#include <ProToolkit.h>
#include <ProMdl.h>
#include <ProUtil.h>
#include <ProUICmd.h>
#include <ProUIDialog.h>
#include <ProUITextarea.h>
#include <ProUIInputpanel.h>
#include <ProUIPushbutton.h>
#include <ProMenu.h>
#include <ProRibbon.h>
#include "功能/runtime_paths/output_storage.inc"

static char dialog_name[] = "codex_native_assistant";
static int created = 0;
static wchar_t history[64000];
#include "native_chat_ui.h"
static HANDLE ai_process = NULL;
static ULONGLONG ai_started;
static ULONGLONG ai_requested;
static wchar_t conversation[24000];
static wchar_t memory[12000];
static int store_ready = 0;
static ModelPlan pending_plan = {0};
static ProMdlName expected_active_model = L"";
static void save_turn(const wchar_t *role, const wchar_t *text);
static wchar_t ai_dir[MAX_PATH], ai_output[MAX_PATH];
static void ai_poll(char *dialog, ProUITimerID timer, ProAppData data);

#include "功能/assistant/ai_cleanup.inc"


#include "功能/assistant/trace.inc"

#include "native_model.h"
#include "part_tools.h"
#include "part_selfcheck.h"
static PartSnapshot request_snapshot={0};
static PartCommand pending_part={0};
static int request_requires_current=0;
#include "part_prompt.h"
#include "foundation_prompt.h"
#include "solid_prompt.h"
#include "surface_prompt.h"
#include "advanced_machining_prompt.h"
#include "structural_prompt.h"
#include "sheetmetal_prompt.h"
#include "reuse_prompt.h"
#include "quality_prompt.h"
#include "delivery_prompt.h"
#include "assembly_prompt.h"
#include "panel_prompt.h"
#include "cabinet_prompt.h"
#include "fastener_prompt.h"
#include "shop_prompt.h"
#include "vent_prompt.h"
#include "revision_prompt.h"
#include "inspection_prompt.h"
#include "organize_prompt.h"
#include "fabrication_prompt.h"
#include "quick_parts_prompt.h"
#include "machining_prompt.h"
static void append(const wchar_t *text);
static void append_card(const wchar_t *role,const wchar_t *text);
#include "功能/assistant/smart_committed_part.inc"
#include "smart_checkpoint.h"
#include "smart_session.h"

#include "功能/assistant/execute_part_command.inc"


#include "功能/assistant/append_card.inc"


#include "功能/assistant/append.inc"


#include "功能/assistant/save_turn.inc"


#include "功能/assistant/save_pending_plan.inc"


#include "功能/assistant/show_pending_plan.inc"


#include "功能/native_model/model_create_centered_cylinder.inc"
#include "功能/assistant/execute_model_plan.inc"


#include "功能/assistant/verify_active_command.inc"


#include "功能/assistant/build_pending_command.inc"


#include "功能/assistant/memory_command.inc"


#include "功能/assistant/read_model.inc"


static void imitation_write_progress(FILE *prompt,ProMdl active);
static int method_explicit(const wchar_t *text);
static int method_prompt_write(FILE *prompt,const wchar_t *request,wchar_t kind);
#include "功能/assistant/ai_send.inc"


#include "功能/assistant/ai_poll.inc"


#include "功能/assistant/run_local.inc"


#include "功能/assistant/on_closed.inc"


#include "功能/assistant/close_window.inc"


#include "功能/assistant/access_command.inc"

static HWND native_backend_window;

#include "功能/assistant/send_native_command.inc"


#include "功能/assistant/inspect_native_command.inc"


#include "功能/assistant/native_workdir_action.inc"
#include "功能/model_imitation/imitation_run.inc"
#include "功能/model_imitation/imitation_resize.inc"
#include "功能/model_imitation/imitation_local.inc"
#include "功能/model_imitation/imitation_ui.inc"
#include "功能/screenshot_markup/markup_capture.inc"
#include "功能/screenshot_markup/markup_apply.inc"

static void native_markup_post(UINT message,LPARAM argument){
    if(!PostMessageW(native_backend_window,message,0,argument)&&message==MARKUP_APPLY){
        markup_deliver(((MarkupRequest *)argument)->ticket,0,0,L"无法提交到 Creo，请重试。");
        free((void *)argument);
    }
}

#include "功能/assistant/native_backend_proc.inc"


#include "功能/assistant/native_send_action.inc"


#include "功能/assistant/native_read_action.inc"


#include "功能/assistant/native_close_action.inc"


#include "功能/assistant/native_poll_action.inc"

#include "功能/assistant/native_company_action.inc"
#include "功能/assistant/native_feature_list_action.inc"

#include "功能/assistant/open_window.inc"


#include "功能/assistant/user_initialize.inc"


#include "功能/assistant/user_terminate.inc"

