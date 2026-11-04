#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <assert.h>
#include <windows.h>
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){return _wfopen(name,mode);}
#include "../src/history_context.h"
int main(void){
 FILE *f;wchar_t result[18000];size_t n;int i;
 f=_wfopen(L"feature-guide.md",L"wt, ccs=UTF-8");assert(f);fputws(L"功能索引：支持关联草绘和偏移基准面。",f);fclose(f);
 f=_wfopen(L"chat-history.md",L"wt, ccs=UTF-8");assert(f);
 fputws(L"较早经验：创建基准面时需要明确偏移方向。\nCREO_OP DATUM_PLANE PRT1 DTM1 TOP 25 创建基准面\n",f);
 for(i=0;i<100;i++)fputws(L"普通无关的聊天记录。\n",f);fclose(f);
 f=_wfopen(L"context.txt",L"wt, ccs=UTF-8");assert(f);history_context_write(f,L"创建基准面需要注意什么");fclose(f);
 f=_wfopen(L"context.txt",L"rt, ccs=UTF-8");assert(f);n=fread(result,sizeof(wchar_t),17999,f);result[n]=0;fclose(f);
 assert(wcsstr(result,L"功能索引"));assert(wcsstr(result,L"偏移方向"));assert(!wcsstr(result,L"CREO_OP"));assert(wcsstr(result,L"只执行最后一条用户明确要求"));
 puts("HISTORY: feature guide, older matching history, bounded retrieval and protocol exclusion PASS");f=_wfopen(L"compact.txt",L"wt, ccs=UTF-8");assert(f);history_context_write_mode(f,L"创建基准面需要注意什么",0);fclose(f);f=_wfopen(L"compact.txt",L"rt, ccs=UTF-8");assert(f);n=fread(result,sizeof(wchar_t),17999,f);result[n]=0;fclose(f);assert(!wcsstr(result,L"功能索引"));assert(wcsstr(result,L"偏移方向"));assert(!wcsstr(result,L"CREO_OP"));puts("COMPACT HISTORY: redundant guide omitted, relevant history and safety retained PASS");return 0;
}
