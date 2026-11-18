static void ai_cleanup(void){
    if(!ai_process)return;
    wchar_t sequence[24];
    swprintf_s(sequence,_countof(sequence),L"%lu",resident_sequence);
    resident_write(L"resident-cancel.txt",sequence);
    ai_process=NULL;
    if(resident_tool_queued||resident_tool_inflight)resident_reset_thread=1;
    resident_tool_queued=resident_tool_inflight=0;
}

