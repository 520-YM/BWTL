static void model_section_errors(ProWSecerror *errors, const char *stage) {
    int count = 0, i;
    FILE *log;
    if (ProSecerrorCount(errors, &count) != PRO_TK_NO_ERROR) return;
    trace("Section solver error count", count);
    log = runtime_open(L"section-errors.txt",L"wt, ccs=UTF-8");
    if (!log) return;
    fwprintf(log, L"Failed stage: %hs\n", stage);
    for (i = 0; i < count; ++i) {
        ProMsg message = L"";
        int entity = -1;
        ProSecerrorMsgGet(*errors, i, message);
        ProSecerrorItemGet(*errors, i, &entity);
        fwprintf(log, L"Entity %d: %ls\n", entity, message);
    }
    fclose(log);
}
