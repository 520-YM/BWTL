static int build_pending_command(uiCmdCmdId command, uiCmdValue *value, void *data) {
    if(pending_part.verb!=PART_NONE)execute_part_command();else if(smart_active)execute_model_plan(); return 0;
}
