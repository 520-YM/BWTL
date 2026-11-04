#ifndef CREO_MODEL_PLAN_H
#define CREO_MODEL_PLAN_H
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <math.h>
typedef struct { int cylinder; double a, b, height, hole; int ready; int empty_kind; } ModelPlan;
/* Host routing guard: a current-model request must not silently become a
   new file merely because an AI returned the older CREO_PLAN protocol. */

#include "功能/model_plan/model_request_current.inc"


#include "功能/model_plan/model_empty_request.inc"


#include "功能/model_plan/model_plan_valid.inc"

/* Accept one exact whole line, never eval generated code or commands. */

#include "功能/model_plan/model_plan_parse.inc"


#include "功能/model_plan/model_plan_volume.inc"

/* Ordinary discussion must not consume the last executable proposal.
   A malformed replacement explicitly clears it to avoid stale execution. */

#include "功能/model_plan/model_plan_update.inc"


#include "功能/model_plan/model_plan_format.inc"

#endif
