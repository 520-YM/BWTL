static void execute_part_command(void){
    ULONGLONG execution_started=GetTickCount64();
    static wchar_t result[PART_TEXT_MAX];
    PartCommand command=pending_part;
    memset(&pending_part,0,sizeof(pending_part));
    if(command.verb==PART_NONE)return;
    /* The first write after loading a new DLL must pass a real native
       check. This is not counted as successful during compilation. */
    if((command.verb==PART_DIMS || command.verb==PART_PARAM || command.verb==PART_SUPPRESS || command.verb==PART_RESUME) && !part_selfcheck_passed){
        if(part_selfcheck(result,PART_TEXT_MAX)){append_card(L"执行结果",result);save_turn(L"原生执行结果",result);return;}
        append_card(L"执行结果",result);
    }
    part_execute(&command,&request_snapshot,result,PART_TEXT_MAX);
    trace("PERF native_execute_ms",(int)(GetTickCount64()-execution_started));
    append_card(L"执行结果",result);save_turn(L"原生执行结果",result);
}
