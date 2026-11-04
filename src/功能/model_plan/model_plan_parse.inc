static int model_plan_parse(const wchar_t *answer, ModelPlan *p) {
    const wchar_t *line = answer;
    int seen = 0;
    ModelPlan candidate = {0};
    p->ready = 0;
    while (*line) {
        const wchar_t *end = wcschr(line, L'\n');
        size_t length = end ? (size_t)(end - line) : wcslen(line);
        if (wcsncmp(line, L"CREO_PLAN ", 10) == 0) {
            wchar_t copy[256], *token, *state, *number_end;
            double values[4]; int i;
            if (++seen > 1 || length >= 256) return 0;
            wcsncpy_s(copy, 256, line, length);
            token = wcstok_s(copy, L" \t\r", &state);
            token = wcstok_s(NULL, L" \t\r", &state);
            if (!token) return 0;
            if (wcscmp(token,L"EMPTY_SOLID")==0 || wcscmp(token,L"EMPTY_SHEETMETAL")==0 || wcscmp(token,L"EMPTY_ASSEMBLY")==0) {
                candidate.empty_kind = wcscmp(token,L"EMPTY_SOLID")==0 ? 1 : wcscmp(token,L"EMPTY_SHEETMETAL")==0 ? 2 : 3;
                if(wcstok_s(NULL,L" \t\r",&state))return 0;
                goto line_complete;
            }
            if (wcscmp(token, L"BOX") == 0) candidate.cylinder = 0;
            else if (wcscmp(token, L"CYLINDER") == 0) candidate.cylinder = 1;
            else return 0;
            for (i = 0; i < 4; ++i) {
                token = wcstok_s(NULL, L" \t\r", &state);
                if (!token) return 0;
                values[i] = wcstod(token, &number_end);
                if (number_end == token || *number_end) return 0;
            }
            if (wcstok_s(NULL, L" \t\r", &state)) return 0;
            candidate.a = values[0]; candidate.b = values[1]; candidate.height = values[2]; candidate.hole = values[3];
            if (!model_plan_valid(&candidate)) return 0;
        }
line_complete:
        if (!end) break;
        line = end + 1;
    }
    if (seen != 1) return 0;
    candidate.ready = 1; *p = candidate; return 1;
}
