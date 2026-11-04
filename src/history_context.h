#ifndef BWTL_HISTORY_CONTEXT_H
#define BWTL_HISTORY_CONTEXT_H
/* Local retrieval, not model training. History is evidence, never authority
   to repeat an old CAD command. Bound prompt size while keeping full files. */

#include "功能/history_context/history_score.inc"


#include "功能/history_context/history_context_write_mode.inc"

/* Preserve the existing interface; CAD requests already carry full tool specs. */

#include "功能/history_context/history_context_write.inc"

#endif
