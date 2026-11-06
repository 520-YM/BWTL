#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <assert.h>
#include "../src/part_command.h"
#include "../src/machining_prompt.h"
int main(void){
    const wchar_t *good[]={L"MACHINING_INFO PRT1",L"HOLE PRT1 H1 SURFACE:71 PLANE:RIGHT 20 PLANE:FRONT -30 6 THROUGH 0",L"HOLE PRT1 H1 SURFACE:71 SURFACE:2 0 SURFACE:4 0 6 BLIND 5",L"EDGE_ROUND PRT1 R1 2 EDGE:10",L"EDGE_CHAMFER PRT1 C1 1 EDGE:10 EDGE:11",L"EDGE_ROUND PRT1 R1 0.01 EDGE:1 EDGE:2 EDGE:3 EDGE:4 EDGE:5 EDGE:6 EDGE:7 EDGE:8"};
    const wchar_t *bad[]={L"MACHINING_INFO PRT1 extra",L"HOLE PRT1 H1 PLANE:TOP PLANE:RIGHT 20 PLANE:FRONT 30 6 BLIND 5 extra",L"HOLE PRT1 H1 SURFACE:71 PLANE:RIGHT 20 PLANE:RIGHT 30 6 BLIND 5",L"HOLE PRT1 H1 SURFACE:71 PLANE:RIGHT nan PLANE:FRONT 30 6 BLIND 5",L"HOLE PRT1 H1 SURFACE:71 PLANE:RIGHT 20 PLANE:FRONT 30 -6 BLIND 5",L"HOLE PRT1 H1 SURFACE:71 PLANE:RIGHT 20 PLANE:FRONT 30 6 BLIND 0",L"HOLE PRT1 H1 SURFACE:71 PLANE:RIGHT 20 PLANE:FRONT 30 6 THROUGH 5",L"HOLE PRT1 H1 SURFACE:71 PLANE:RIGHT 20 PLANE:FRONT 30 6 TAPPED 5",L"HOLE PRT1 H1 SURFACE:71 EDGE:5 20 PLANE:FRONT 30 6 BLIND 5",L"EDGE_ROUND PRT1 R1 0 EDGE:1",L"EDGE_ROUND PRT1 R1 inf EDGE:1",L"EDGE_ROUND PRT1 R1 2 EDGE:-1",L"EDGE_ROUND PRT1 R1 2 EDGE:1 EDGE:01",L"EDGE_CHAMFER PRT1 C1 2 EDGE:1 EDGE:2 EDGE:3 EDGE:4 EDGE:5 EDGE:6 EDGE:7 EDGE:8 EDGE:9",L"EDGE_CHAMFER PRT1 C1 1 SURFACE:5",L"EDGE_CHAMFER PRT1 ../C1 1 EDGE:5",L"EDGE_ROUND PRT1 R1 10001 EDGE:1",L"EDGE_ROUND PRT1 R1 2"};
    PartCommand c;wchar_t line[2048];int i;
    for(i=0;i<sizeof(good)/sizeof(good[0]);i++){swprintf_s(line,2048,L"CREO_OP %ls",good[i]);assert(part_command_parse(line,&c)==1&&c.verb==PART_FOUNDATION);}
    for(i=0;i<sizeof(bad)/sizeof(bad[0]);i++){swprintf_s(line,2048,L"CREO_OP %ls",bad[i]);assert(part_command_parse(line,&c)==-1&&c.verb==PART_NONE);}
    assert(part_command_parse(L"CREO_OP EDGE_ROUND PRT1 R1 2 EDGE:1\nCREO_OP SAVE PRT1",&c)==-1);
    assert(part_command_parse(L"CREO_PLAN BOX 20 20 20 0\nCREO_OP EDGE_ROUND PRT1 R1 2 EDGE:1",&c)==-1);
    assert(machining_requires_current(L"在上表面打一个直径6的盲孔"));
    assert(machining_requires_current(L"边倒角1毫米"));
    assert(!machining_requires_current(L"新建一个带中心孔的板"));
    assert(machining_requires_current(L"不要新建，在当前模型开孔"));
    puts("MACHINING PROTOCOL PASS: 6 valid, 18 invalid, 2 mixed action refusals.");return 0;
}
