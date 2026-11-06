#ifndef BWTL_RUNTIME_PATHS_H
#define BWTL_RUNTIME_PATHS_H
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <psapi.h>
static wchar_t runtime_root[MAX_PATH],runtime_data[MAX_PATH],runtime_settings[MAX_PATH];
static HANDLE runtime_data_mutex;

#include "功能/runtime_paths/runtime_init.inc"


#include "功能/runtime_paths/runtime_open.inc"

#include "功能/runtime_paths/runtime_isolation_verify.inc"
#include "功能/runtime_paths/runtime_isolation_begin.inc"
#include "功能/runtime_paths/runtime_isolation_end.inc"


#include "功能/runtime_paths/runtime_exe_exists.inc"


#include "功能/runtime_paths/runtime_codex.inc"

#endif
