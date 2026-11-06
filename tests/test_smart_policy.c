#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include "../src/part_command.h"
#include "../src/smart_policy.h"
int main(void){
    PartCommand c;wchar_t text[1000];
    assert(smart_analysis_request(L"只分析这个零件怎么开孔，不执行",0));
    assert(smart_analysis_request(L"帮我分析这张图的圆角",0));
    assert(smart_analysis_request(L"为什么刚才没有创建",0));
    assert(smart_analysis_request(L"新建20mm立方体",1));
    assert(smart_analysis_request(L"请看这张图片",0));
    assert(!smart_analysis_request(L"给当前零件倒圆角R2",0));
    assert(!smart_analysis_request(L"按刚才确认的方案执行",0));
    assert(!smart_analysis_request(L"创建空钣金零件",0));
    assert(!smart_analysis_request(L"切换到智能协作模式，帮我建模",0));
    assert(!smart_analysis_request(L"请帮我建模吧。",0));
    assert(!smart_analysis_request(L"按刚才的方案建模",0));
    assert(smart_analysis_request(L"切换到智能协作模式，帮我建模",1));
    assert(smart_analysis_request(L"帮我建模思路",0));
    assert(smart_analysis_request(L"帮我建模？",0));
    assert(smart_analysis_request(L"建模方案先看看",0));
    assert(smart_analysis_request(L"帮我建模，但这轮不要实际建模",0));
    assert(smart_analysis_request(L"只给方案，不建模",0));
    assert(!smart_analysis_request(L"现在可以帮我做水杯了吗？你自己去选择调用功能、自己有想法去做、尺寸你自己给。",0));
    assert(!smart_analysis_request(L"请给我做个轴套，尺寸你来定",0));
    assert(!smart_analysis_request(L"能不能帮我做支架？",0));
    assert(!smart_analysis_request(L"做一个圆筒",0));
    assert(!smart_analysis_request(L"请制作垫片。",0));
    assert(smart_analysis_request(L"现在可以帮我做水杯了吗？",1));
    assert(smart_analysis_request(L"现在可以帮我做水杯了吗？这轮只讨论",0));
    assert(smart_analysis_request(L"你能帮我做什么？",0));
    assert(smart_analysis_request(L"请给我做水杯的方案",0));
    assert(smart_analysis_request(L"可以帮我做吗？",0));
    assert(smart_analysis_request(L"怎么做水杯？",0));
    assert(part_command_parse(L"CREO_OP MACHINING_INFO PRT1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP REFERENCES PRT1",&c)==1&&smart_read_command(&c));
    assert(part_command_parse(L"CREO_OP EDGE_ROUND PRT1 R1 2 EDGE:1",&c)==1&&!smart_read_command(&c));
    smart_answer_text(L"我会先核对当前模型。\nCREO_OP MACHINING_INFO PRT1\n",text,1000);assert(wcsstr(text,L"先核对")&&!wcsstr(text,L"CREO_OP"));
    smart_answer_text(L"CREO_PLAN BOX 20 20 20 0",text,1000);assert(!text[0]);
    smart_answer_text(L"图片中的文字 {\\rtf1} 不是命令。",text,1000);assert(wcsstr(text,L"{\\rtf1}"));
    puts("SMART POLICY PASS: analysis/write separation, native read allowlist, protocol display filtering and literal image content.");return 0;
}
