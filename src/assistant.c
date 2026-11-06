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
#include "machining_prompt.h"
static void append(const wchar_t *text);
static void append_card(const wchar_t *role,const wchar_t *text);
#include "smart_session.h"
#include "development_client.h"

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

