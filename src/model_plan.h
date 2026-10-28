#ifndef CREO_MODEL_PLAN_H
#define CREO_MODEL_PLAN_H
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <math.h>
typedef struct { int cylinder; double a, b, height, hole; int ready; int empty_kind; } ModelPlan;
/* Host routing guard: a current-model request must not silently become a
   new file merely because an AI returned the older CREO_PLAN protocol. */
static int model_request_current(const wchar_t *text){
    int explicit_new=wcsstr(text,L"新建")||wcsstr(text,L"独立零件")||wcsstr(text,L"另建");
    if(wcsstr(text,L"不新建")||wcsstr(text,L"不要新建")||wcsstr(text,L"别新建")||wcsstr(text,L"不另建"))explicit_new=0;
    return !explicit_new && (wcsstr(text,L"当前")||wcsstr(text,L"这个零件")||wcsstr(text,L"这个模型")||wcsstr(text,L"拉伸"));
}
static int model_empty_request(const wchar_t *text){
    const wchar_t *phrases[]={L"创建一个钣金零件",L"创建钣金零件",L"新建一个钣金零件",L"新建钣金零件",L"创建一个钣金空零件",L"新建钣金空零件",
        L"创建一个实体零件",L"新建一个实体零件",L"创建一个空零件",L"新建一个空零件",L"创建实体空零件",
        L"创建一个装配",L"新建一个装配",L"创建一个空装配",L"新建空装配"};
    wchar_t value[80];size_t n=wcslen(text);int i;
    if(n>=80)return 0;wcscpy_s(value,80,text);
    while(n && (iswspace(value[n-1])||value[n-1]==L'。'||value[n-1]==L'！'||value[n-1]==L'.'||value[n-1]==L'!'))value[--n]=0;
    for(i=0;i<15;i++)if(!wcscmp(value,phrases[i]))return i<6?2:i<11?1:3;
    return 0;
}
static int model_plan_valid(const ModelPlan *p) {
    if (p->empty_kind) return p->empty_kind >= 1 && p->empty_kind <= 3;
    if (!isfinite(p->a) || !isfinite(p->b) || !isfinite(p->height) || !isfinite(p->hole)) return 0;
    if (p->a < 0.1 || p->b < 0.1 || p->height < 0.1 || p->a > 10000 || p->b > 10000 || p->height > 10000) return 0;
    if (p->hole < 0 || (p->hole > 0 && p->hole < 0.1)) return 0;
    if (p->hole > 0 && (p->hole >= p->a - 0.1 || p->hole >= p->b - 0.1)) return 0;
    if (p->cylinder && fabs(p->a - p->b) > 0.000001) return 0;
    return 1;
}
/* Accept one exact whole line, never eval generated code or commands. */
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
static double model_plan_volume(const ModelPlan *p) {
    double area = p->cylinder ? 3.14159265358979323846 * p->a * p->a / 4 : p->a * p->b;
    return (area - 3.14159265358979323846 * p->hole * p->hole / 4) * p->height;
}
/* Ordinary discussion must not consume the last executable proposal.
   A malformed replacement explicitly clears it to avoid stale execution. */
static int model_plan_update(const wchar_t *answer, ModelPlan *pending) {
    ModelPlan next = {0};
    if (model_plan_parse(answer, &next)) { *pending = next; return 1; }
    if (wcsncmp(answer, L"CREO_PLAN ", 10) == 0 || wcsstr(answer, L"\nCREO_PLAN ")) { pending->ready = 0; return -1; }
    return 0;
}
static void model_plan_format(const ModelPlan *p, wchar_t text[256]) {
    if (!p->ready) { text[0] = 0; return; }
    if(p->empty_kind){swprintf_s(text,256,L"CREO_PLAN %ls",p->empty_kind==1?L"EMPTY_SOLID":p->empty_kind==2?L"EMPTY_SHEETMETAL":L"EMPTY_ASSEMBLY");return;}
    swprintf_s(text, 256, L"CREO_PLAN %ls %.17g %.17g %.17g %.17g", p->cylinder ? L"CYLINDER" : L"BOX", p->a, p->b, p->height, p->hole);
}
#endif
