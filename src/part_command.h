#ifndef CREO_PART_COMMAND_H
#define CREO_PART_COMMAND_H
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

/* Independent implementation. No generated code, shell, paths or macros are
   accepted. A response can contain exactly ONE executable protocol line. */
#define PART_ARG_MAX 32
#define PART_TOKEN_MAX 256
typedef enum { PART_NONE, PART_READ, PART_DIMS, PART_PARAM, PART_COPY,
    PART_RENAME, PART_REGEN, PART_SAVE, PART_STEP, PART_SUPPRESS, PART_RESUME, PART_DISPLAY, PART_EXTRUDE, PART_FOUNDATION } PartVerb;
typedef struct { PartVerb verb; int count; wchar_t arg[PART_ARG_MAX][PART_TOKEN_MAX]; } PartCommand;

#include "功能/part_command/part_number.inc"


#include "功能/part_command/part_integer.inc"


#include "功能/part_command/part_name.inc"

#include "foundation_plan.h"

#include "功能/part_command/part_tokens.inc"

/* -1: malformed/multiple actions, 0: no action, 1: valid native action.
   Legacy CREO_PLAN is counted too, so mixed protocols cannot both execute. */

#include "功能/part_command/part_command_parse.inc"

#endif
