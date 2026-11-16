static ProError ax_execute(ProMdl assembly,const PartCommand *cmd,wchar_t *report,size_t capacity){
    PartText out={report,capacity,0};
    int kind=ax_kind(cmd->arg[0]);
    ProError error;
    double scale=1;
    if(assembly_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(ax_recovery_uncertain&&ax_mutates(kind)&&!ax_delete_preview(cmd)){
        part_text_add(&out,L"上次恢复未能核实，当前会话已停止装配写入。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    if(kind!=AX_TREE&&kind!=AX_Q_REGEN&&kind!=AX_Q_REPORT&&kind!=AX_V_ISOLATE&&kind!=AX_V_VIEW&&(error=ax_master(assembly))){
        part_text_add(&out,L"本操作需要活动装配处于主表示，且动态定位关闭。\n");
        return error;
    }
    if(kind!=AX_Q_REGEN&&kind!=AX_Q_REPORT&&(error=qa_mm_per_unit(assembly,&scale)))goto done;
    switch(kind){
    case AX_TREE:error=ax_tree(assembly,cmd->arg[1],&out);break;
    case AX_INSERT:error=ax_insert(assembly,cmd,scale,&out);break;
    case AX_CSYS:error=ax_constrain(assembly,cmd,scale,&out);break;
    case AX_MOVE:error=ax_move(assembly,cmd,scale,&out);break;
    case AX_STATE:error=ax_state(assembly,cmd,&out);break;
    case AX_CLEARANCE:error=ax_fit_pair(assembly,cmd,scale,&out);break;
    case AX_INTERFERENCE:error=cmd->count==3?ax_fit_all(assembly,scale,&out):ax_fit_pair(assembly,cmd,scale,&out);break;
    case AX_MASS:error=ax_mass(assembly,scale,&out);break;
    case AX_BOM:error=ax_bom(assembly,cmd,&out);break;
    case AX_STEP:error=ax_step(assembly,cmd,&out);break;
    case AX_REFS:error=ax_placement_refs(assembly,cmd->arg[2],&out);break;
    case AX_PLACEMENT:error=ax_placement_status(assembly,cmd->arg[2],scale,&out);break;
    case AX_FIX:case AX_MATE:case AX_PARALLEL:case AX_OFFSET:case AX_COAX:case AX_THREE:case AX_FASTENER:case AX_EDIT_OFFSET:
        error=ax_placement_write(assembly,cmd,kind,scale,&out);break;
    case AX_INSERT_ASM:case AX_NEW_ASM:error=ax_subassembly(assembly,cmd,kind,scale,&out);break;
    case AX_DELETE_COMPONENT:error=ax_delete_component(assembly,cmd,&out);break;
    case AX_ROTATE_COMPONENT:case AX_FLIP_COMPONENT:case AX_UNFIX_COMPONENT:
        error=ax_component_pose(assembly,cmd,kind,scale,&out);break;
    case AX_COPY_COMPONENT:error=ax_copy_occurrence(assembly,cmd,scale,&out);break;
    case AX_REPLACE_COMPONENT:error=ax_replace_occurrence(assembly,cmd,scale,&out);break;
    case AX_EDIT_REF:case AX_REPAIR_REF:error=ax_change_occurrence_ref(assembly,cmd,kind,scale,&out);break;
    case AX_ARRAY_LINEAR:case AX_ARRAY_GRID:case AX_ARRAY_CIRCULAR:case AX_MIRROR_COMPONENT:
        error=ax_place_array(assembly,cmd,kind,scale,&out);break;
    case AX_HOLE_ARRAY:case AX_PRESS_HARDWARE:case AX_FASTENER_STACK:case AX_FASTENER_FIT:case AX_FASTENER_AUDIT:
        error=ax_hardware(assembly,cmd,kind,scale,&out);break;
    case AX_REPLACE_BATCH:error=ax_replace_batch(assembly,cmd,scale,&out);break;
    case AX_SCREEN_MOUNT:case AX_GLASS_MOUNT:case AX_FILTER_MOUNT:case AX_LATCH_MOUNT:
        error=ax_device_window(assembly,cmd,kind,scale,&out);break;
    case AX_POWER_MOUNT:case AX_PCB_MOUNT:case AX_FAN_MOUNT:
        error=ax_device_bolts(assembly,cmd,kind,scale,&out);break;
    case AX_RAIL_MOUNT:error=ax_device_rail(assembly,cmd,scale,&out);break;
    case AX_HINGE_MOUNT:error=ax_device_hinges(assembly,cmd,scale,&out);break;
    case AX_SEAL_CHECK:error=ax_device_seal(assembly,cmd,scale,&out);break;
    case AX_JOINT_PIN:case AX_JOINT_SLIDER:case AX_JOINT_CYLINDER:case AX_ROTATION_LIMIT:case AX_TRAVEL_LIMIT:case AX_MOTION_PRESET:
        error=ax_joint_command(assembly,cmd,kind,scale,&out);break;
    case AX_MOTION_SWEEP:case AX_DOOR_SWEEP:case AX_TRAY_SWEEP:case AX_TOOL_SWEEP:
        error=ax_motion_sweep(assembly,cmd,kind,scale,&out);break;
    case AX_LAYOUT_DATUM:case AX_LAYOUT_INTERFACE:
        error=ax_layout_datums(assembly,cmd,kind,scale,&out);break;
    case AX_LAYOUT_ENVELOPE:case AX_LAYOUT_SKELETON:
        error=ax_layout_build(assembly,cmd,kind,scale,&out);break;
    case AX_LAYOUT_LINK:error=ax_layout_link(assembly,cmd,scale,&out);break;
    case AX_LAYOUT_COPY:error=ax_layout_copy(assembly,cmd,scale,&out);break;
    case AX_LAYOUT_DIM:error=ax_layout_dimension(assembly,cmd,scale,&out);break;
    case AX_LAYOUT_REFS:case AX_LAYOUT_AUDIT:
        error=ax_layout_refs(assembly,cmd,kind,&out);break;
    case AX_LAYOUT_COMPARE:case AX_V_COMPARE:error=ax_layout_compare(assembly,cmd,scale,&out);break;
    case AX_FIELD_CUT:case AX_FIELD_HOLE:case AX_FIELD_GRID:case AX_FIELD_TRANSFER:
    case AX_FIELD_LAYERS:case AX_FIELD_WINDOW:case AX_FIELD_ROUTE:
        error=ax_field_execute(assembly,cmd,kind,scale,&out);break;
    case AX_FIELD_SHIM:error=ax_field_shim(assembly,cmd,scale,&out);break;
    case AX_FIELD_MARK:error=ax_field_mark(assembly,cmd,&out);break;
    case AX_FIELD_AUDIT:error=ax_field_audit(assembly,cmd->arg[2],&out);break;
    case AX_Q_REGEN:case AX_Q_GAPS:case AX_Q_INTERF:case AX_Q_HOLES:case AX_Q_FLUSH:
    case AX_Q_COPLANE:case AX_Q_STACK:case AX_Q_SUPPORT:case AX_Q_SPACE:case AX_Q_REPORT:
        error=ax_quality_execute(assembly,cmd,kind,scale,&out);break;
    case AX_V_REP:case AX_V_ISOLATE:error=ax_view_rep(assembly,cmd,kind==AX_V_ISOLATE,&out);break;
    case AX_V_VIEW:error=ax_view_named(assembly,cmd,&out);break;
    case AX_V_SECTION:error=ax_view_section(assembly,cmd,scale,&out);break;
    case AX_V_EXPLODE:error=ax_view_explode(assembly,cmd,scale,&out);break;
    case AX_V_SEQUENCE:error=ax_view_sequence(assembly,cmd,&out);break;
    case AX_V_CONFIG:case AX_V_APPLY:case AX_V_ORDER:error=ax_config_execute(assembly,cmd,kind,scale,&out);break;
    case AX_B_TREE:case AX_B_BUY:case AX_B_DIFF:case AX_B_CHECK:case AX_B_FAST:case AX_B_WIRE:
    case AX_B_SPARE:case AX_B_ROUTE:case AX_B_NATIVE:case AX_B_PACK:
        error=ax_bom_delivery(assembly,cmd,kind,&out);break;
    default:error=kind>AW_NONE?aw_execute(assembly,cmd,kind,scale,&out):PRO_TK_BAD_INPUTS;break;
    }
done:
    if(error)part_text_add(&out,L"%ls 未完成（Creo %d）。请核对组件路径、装配状态和操作范围。\n",cmd->arg[0],error);
    return error;
}
