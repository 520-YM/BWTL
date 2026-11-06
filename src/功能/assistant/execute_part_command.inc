static void execute_part_command(void){
    ULONGLONG execution_started=GetTickCount64();
    static wchar_t result[PART_TEXT_MAX];ProError error;
    PartCommand command=pending_part;
    memset(&pending_part,0,sizeof(pending_part));
    if(command.verb==PART_NONE||!smart_active)return;
    if(!smart_protocol_allowed(&command)){smart_reply(smart_write_block_message());return;}
    /* The first write after loading a new DLL must pass a real native
       check. This is not counted as successful during compilation. */
    if((command.verb==PART_DIMS || command.verb==PART_PARAM || command.verb==PART_SUPPRESS || command.verb==PART_RESUME) && !part_selfcheck_passed){
        if(part_selfcheck(result,PART_TEXT_MAX)){smart_record_tool(result);smart_reply(result);return;}
    }
    error=part_execute(&command,&request_snapshot,result,PART_TEXT_MAX);
    trace("PERF native_execute_ms",(int)(GetTickCount64()-execution_started));
    smart_after_tool(error,result,command.verb==PART_EXTRUDE||command.verb==PART_DIMS||
        command.verb==PART_PARAM||command.verb==PART_SUPPRESS||command.verb==PART_RESUME||
        (command.verb==PART_FOUNDATION&&!smart_read_command(&command)&&wcscmp(command.arg[0],L"SKETCH_BEGIN")));
}
