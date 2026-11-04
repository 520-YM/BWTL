#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../src/prompt_encoding.h"
int main(void){FILE *f;int i,n;char *s;f=_wfopen(L"prompt-encoding-test.txt",L"wt, ccs=UTF-16LE");assert(f);for(i=0;i<4000;i++)fputws(L"中文建模：创建空装配，手机支架，emoji\xd83d\xde00\n",f);assert(!fclose(f));assert(prompt_finalize_utf8(L"prompt-encoding-test.txt",&n));f=_wfopen(L"prompt-encoding-test.txt",L"rb");assert(f);s=malloc(n);assert(fread(s,1,n,f)==n);fclose(f);assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,n,NULL,0)>0);free(s);f=_wfopen(L"prompt-invalid-test.txt",L"wb");{wchar_t bad[]={0xfeff,0xd800};fwrite(bad,2,2,f);}fclose(f);assert(!prompt_finalize_utf8(L"prompt-invalid-test.txt",&n));puts("UTF8: 4000 Chinese/emoji lines valid; invalid surrogate rejected PASS");return 0;}