static ProError aw_execute(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    switch(kind){
    case AW_FIND:return aw_find(assembly,cmd,mm,out);
    case AW_OCCURRENCE:return aw_occurrence(assembly,cmd->arg[2],mm,out);
    case AW_CONFLICT:case AW_FREEDOM:return aw_diagnose(assembly,cmd->arg[2],kind,mm,out);
    case AW_PAIRS:return aw_pair_references(assembly,cmd,mm,out);
    case AW_ORIENTATION:case AW_FANS:return aw_orientation(assembly,cmd,kind,out);
    case AW_SCREW_LENGTH:case AW_BLIND_BOTTOM:case AW_COUNTERSINK:case AW_PRESS_FIT:case AW_RIVET_GRIP:case AW_ASBUILT:
        return aw_hardware(assembly,cmd,kind,mm,out);
    case AW_THERMAL_GAP:return aw_thermal(assembly,cmd,mm,out);
    case AW_DOOR_GAP:return aw_door_gaps(assembly,cmd,mm,out);
    case AW_DRAIN:return aw_drain(assembly,cmd,mm,out);
    case AW_REVISIONS:return aw_revisions(assembly,cmd,out);
    case AW_ALIGN:case AW_ALIGN_BATCH:return aw_align_batch(assembly,cmd,mm,out);
    case AW_CHECKPOINT:return aw_inspect_checkpoint(assembly,cmd,mm,out);
    case AW_RESULT:return aw_show_batch_result(assembly,out);
    case AW_SQUARE:return aw_square(assembly,cmd,mm,out);
    case AW_TWIST:return aw_twist(assembly,cmd,mm,out);
    case AW_LEVEL:return aw_level(assembly,cmd,mm,out);
    case AW_ANCHORS:return aw_anchors(assembly,cmd,mm,out);
    case AW_WIRE_LENGTH:return aw_cable_length(assembly,cmd,mm,out);
    case AW_WIRE_BEND:return aw_cable_bend(assembly,cmd,mm,out);
    case AW_CHANNEL_FILL:return aw_channel_fill(assembly,cmd,mm,out);
    case AW_DISPLAY_VIEW:case AW_BYPASS:return aw_display_view(assembly,cmd,mm,kind,out);
    case AW_AIRFLOW:return aw_airflow(assembly,cmd,mm,out);
    case AW_RAIN_COVER:return aw_rain_cover(assembly,cmd,mm,out);
    case AW_VOLTAGE_ZONE:return aw_voltage_clearance(assembly,cmd,mm,out);
    case AW_SEAL_LOOP:return aw_seal_loop(assembly,cmd,mm,out);
    case AW_SLOT_RULE:case AW_REQUIRED:case AW_TORQUE:case AW_TERMINALS:
    case AW_PRODUCTION_BOM:case AW_TEMPORARY:case AW_ISSUE_LIST:return aw_production(assembly,cmd,kind,out);
    case AW_SHARED_MODELS:return aw_shared_model(assembly,cmd,out);
    case AW_RAIL_CAPACITY:return aw_available_rail(assembly,cmd,mm,out);
    case AW_FASTENER_SIDE:return aw_fastener_side(assembly,cmd,mm,out);
    case AW_MIXED_SCREWS:return aw_mixed_fasteners(assembly,cmd,mm,out);
    case AW_ELECTRIC_LAYOUT:return aw_electric_layout(assembly,cmd,mm,out);
    case AW_INSTALL_ORDER:return aw_assembly_order(assembly,cmd,out);
    case AW_TRANSPORT:return aw_transport(assembly,cmd,mm,out);
    case AW_REWORK:return aw_rework(assembly,cmd,mm,out);
    case AW_METHOD_RECORD:return aw_method_record(assembly,cmd,mm,out);
    case AW_METHOD_RECALL:return aw_method_recall(assembly,cmd,mm,out);
    case AW_TOOL_ACCESS:case AW_DOOR_FINAL:case AW_TERMINAL_ACCESS:case AW_PLUG_PATH:
    case AW_RAIL_SERVICE:case AW_SMALL_SERVICE:case AW_FILTER_SERVICE:case AW_INSTALL_PATH:
        return aw_service_motion(assembly,cmd,kind,mm,out);
    case AW_DOOR_SHIM:return aw_shim_plan(assembly,cmd,mm,out);
    case AW_DOOR_WIRE:return aw_door_wire(assembly,cmd,mm,out);
    case AW_EDGE_WIRE:return aw_edge_clearance(assembly,cmd,mm,out);
    case AW_WIRE_CROSS:return aw_wire_crossing(assembly,cmd,mm,out);
    case AW_CONNECTIONS:return aw_connection_audit(assembly,cmd,out);
    case AW_WELD_CONFLICT:return aw_weld_check(assembly,cmd,mm,out);
    case AW_WELD_TOOL:return aw_service_motion(assembly,cmd,kind,mm,out);
    case AW_HIGHLIGHT:return aw_highlight(assembly,cmd,out);
    case AW_INTERFACE:return aw_interface_check(assembly,cmd,mm,out);
    case AW_CHANGE_PREVIEW:return aw_layout_preview(assembly,cmd,out);
    case AW_INTERFACE_DRAW:return aw_interface_draw(assembly,cmd,mm,out);
    case AW_ORDER_AUDIT:return aw_order_audit(assembly,cmd,out);
    case AW_BEAM_LAYOUT:case AW_TRAY_LAYOUT:return aw_distribute(assembly,cmd,kind,mm,out);
    case AW_DROP_FASTENER:case AW_LABEL_VIEW:case AW_WATER_PATH:case AW_RELEASE_DROP:return aw_rays_audit(assembly,cmd,kind,mm,out);
    case AW_GLASS_CORNER:return aw_glass_corners(assembly,cmd,mm,out);
    case AW_GLUE_SPACE:return aw_glue_space(assembly,cmd,mm,out);
    case AW_GLASS_PRESSURE:return aw_no_pressure(assembly,cmd,mm,out);
    case AW_HEAT_ZONE:return aw_heat_zone(assembly,cmd,mm,out);
    case AW_AIR_PAIR:return aw_air_pair(assembly,cmd,mm,out);
    case AW_ROUTE_WRITE:return aw_build_route(assembly,cmd,mm,out);
    case AW_CLIP_WRITE:return aw_place_clips(assembly,cmd,mm,out);
    case AW_GLASS_BLOCKS:return aw_glass_blocks(assembly,cmd,mm,out);
    case AW_DOOR_HAND:return aw_door_hand(assembly,cmd,mm,out);
    case AW_GROUND_BOND:return aw_ground_bond(assembly,cmd,mm,out);
    case AW_CONNECTOR_OPENING:return aw_connector_opening(assembly,cmd,mm,out);
    case AW_POSTURE:return aw_posture(assembly,cmd,mm,out);
    case AW_REASSEMBLE:return aw_reassembly_guide(assembly,cmd,mm,out);
    case AW_WORKPOINTS:return aw_field_checks(assembly,cmd,mm,out);
    case AW_REMOVAL_DRAW:return aw_disassembly_sheet(assembly,cmd,mm,out);
    case AW_ROOM_CLEARANCE:return aw_spacing_modes(assembly,cmd,mm,out);
    case AW_HANDLING:return aw_handling(assembly,cmd,mm,out);
    case AW_ORDER_IMPACT:return aw_order_impact(assembly,cmd,out);
    case AW_PRE_RELEASE:return aw_pre_release(assembly,cmd,mm,out);
    case AW_OUTDOOR_ACCESS:case AW_CLOSE_BLOCKS:case AW_TEMP_SUPPORT:case AW_REMOVE_MIN:case AW_CASTER_BRAKE:
        return aw_service_motion(assembly,cmd,kind,mm,out);
    default:return PRO_TK_BAD_INPUTS;
    }
}
