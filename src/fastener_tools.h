#ifndef BWTL_FASTENER_TOOLS_H
#define BWTL_FASTENER_TOOLS_H
typedef struct {
    PanelPlan panel;
    PanelProfile marker;
    double margin,min_thick,max_thick,min_actual,reference_clearance;
    double box_width,box_height;
} FastenerPlan;
#include "功能/fastener/fx_boundary.inc"
#include "功能/fastener/fx_key.inc"
#include "功能/fastener/fx_slot.inc"
#include "功能/fastener/fx_corners.inc"
#include "功能/fastener/fx_pcd.inc"
#include "功能/fastener/fx_hinge.inc"
#include "功能/fastener/fx_press.inc"
#include "功能/fastener/fx_rivet.inc"
#include "功能/fastener/fx_ground.inc"
#include "功能/fastener/fx_tie.inc"
#include "功能/fastener/fx_tab.inc"
#include "功能/fastener/fx_execute.inc"
#endif
