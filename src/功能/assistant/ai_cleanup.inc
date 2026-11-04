static void ai_cleanup(void) {
    if (ai_process) { TerminateProcess(ai_process, 1); CloseHandle(ai_process); ai_process = NULL; }
}
