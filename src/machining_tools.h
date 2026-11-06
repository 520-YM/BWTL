#ifndef BWTL_MACHINING_TOOLS_H
#define BWTL_MACHINING_TOOLS_H
#include <ProHole.h>
#include <ProRound.h>
#include <ProChamfer.h>
#include <ProCrvcollection.h>
typedef struct {ProVector center,normal;double radius,length,angle;int cylinders;ProError error;} McHoleGeometry;
#include "功能/machining_tools/mc_references.inc"
#include "功能/machining_tools/mc_verify.inc"
#include "功能/machining_tools/mc_hole.inc"
#include "功能/machining_tools/mc_edge.inc"
#include "功能/machining_tools/mc_execute.inc"
#endif
