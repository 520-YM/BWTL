#ifndef BWTL_PANEL_TOOLS_H
#define BWTL_PANEL_TOOLS_H

enum PanelShape {PN_CIRCLE,PN_RECTANGLE,PN_ROUNDED,PN_NOTCH,PN_FLAT,PN_EDGE_SLOT,PN_KEY,PN_CAPSULE,PN_HEX,PN_SHOP_GROOVE};

typedef struct {
    enum PanelShape shape;
    double x,y,width,height,detail,angle;
} PanelProfile;

typedef struct {
    PanelProfile through[12],seat;
    int count;
    double thickness,seat_depth,bezel_x,bezel_y;
} PanelPlan;

#include "功能/panel/pn_geometry.inc"
#include "功能/panel/pn_button.inc"
#include "功能/panel/pn_estop.inc"
#include "功能/panel/pn_power.inc"
#include "功能/panel/pn_port.inc"
#include "功能/panel/pn_fan.inc"
#include "功能/panel/pn_display.inc"
#include "功能/panel/pn_seat.inc"
#include "功能/panel/pn_handle.inc"
#include "功能/panel/pn_lock.inc"
#include "功能/panel/pn_comb.inc"
#include "功能/panel/pn_native.inc"
#include "功能/panel/pn_verify.inc"
#include "功能/panel/pn_execute.inc"
#endif
