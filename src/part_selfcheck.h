#ifndef CREO_PART_SELFCHECK_H
#define CREO_PART_SELFCHECK_H
/* This self-check runs inside the already loaded, trusted native assistant,
   in its Creo command callback. It touches only a fresh private test part,
   never displays a window and never saves the test part. */
static int part_selfcheck_passed=0;

#include "功能/part_selfcheck/part_selfcheck.inc"

#endif
