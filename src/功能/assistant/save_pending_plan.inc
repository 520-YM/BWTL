static int save_pending_plan(void) {
    wchar_t text[256];
    model_plan_format(&pending_plan, text);
    if (!store_ready || !store_write(L"pending-plan.txt", text)) {
        append(L"待执行方案状态保存失败。为避免下次重复执行，本次不启动建模。"); return 0;
    }
    return 1;
}
