#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include "../src/part_command.h"
int main(void){
    PartCommand p={0};int n;
    assert(part_command_parse(L"先读取\nCREO_OP READ board_1\n",&p)==1&&p.verb==PART_READ);
    assert(part_command_parse(L"CREO_OP DIMS board_1 40 d1 80 100",&p)==1&&p.verb==PART_DIMS&&p.count==6);
    assert(part_command_parse(L"CREO_OP DIMS board_1 40 d1 80 100 d2 60 70",&p)==1&&p.count==9);
    assert(part_command_parse(L"CREO_OP PARAM board_1 DESCRIPTION STRING \"旧 名称\" \"新\\\"名称\"",&p)==1&&!wcscmp(p.arg[5],L"新\"名称"));
    assert(part_command_parse(L"CREO_OP PARAM board_1 CODE STRING \"\" \"新\"",&p)==1&&!p.arg[4][0]);
    assert(part_command_parse(L"CREO_OP COPY board_1 board_2",&p)==1&&p.verb==PART_COPY);
    assert(part_command_parse(L"CREO_OP RENAME board_1 board_2",&p)==1&&p.verb==PART_RENAME);
    assert(part_command_parse(L"CREO_OP DISPLAY board_1 board_2",&p)==1&&p.verb==PART_DISPLAY);
    assert(part_command_parse(L"CREO_OP EXTRUDE board_1 BOX 20 20 20 0",&p)==1&&p.verb==PART_EXTRUDE);
    assert(part_command_parse(L"CREO_OP EXTRUDE board_1 CYLINDER 20 20 40 10",&p)==1&&p.verb==PART_EXTRUDE);
    assert(part_command_parse(L"CREO_OP EXTRUDE board_1 BOX nan 20 20 0",&p)==-1);
    assert(part_command_parse(L"CREO_OP EXTRUDE board_1 BOX 20 20 0 0",&p)==-1);
    assert(part_command_parse(L"CREO_OP EXTRUDE board_1 BOX 20 20 20 20",&p)==-1);
    assert(part_command_parse(L"CREO_OP EXTRUDE board_1 CYLINDER 20 30 40 0",&p)==-1);
    assert(part_command_parse(L"CREO_OP EXTRUDE board_1 BOX 20 20 20 0\nCREO_PLAN EMPTY_SOLID",&p)==-1);
    assert(part_command_parse(L"CREO_OP SAVE board_1",&p)==1&&p.verb==PART_SAVE);
    assert(part_command_parse(L"CREO_OP STEP board_1 drawing",&p)==1&&p.verb==PART_STEP);
    assert(part_command_parse(L"CREO_OP REGEN board_1",&p)==1&&p.verb==PART_REGEN);
    assert(part_command_parse(L"CREO_OP SUPPRESS board_1 100 44",&p)==1&&p.verb==PART_SUPPRESS);
    assert(part_command_parse(L"CREO_OP RESUME board_1 100 44",&p)==1&&p.verb==PART_RESUME);
    assert(part_command_parse(L"CREO_OP DIMS board_1 40 d1 nan 100",&p)==-1&&p.verb==PART_NONE);
    assert(part_command_parse(L"CREO_OP DIMS board_1 40 d1 80 inf",&p)==-1);
    assert(part_command_parse(L"CREO_OP DIMS board_1 40 d1 80 100 d1 80 120",&p)==-1);
    assert(part_command_parse(L"CREO_OP DIMS board_1 40 d1 80",&p)==-1);
    assert(part_command_parse(L"CREO_OP DIMS board_1 40 d1 80 100 trailing",&p)==-1);
    assert(part_command_parse(L"CREO_OP SAVE board_1\nCREO_OP RENAME board_1 x",&p)==-1);
    assert(part_command_parse(L"CREO_PLAN BOX 10 10 1 0\nCREO_OP SAVE board_1",&p)==-1);
    assert(part_command_parse(L"CREO_OP READ board_1\n CREO_PLAN BOX 10 10 1 0",&p)==-1);
    assert(part_command_parse(L"CREO_PLAN BOX 10 10 1 0\nCREO_PLAN BOX 20 20 1 0",&p)==-1);
    assert(part_command_parse(L"CREO_OP STEP board_1 ../escape",&p)==-1);
    assert(part_command_parse(L"CREO_OP STEP board_1 C:\\escape",&p)==-1);
    assert(part_command_parse(L"CREO_OP COPY board_1 board.prt",&p)==-1);
    assert(part_command_parse(L"CREO_OP READ board_1;run",&p)==-1);
    assert(part_command_parse(L"CREO_OP PARAM board_1 P STRING \"old\" \"new\"unexpected",&p)==-1);
    assert(part_command_parse(L"CREO_OP PARAM board_1 P STRING \"old\" \"new",&p)==-1);
    assert(part_command_parse(L"CREO_OP PARAM board_1 P STRING \"old\" \"bad\\q\"",&p)==-1);
    assert(part_command_parse(L"普通对话，不执行",&p)==0&&p.verb==PART_NONE);
    assert(part_command_parse(L"CREO_PLAN EMPTY_SOLID",&p)==0);
    assert(!part_integer(L"999999999999999999999999999999999999999",&n));
    puts("PART COMMAND: strict parsing, stale protocol reset, UTF-16 strings, paths, malformed numbers and conflicting actions PASS");return 0;
}
