
#ifndef BWTL_SHEETMETAL_PRODUCTION_TOOLS_H
#define BWTL_SHEETMETAL_PRODUCTION_TOOLS_H

static ProError smx_verify_changed_feature(FdTxn *tx){
    int had_volume=tx->has_volume;
    tx->has_volume=0;
    ProError error=fd_verify(tx);
    tx->has_volume=had_volume;
    if(!error)error=part_health(tx->model);
    return error;
}

#include "功能/sheetmetal/smx_standards.inc"
#include "功能/sheetmetal/smx_thickness.inc"
#include "功能/sheetmetal/smx_geometry.inc"
#include "功能/sheetmetal/smx_bend_control.inc"
#include "功能/sheetmetal/smx_rebuild.inc"
#include "功能/sheetmetal/smx_body_recovery.inc"
#include "功能/sheetmetal/smx_corner_join.inc"
#include "功能/sheetmetal/smx_miter.inc"
#include "功能/sheetmetal/smx_rip.inc"
#include "功能/sheetmetal/smx_openings.inc"
#include "功能/sheetmetal/smx_opening_inspection.inc"
#include "功能/sheetmetal/smx_join_fit.inc"
#include "功能/sheetmetal/workshop_form.inc"
#include "功能/sheetmetal/workshop_fastening.inc"
#include "功能/sheetmetal/workshop_fastener_access.inc"
#include "功能/sheetmetal/workshop_tool_withdrawal.inc"
#include "功能/sheetmetal/workshop_door_fit.inc"
#include "功能/sheetmetal/workshop_vent_interface.inc"
#include "功能/sheetmetal/workshop_mount_inspection.inc"
#include "功能/sheetmetal/workshop_weld_layout.inc"
#include "功能/sheetmetal/workshop_enclosure.inc"
#include "功能/sheetmetal/workshop_cable_entry.inc"
#include "功能/sheetmetal/workshop_stock_input.inc"
#include "功能/sheetmetal/workshop_stock_nesting.inc"
#include "功能/sheetmetal/workshop_stock_execute.inc"
#include "功能/sheetmetal/workshop_join_card.inc"
#include "功能/sheetmetal/workshop_flat_geometry.inc"
#include "功能/sheetmetal/workshop_flat_execute.inc"
#include "功能/sheetmetal/workshop_mirror_pair.inc"
#include "功能/sheetmetal/workshop_dfm.inc"
#include "功能/sheetmetal/workshop_form_interaction.inc"
#include "功能/sheetmetal/workshop_finish_checks.inc"
#include "功能/sheetmetal/workshop_gauge_face.inc"
#include "功能/sheetmetal/workshop_bend_schedule.inc"
#include "功能/sheetmetal/workshop_bend_process.inc"
#include "功能/sheetmetal/workshop_bend_tool_library.inc"
#include "功能/sheetmetal/workshop_versions.inc"
#include "功能/sheetmetal/workshop_delivery.inc"
#include "功能/sheetmetal/workshop_drawing.inc"
#include "功能/sheetmetal/workshop_form_flat.inc"
#include "功能/sheetmetal/workshop_flat_mapping.inc"

static ProError smx_execute(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    if(smx_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg,kind)!=1)return PRO_TK_BAD_INPUTS;
    if(dv_recovery_uncertain||(!smx_read_only(kind,cmd)&&sh_recovery_uncertain))return PRO_TK_BAD_CONTEXT;
    ProMdlsubtype subtype;
    ProError error=model?ProMdlSubtypeGet(model,&subtype):PRO_TK_NO_ERROR;
    if(error)return error;
    if(model&&kind!=SMX_MATERIAL&&kind!=SMX_SUBSTITUTE&&kind!=SMX_RULE&&kind!=SMX_COUPON&&
        (kind<=SMX_REVISION||kind==SMX_DXF_REBUILD||kind==SMX_RIP||kind>=SMX_OUTER)&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_BAD_CONTEXT;
    if(kind==SMY_FORM_RECESS||kind==SMY_FORM_BOSS||kind==SMY_FORM_LIBRARY||kind==SMY_FORM_CLEARANCE)return smy_form_execute(model,cmd,kind,out);
    if(kind==SMY_FORM_FLAT)return smy_form_flat(model,cmd,out);
    if(kind==SMY_PRESS_CHECK||kind==SMY_RIVET_GRIP)return smy_fastener_check(model,cmd,kind,out);
    if(kind==SMY_PRESS_STUD)return smy_press_stud(model,cmd,out);
    if(kind==SMY_PRESS_ACCESS||kind==SMY_DRIVER_ACCESS)return smy_tool_access(model,cmd,kind,out);
    if(kind==SMY_GAUGE_ACCESS)return smy_gauge_access(model,cmd,out);
    if(kind==SMY_RIVET_ALIGN)return smy_rivet_align(model,cmd,out);
    if(kind==SMY_SPOT_WELDS)return smy_spot_welds(model,cmd,out);
    if(kind==SMY_PLUG_SLOT)return smy_plug_slot(model,cmd,out);
    if(kind==SMY_JOIN_CARD)return smy_join_card(model,cmd,out);
    if(kind==SMY_WELD_SCHEDULE)return smy_weld_schedule(model,cmd,out);
    if(kind==SMY_CASE_SPACE)return smy_case_space(model,cmd,out);
    if(kind==SMY_CASE_FLUSH||kind==SMY_CASE_SEAL)return smy_door_fit(model,cmd,kind,out);
    if(kind==SMY_VENT_INTERFACE)return smy_vent_interface(model,cmd,out);
    if(kind==SMY_MOUNT_INSPECTION)return smy_mount_inspection(model,cmd,out);
    if(kind==SMY_TOOL_WITHDRAWAL)return smy_tool_withdrawal(model,cmd,out);
    if(kind==SMY_FINISH_PROTECTION)return smy_finish_protection(model,cmd,out);
    if(kind==SMY_COATING_POINT)return smy_coating_point(model,cmd,out);
    if(kind==SMY_CABLE_ENTRY)return smy_cable_entry(model,cmd,out);
    if(kind==SMY_CASE_DISPLAY)return smy_case_display(model,cmd,out);
    if(kind==SMY_MIRROR_PAIR)return smy_mirror_pair(model,cmd,out);
    if(kind==SMY_GAUGE_FACE)return smy_gauge_face(model,cmd,out);
    if(kind==SMY_DFM_FLANGE||kind==SMY_DFM_SHORT)return smy_dfm_run(model,cmd,kind,out);
    if(kind==SMY_FORM_INTERACTION)return smy_form_interaction(model,cmd,out);
    if(kind==SMY_BEND_NUMBER||kind==SMY_BEND_ORDER||kind==SMY_BEND_CANDIDATES||kind==SMY_BEND_SEGMENTS||kind==SMY_BEND_CARD)
        return smy_bend_execute(model,cmd,kind,out);
    if(kind==SMY_BEND_TOOL)return smy_bend_tool(model,cmd,out);
    if(kind==SMY_BEND_FORCE||kind==SMY_BEND_TRIAL)return smy_bend_process(model,cmd,kind,out);
    if(kind==SMY_VERSION_COMPARE)return smy_version_compare(model,cmd,out);
    if(kind==SMY_DRAW_TEMPLATE)return smy_draw_template(model,cmd,out);
    if(kind==SMY_FIRST_CARD)return smy_first_card(model,cmd,out);
    if(kind==SMY_DELIVERY_CHECK)return smy_package_check(model,cmd,out);
    if(kind==SMY_DELIVERY_BUNDLE)return smy_package_bundle(model,cmd,out);
    if(kind==SMY_FLAT_HOLE_MAP)return smy_flat_hole_map(model,cmd,out);
    if(kind==SMY_FLAT_BEND_LAYERS)return smy_flat_bend_layers(model,cmd,out);
    if(kind>=SMY_FLAT_COMPARE&&kind<=SMY_FLAT_LOCK)return smy_flat_execute(model,cmd,kind,out);
    if(kind>=SMY_STOCK_MEASURE&&kind<=SMY_STOCK_TOTAL)return smy_stock_execute(model,cmd,kind,out);
    switch(kind){
    case SMX_TEMPLATE:return smx_template_create(cmd,out);
    case SMX_MATERIAL:case SMX_SUBSTITUTE:return smx_material_query(cmd,kind,out);
    case SMX_METADATA:return smx_metadata(model,cmd,out);
    case SMX_THICK_PREVIEW:case SMX_THICK_APPLY:return smx_thickness(model,cmd,kind==SMX_THICK_APPLY,out);
    case SMX_FRAMES:return smx_frames(model,cmd,out);
    case SMX_GRAIN:return smx_grain(model,cmd,out);
    case SMX_RULE:return smx_bend_rule(cmd,out);
    case SMX_REVISION:return smx_revision(model,cmd,out);
    case SMX_GAUGE:case SMX_ATTACH:case SMX_PAIR:case SMX_CONFIDENCE:case SMX_BODIES:return smx_geo_report(model,kind,out);
    case SMX_REBUILD:return smx_rebuild_body(model,cmd,out);
    case SMX_SPLIT:return smx_split_body(model,cmd,out);
    case SMX_DXF_REBUILD:return smx_dxf_rebuild(model,cmd,out);
    case SMX_BEND_OFFSET:return smx_bend_offset(model,cmd,out);
    case SMX_RIP:return smx_rip_candidates(model,cmd,out);
    case SMX_MITER:case SMX_MITER_OFFSET:case SMX_WELD_SEAM:return smx_miter_pair(model,cmd,kind,out);
    case SMX_JOGGLE:return smx_joggle_fit(model,cmd,out);
    case SMX_RELIEF:case SMX_RELIEF_COMPARE:return smx_relief_apply(model,cmd,kind,out);
    case SMX_FLANGE_CLEARANCE:return smx_flange_clearance(model,cmd,out);
    case SMX_TAB_PAIR:return smx_tabs_pair(model,cmd,out);
    case SMX_CLOSED_HEM:case SMX_PIN_BARREL:return smx_hem_fit(model,cmd,kind,out);
    case SMX_GRID:case SMX_CLIP_GRID:case SMX_MOUNT:case SMX_INTERFACE:return smx_opening_patterns(model,cmd,kind,out);
    case SMX_NORMAL:case SMX_BEND_CLEARANCE:case SMX_WEB_CLEARANCE:return smx_hole_check(model,cmd,kind,out);
    case SMX_CROSS_BEND:case SMX_LASER:return smx_flat_openings(model,cmd,kind,out);
    case SMX_EDIT_CIRCLES:return smx_edit_circles(model,cmd,out);
    case SMX_GEOMETRY_DIFF:return smx_geometry_compare(model,cmd,out);
    case SMX_OUTER:case SMX_INNER:case SMX_TO_PLANE:return smx_sized_flange(model,cmd,kind,out);
    case SMX_PROFILE:return smx_profile_chain(model,cmd,out);
    case SMX_DIMENSION:return smx_bend_dimension(model,cmd,out);
    case SMX_DIRECTIONS:return smx_direction_compare(model,cmd,out);
    case SMX_RADIUS:return smx_radius_check(model,cmd,out);
    case SMX_ALLOWANCE_SOURCE:return smx_allowance_report(model,out);
    case SMX_COUPON:return smx_coupon(cmd,out);
    }
    return PRO_TK_BAD_INPUTS;
}

static ProError smx_new(const PartCommand *cmd,const PartSnapshot *snapshot,wchar_t *report,size_t capacity){
    PartText out={report,capacity,0};
    ProPath directory;
    ProMdl active=NULL;
    ProError error=ProDirectoryCurrentGet(directory);
    if(error||!snapshot->directory[0]||_wcsicmp(directory,snapshot->directory)){
        part_text_add(&out,L"Creo 工作目录已变化，请核对后重发。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    error=ProMdlCurrentGet(&active);
    if((error&&error!=PRO_TK_BAD_CONTEXT&&error!=PRO_TK_E_NOT_FOUND)||active!=snapshot->window_model){
        part_text_add(&out,L"活动模型已变化，请核对后重发。\n");
        return error?error:PRO_TK_BAD_CONTEXT;
    }
    return smx_execute(NULL,cmd,smx_kind(cmd->arg[0]),&out);
}

#endif
