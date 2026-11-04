static int model_plan_update(const wchar_t *answer, ModelPlan *pending) {
    ModelPlan next = {0};
    if (model_plan_parse(answer, &next)) { *pending = next; return 1; }
    if (wcsncmp(answer, L"CREO_PLAN ", 10) == 0 || wcsstr(answer, L"\nCREO_PLAN ")) { pending->ready = 0; return -1; }
    return 0;
}
