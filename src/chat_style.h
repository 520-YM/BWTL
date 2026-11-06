#ifndef CREO_CHAT_STYLE_H
#define CREO_CHAT_STYLE_H
#include <stdio.h>
#include <string.h>
/* Native RichEdit RTF: shaded table cells fill the message width. Nothing
   is rendered in a browser; message text is escaped, never treated as RTF. */
typedef struct { char *text;size_t capacity,length;int failed; } ChatRtf;
typedef struct { const char *text;size_t length,offset; } ChatStream;

#include "功能/chat_style/chat_rtf_add.inc"


#include "功能/chat_style/chat_rtf_text.inc"
#include "功能/chat_style/chat_rtf_markup.inc"

/* Also recognize persisted, plain-text histories from earlier versions. */

#include "功能/chat_style/chat_line_role.inc"


#include "功能/chat_style/chat_rtf_build.inc"


#include "功能/chat_style/chat_stream_read.inc"


#include "功能/chat_style/chat_render.inc"

#endif
