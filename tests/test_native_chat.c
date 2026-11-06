#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <assert.h>
#include <windows.h>
static BOOL WINAPI test_track_popup(HMENU menu,UINT flags,int x,int y,HWND owner,LPTPMPARAMS options);
#define TrackPopupMenuEx test_track_popup
#include "../src/native_chat_ui.h"
#undef TrackPopupMenuEx
static int ticks,sends,applies;static UINT workdir_action;
static void native_send_action(void){sends++;}
static void native_read_action(void){}
static void native_close_action(void){}
static void native_poll_action(void){ticks++;}
static void native_cancel_action(void){}
static void native_apply_action(void){applies++;}
static void native_details_action(void){}
static void native_company_action(void){}
static void native_workdir_action(UINT action){workdir_action=action;}
static BOOL WINAPI test_track_popup(HMENU menu,UINT flags,int x,int y,HWND owner,LPTPMPARAMS options){
 const wchar_t *labels[]={L"查看当前工作目录",L"打开当前工作目录",L"复制工作目录路径",L"切换工作目录…"};
 UINT ids[]={WORKDIR_VIEW,WORKDIR_OPEN,WORKDIR_COPY,WORKDIR_PICK};
 wchar_t text[64];int i;
 assert(GetMenuItemCount(menu)==5);
 for(i=0;i<4;i++){assert(GetMenuStringW(menu,ids[i],text,64,MF_BYCOMMAND));assert(!wcscmp(text,labels[i]));}
 return WORKDIR_PICK;
}
static void native_feature_list_action(void){}
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
 assert(SetEnvironmentVariableW(L"BWTL_INSTANCE_NUMBER",L"2"));
 native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL); native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL); assert(native_ui_thread); assert(WaitForSingleObject(native_ui_ready,5000)==WAIT_OBJECT_0); CloseHandle(native_ui_ready);native_ui_ready=NULL; assert(native_ui_thread_id!=GetCurrentThreadId()); assert(IsWindowUnicode(native_prompt));assert(!IsWindowVisible(native_window));
 GetWindowTextW(native_window,input,64);assert(!wcscmp(input,L"BWTL · 会话 2"));
 GetWindowTextW(native_title,input,64);assert(!wcscmp(input,L"BWTL · 会话 2"));
 puts("The BWTL title and header identify the session: PASS");
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
 assert(SendMessageW(native_mode_control,CB_GETCOUNT,0,0)==3);
 assert(!GetDlgItem(native_window,CHAT_ATTACH)&&!GetDlgItem(native_window,CHAT_CLEAR_IMAGES));
 assert(!GetDlgItem(native_window,CHAT_DETAILS));
 assert(GetDlgItem(native_window,CHAT_WORKDIR)==native_top_buttons[2]);
 GetWindowTextW(native_top_buttons[2],input,64);assert(!wcscmp(input,L"工作目录"));
 assert(IsWindowEnabled(native_top_buttons[2])&&!IsWindowEnabled(native_top_buttons[3])&&!IsWindowEnabled(native_top_buttons[4]));
 SendMessageW(native_top_buttons[2],BM_CLICK,0,0);assert(workdir_action==WORKDIR_PICK);
 workdir_action=0;native_set_busy(1);SendMessageW(native_window,WM_COMMAND,CHAT_WORKDIR,0);assert(!workdir_action);native_set_busy(0);
 puts("Working directory: visible active button, four actions and busy protection PASS");
 assert(GetDlgItem(native_window,CHAT_APPLY)==native_apply_button);
 assert(GetWindowLongPtrW(native_apply_button,GWL_STYLE)&WS_VISIBLE);
 GetWindowTextW(native_apply_button,input,64);assert(!wcscmp(input,L"应用已验升级"));
 for(i=640;i<=980;i+=340){
  RECT read,apply,send;
  SetWindowPos(native_window,NULL,0,0,i,650,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
  GetWindowRect(native_buttons[0],&read);GetWindowRect(native_apply_button,&apply);GetWindowRect(native_buttons[1],&send);
  assert(read.top==apply.top&&apply.top==send.top&&apply.bottom==read.bottom);
  assert(read.right<apply.left&&apply.right<send.left);
 }
 SendMessageW(native_apply_button,BM_CLICK,0,0);assert(applies==1);
 for(i=1;i<=2;i++){
  native_set_busy(i);
  assert(!IsWindowEnabled(native_apply_button)&&!IsWindowEnabled(native_top_buttons[2]));
  SendMessageW(native_window,WM_COMMAND,CHAT_APPLY,0);assert(applies==1);
 }
 native_set_busy(0);assert(IsWindowEnabled(native_apply_button)&&IsWindowEnabled(native_top_buttons[2]));
 SendMessageW(native_apply_button,BM_CLICK,0,0);assert(applies==2);
 puts("Apply upgrade: visible beside Read Model at minimum and default widths; callback and busy guards PASS");
 assert(!(GetWindowLongPtrW(native_preview,GWL_STYLE)&WS_VISIBLE));
 {RECT pane,edit;GetWindowRect(native_history,&pane);GetWindowRect(native_prompt,&edit);assert(edit.top-pane.bottom==10);}
 native_set_busy(1);SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);assert(sends==0&&!IsWindowEnabled(native_buttons[1]));
 GetWindowTextW(native_buttons[1],input,64);assert(!wcscmp(input,L"处理中…"));
 native_set_busy(0);SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);assert(sends==1&&IsWindowEnabled(native_buttons[1]));
 GetWindowTextW(native_buttons[1],input,64);assert(!wcscmp(input,L"发送给 AI"));
 native_set_busy(2);assert(!IsWindowEnabled(native_stop_button));native_set_busy(0);
 assert(native_ui_history(L"【AI】\n**已经完成**，参照 `EDGE:12`。\n- 第一项\n"));
 {wchar_t shown[512];FINDTEXTEXW find={{0,-1},L"已经完成",{0,0}};CHARFORMAT2W format={0};GetWindowTextW(native_history,shown,512);assert(wcsstr(shown,L"已经完成")&&!wcsstr(shown,L"**")&&!wcsstr(shown,L"`"));assert(SendMessageW(native_history,EM_FINDTEXTEXW,FR_DOWN,(LPARAM)&find)>=0);SendMessageW(native_history,EM_EXSETSEL,0,(LPARAM)&find.chrgText);format.cbSize=sizeof(format);SendMessageW(native_history,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);assert(format.dwEffects&CFE_BOLD);}
 puts("Smart UI: three modes, busy/double-send guard, atomic-install stop guard, literal-safe bold/code/list rendering PASS");
 native_ui_stop_thread();
 puts("HIDDEN NATIVE CONTROL TESTS PASSED; no screenshots or desktop input");return 0;
}
