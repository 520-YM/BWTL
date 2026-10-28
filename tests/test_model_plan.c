#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include "../src/model_plan.h"
int main(int argc, char **argv) {
    ModelPlan p = {0};
    assert(model_request_current(L"拉伸一个正方体"));
    assert(model_request_current(L"不新建，在当前零件拉伸一个20毫米正方体"));
    assert(!model_request_current(L"新建独立零件，拉伸一个20毫米正方体"));
    assert(!model_request_current(L"创建一个空零件"));
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rt, ccs=UTF-8");
        wchar_t buffer[16000]; size_t count = 0; wint_t ch;
        assert(file);
        while (count < 15999 && (ch = fgetwc(file)) != WEOF) buffer[count++] = (wchar_t)ch;
        buffer[count] = 0; fclose(file);
        assert(model_plan_parse(buffer, &p));
        assert(p.a == 100 && p.b == 80 && p.height == 2 && p.hole == 10 && !p.cylinder);
        puts("REAL AI OUTPUT PARSED AND DIMENSIONS VERIFIED"); return 0;
    }
    assert(model_plan_parse(L"创建板\nCREO_PLAN BOX 100 80 2 0\n", &p));
    assert(!p.cylinder && p.ready && fabs(model_plan_volume(&p) - 16000) < 1e-8);
    assert(model_plan_parse(L"CREO_PLAN CYLINDER 50 50 100 10", &p));
    assert(p.cylinder && fabs(model_plan_volume(&p) - 60000*3.141592653589793) < 1e-7);
    assert(model_plan_parse(L"CREO_PLAN BOX 0.1 0.1 0.1 0", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX 100 80 2 80", &p) && !p.ready);
    assert(!model_plan_parse(L"CREO_PLAN BOX -1 80 2 0", &p));
    assert(!model_plan_parse(L"CREO_PLAN CYLINDER 50 60 1 0", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX nan 80 2 0", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX inf 80 2 0", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX 1e300 80 2 0", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX 10 10 1 0.01", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX 10 10 1 0 run-this", &p));
    assert(!model_plan_parse(L"CREO_PLAN SHELL 10 10 1 0", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX 10 10 1", &p));
    assert(!model_plan_parse(L"CREO_PLAN BOX 10 10 1 0\nCREO_PLAN BOX 10 10 1 0", &p));
    assert(!model_plan_parse(L"这是能力说明，没有建模指令", &p));
    assert(model_plan_update(L"CREO_PLAN BOX 100 80 2 10", &p) == 1 && p.ready);
    assert(model_plan_update(L"你可以输入执行建模。", &p) == 0 && p.ready && p.hole == 10);
    {
        wchar_t serialized[256]; ModelPlan restored = {0};
        model_plan_format(&p, serialized);
        assert(model_plan_parse(serialized, &restored));
        assert(restored.a == p.a && restored.b == p.b && restored.height == p.height && restored.hole == p.hole);
    }
    assert(model_plan_update(L"CREO_PLAN BOX -1 80 2 10", &p) == -1 && !p.ready);
    assert(model_plan_update(L"CREO_PLAN CYLINDER 50 50 100 0", &p) == 1 && p.ready && p.cylinder);
    assert(model_plan_parse(L"CREO_PLAN EMPTY_SHEETMETAL",&p)&&p.empty_kind==2&&p.ready);
    assert(model_plan_parse(L"CREO_PLAN EMPTY_SOLID",&p)&&p.empty_kind==1);
    assert(model_plan_parse(L"CREO_PLAN EMPTY_ASSEMBLY",&p)&&p.empty_kind==3);
    assert(!model_plan_parse(L"CREO_PLAN EMPTY_SHEETMETAL 100 80",&p));
    assert(!model_plan_parse(L"CREO_PLAN EMPTY_SHEETMETAL\nCREO_PLAN BOX 10 10 2 0",&p));
    assert(model_empty_request(L"创建一个钣金零件")==2);
    assert(model_empty_request(L"创建一个钣金零件。")==2);
    assert(model_empty_request(L"创建一个实体零件")==1);
    assert(model_empty_request(L"创建一个装配")==3);
    assert(!model_empty_request(L"不要创建一个钣金零件"));
    assert(!model_empty_request(L"创建一个钣金零件，再开孔"));
    puts("MODEL PLAN VALIDATION / VOLUME / INVALID AND MULTIPLE ACTIONS PASS");
    return 0;
}
