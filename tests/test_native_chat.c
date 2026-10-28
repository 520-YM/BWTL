#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include "../src/native_chat_ui.h"
static int ticks;
static void native_send_action(void){}
static void native_read_action(void){}
static void native_close_action(void){}
static void native_poll_action(void){ticks++;}
static wchar_t normalized_next(const wchar_t **p){wchar_t c=*(*p)++;if(c==L'\r'){if(**p==L'\n')(*p)++;return L'\n';}return c;}
static int normalized_equal(const wchar_t *a,const wchar_t *b){
 while(*a&&*b){if(normalized_next(&a)!=normalized_next(&b))return 0;}
 while(*a==L'\r'||*a==L'\n')a++;while(*b==L'\r'||*b==L'\n')b++;return !*a&&!*b;
}
static void check_card_color(const wchar_t *needle,COLORREF color){
 FINDTEXTEXW find={{0,-1},(LPWSTR)needle,{0,0}};CHARFORMAT2W format={0};
 assert(SendMessageW(native_history,EM_FINDTEXTEXW,FR_DOWN,(LPARAM)&find)>=0);
 SendMessageW(native_history,EM_EXSETSEL,0,(LPARAM)&find.chrgText);
 format.cbSize=sizeof(format);SendMessageW(native_history,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);
 assert((format.dwMask&CFM_BACKCOLOR)&&format.crBackColor==color);
}
int main(void){
 wchar_t text[12000]=L"",line[64],input[64];int i,first,old;DWORD start;MSG message;
 native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL); native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL); assert(native_ui_thread); assert(WaitForSingleObject(native_ui_ready,5000)==WAIT_OBJECT_0); CloseHandle(native_ui_ready);native_ui_ready=NULL; assert(native_ui_thread_id!=GetCurrentThreadId()); assert(IsWindowUnicode(native_prompt));assert(!IsWindowVisible(native_window));
 for(i=0;i<250;i++){swprintf_s(line,64,L"Line %03d: test message\n",i);wcscat_s(text,12000,line);}
 native_prompt_set(L"测试输入保持不变");SendMessageW(native_prompt,EM_SETSEL,2,2);
 assert(native_ui_history(text));first=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);assert(first>150);
 printf("Initial long history follows latest: PASS (first visible line %d)\n",first);
 wcscat_s(text,12000,L"AI answer complete\n");assert(native_ui_history(text));first=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);assert(first>150);
 puts("Append while at bottom stays at bottom: PASS");
 SendMessageW(native_history,WM_VSCROLL,SB_TOP,0);old=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);assert(old==0);
 wcscat_s(text,12000,L"Another update while reviewing\n");assert(native_ui_history(text));first=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);assert(first==old);
 puts("Reviewing older messages preserves reading position: PASS");
 native_force_follow=1;assert(native_ui_history(text));assert(SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0)>150);
 GetWindowTextW(native_prompt,input,64);assert(!wcscmp(input,L"测试输入保持不变"));assert(GetFocus()==NULL);
 puts("Explicit follow resumes latest without changing input or focus: PASS");
 {
  static wchar_t large_history[64000]=L"";
  for(i=0;i<1800;i++)wcscat_s(large_history,64000,L"Large model snapshot / 中文尺寸参数\n");
  native_force_follow=1;assert(native_ui_history(large_history));
  {int n=GetWindowTextLengthW(native_history);wchar_t *actual=calloc((size_t)n+1,sizeof(wchar_t));assert(actual);GetWindowTextW(native_history,actual,n+1);assert(normalized_equal(large_history,actual));free(actual);}
  assert(!IsWindowVisible(native_window));
  puts("Large model snapshot beyond 32K displayed completely in hidden Unicode control: PASS");
 }
 {
  const wchar_t *cards=L"【你】\n用户内容 {\\rtf1} 中文😀\n\n【AI】\n助手内容\n\n【执行结果】\n执行内容\n\n【模型信息】\n模型内容\n\n【提示】\n提示内容\n";
  wchar_t rendered[2048];
  native_force_follow=1;assert(native_ui_history(cards));GetWindowTextW(native_history,rendered,2048);
  assert(wcsstr(rendered,L"用户内容 {\\rtf1} 中文😀"));
  check_card_color(L"用户内容",RGB(232,241,255));
  check_card_color(L"助手内容",RGB(232,246,240));
  check_card_color(L"执行内容",RGB(242,236,255));
  check_card_color(L"模型内容",RGB(255,246,226));
  check_card_color(L"提示内容",RGB(235,240,245));
  SetWindowPos(native_window,NULL,0,0,480,650,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
  GetWindowTextW(native_history,rendered,2048);assert(wcsstr(rendered,L"助手内容"));
  check_card_color(L"模型内容",RGB(255,246,226));assert(!IsWindowVisible(native_window));
  puts("Role cards: five actual background colors, escaped RTF text, Chinese/emoji and hidden resize PASS");
 }
 assert(native_schedule_poll());start=GetTickCount();while(GetTickCount()-start<1500 && !ticks){while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(5);}assert(ticks==1);
 puts("Background AI polling timer dispatch: PASS");
 native_ui_stop_thread();
 puts("HIDDEN NATIVE CONTROL TESTS PASSED; no screenshots or desktop input");return 0;
}
