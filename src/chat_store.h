/* Local UTF-8 persistence. No network, desktop or Creo dependencies. */
#ifndef CREO_CHAT_STORE_H
#define CREO_CHAT_STORE_H
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <windows.h>

static wchar_t store_root[MAX_PATH];

#include "功能/chat_store/store_path.inc"


#include "功能/chat_store/store_init.inc"

/* Keep the newest portion in memory; full history is separately append-only. */

#include "功能/chat_store/store_tail_add.inc"


#include "功能/chat_store/store_read.inc"


#include "功能/chat_store/store_write.inc"


#include "功能/chat_store/store_log.inc"

/* Each memory is a complete line. Remove only an exact entry. */

#include "功能/chat_store/store_forget.inc"

#endif
