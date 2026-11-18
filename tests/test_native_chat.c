#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <assert.h>
#include <windows.h>
static BOOL WINAPI test_track_popup(HMENU menu,UINT flags,int x,int y,HWND owner,LPTPMPARAMS options);
#define TrackPopupMenuEx test_track_popup
#include "../src/native_chat_ui.h"
#undef TrackPopupMenuEx
static int ticks,sends;static UINT workdir_action,markup_action;
static void native_send_action(void){sends++;}
static void native_read_action(void){}
static void native_close_action(void){}
static void native_poll_action(void){ticks++;}
static void native_cancel_action(void){}
static void native_details_action(void){}
static void native_company_action(void){}
static void native_workdir_action(UINT action,wchar_t *name){workdir_action=action;free(name);}
static void native_imitation_action(UINT action){(void)action;}
static void native_markup_post(UINT message,LPARAM argument){(void)argument;markup_action=message;}
static void check_menu_entry(HMENU menu,int position,UINT action,const wchar_t *label){
 wchar_t text[64];
 assert(GetMenuItemID(menu,position)==action);
 assert(GetMenuStringW(menu,position,text,_countof(text),MF_BYPOSITION));
 assert(!wcscmp(text,label));
}
static BOOL WINAPI test_track_popup(HMENU menu,UINT flags,int x,int y,HWND owner,LPTPMPARAMS options){
 assert(GetMenuItemCount(menu)==3);
 check_menu_entry(menu,0,WORKDIR_CREATE,L"创建新项目");
 check_menu_entry(menu,1,WORKDIR_PICK,L"切换工作目录…");
 check_menu_entry(menu,2,(UINT)-1,L"当前目录");
 HMENU current=GetSubMenu(menu,2);
 assert(GetMenuItemCount(current)==3);
 check_menu_entry(current,0,WORKDIR_VIEW,L"查看当前工作目录");
 check_menu_entry(current,1,WORKDIR_OPEN,L"打开当前工作目录");
 check_menu_entry(current,2,WORKDIR_COPY,L"复制当前工作目录路径");
 return WORKDIR_PICK;
}
static void check_company_menu(void){
 HMENU menu=native_company_menu_create();
 assert(GetMenuItemCount(menu)==3);
 check_menu_entry(menu,0,IMITATION_OPEN_STANDARD,L"打开已验收标准件…");
 check_menu_entry(menu,1,(UINT)-1,L"资料");
 check_menu_entry(menu,2,(UINT)-1,L"归档");
 HMENU documents=GetSubMenu(menu,1),archive=GetSubMenu(menu,2);
 assert(GetMenuItemCount(documents)==3&&GetMenuItemCount(archive)==2);
 check_menu_entry(documents,0,COMPANY_OPEN_STANDARDS,L"打开公司标准");
 check_menu_entry(documents,1,COMPANY_OPEN_TECHNICAL,L"打开技术文件");
 check_menu_entry(documents,2,COMPANY_OPEN_ROOT,L"打开公司资料目录");
 check_menu_entry(archive,0,IMITATION_LIBRARY,L"将上次结果存为公司标准件");
 check_menu_entry(archive,1,IMITATION_BATCH_LIBRARY,L"批量归档已通过的结果");
 DestroyMenu(menu);
 native_set_busy(1);
 menu=native_company_menu_create();
 assert(menu);
 assert(GetMenuState(menu,0,MF_BYPOSITION)&MF_GRAYED);
 assert(!(GetMenuState(GetSubMenu(menu,1),0,MF_BYPOSITION)&MF_GRAYED));
 assert(GetMenuState(GetSubMenu(menu,2),0,MF_BYPOSITION)&MF_GRAYED);
 DestroyMenu(menu);
 native_set_busy(0);
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
static void check_request_timer(void){
 wchar_t text[80],final[80];
 native_request_timer_format(text,_countof(text),0,59999);assert(!wcscmp(text,L"用时 59.9 秒"));
 native_request_timer_format(text,_countof(text),1,60000);assert(!wcscmp(text,L"运行中 1分00.0秒"));
 native_request_timer_format(text,_countof(text),0,3600100);assert(!wcscmp(text,L"用时 1时00分00.1秒"));
 GetWindowTextW(native_elapsed_label,text,_countof(text));assert(!wcscmp(text,L"等待指令"));
 native_prompt_set(L"计时测试");SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);
 GetWindowTextW(native_elapsed_label,text,_countof(text));assert(wcsstr(text,L"运行中"));
 ULONGLONG started=native_request_started;
 native_set_busy(1);Sleep(350);GetWindowTextW(native_elapsed_label,text,_countof(text));
 assert(native_request_running&&native_request_elapsed>=200&&wcsstr(text,L"运行中"));
 native_set_busy(2);native_request_timer_begin();assert(native_request_started==started);
 native_set_busy(0);GetWindowTextW(native_elapsed_label,final,_countof(final));
 assert(!native_request_running&&wcsstr(final,L"用时"));
 Sleep(180);GetWindowTextW(native_elapsed_label,text,_countof(text));assert(!wcscmp(text,final));
 native_request_timer_begin();assert(native_request_running&&native_request_started>started);
 native_request_timer_finish();native_request_timer_finish();
 for(int dpi=96;dpi<=144;dpi+=48){
  native_dpi=dpi;
  SetWindowPos(native_window,NULL,0,0,MulDiv(640,dpi,96),MulDiv(650,dpi,96),SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
  RECT title,timer,client;GetWindowRect(native_title,&title);GetWindowRect(native_elapsed_label,&timer);GetWindowRect(native_window,&client);
  assert(title.right<timer.left&&timer.right<client.right&&timer.top>=title.top);
 }
 native_dpi=96;sends=0;
 puts("Request clock: send, live ticks, continuation, final freeze, reset, hours and 96/144 DPI PASS");
}
int main(void){
 wchar_t text[12000]=L"",line[64],input[64];int i,first,old;DWORD start;MSG message;
 assert(SetEnvironmentVariableW(L"BWTL_INSTANCE_NUMBER",L"2"));
 native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL); native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL); assert(native_ui_thread); assert(WaitForSingleObject(native_ui_ready,5000)==WAIT_OBJECT_0); CloseHandle(native_ui_ready);native_ui_ready=NULL; assert(native_ui_thread_id!=GetCurrentThreadId()); assert(IsWindowUnicode(native_prompt));assert(!IsWindowVisible(native_window));
 GetWindowTextW(native_window,input,64);assert(!wcscmp(input,L"BWTL · 会话 2"));
 GetWindowTextW(native_title,input,64);assert(!wcscmp(input,L"BWTL · 会话 2"));
 puts("The BWTL title and header identify the session: PASS");
 check_request_timer();
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
  check_card_color(L"用户内容",chat_card_fill[0]);
  check_card_color(L"助手内容",chat_card_fill[1]);
  check_card_color(L"执行内容",chat_card_fill[2]);
  check_card_color(L"模型内容",chat_card_fill[3]);
  check_card_color(L"提示内容",chat_card_fill[4]);
  SetWindowPos(native_window,NULL,0,0,480,650,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
  GetWindowTextW(native_history,rendered,2048);assert(wcsstr(rendered,L"助手内容"));
  check_card_color(L"模型内容",chat_card_fill[3]);assert(!IsWindowVisible(native_window));
  puts("Role cards: five actual background colors, escaped RTF text, Chinese/emoji and hidden resize PASS");
 }
 assert(native_schedule_poll());start=GetTickCount();while(GetTickCount()-start<1500 && !ticks){while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(5);}assert(ticks==1);
 puts("Background AI polling timer dispatch: PASS");
 assert(!FindWindowExW(native_window,NULL,L"ComboBox",NULL));
 assert(GetDlgItem(native_window,CHAT_ATTACH)==native_attach_button&&GetDlgItem(native_window,CHAT_CLEAR_IMAGES)==native_clear_button);
 assert(!GetDlgItem(native_window,CHAT_DETAILS));
 assert(GetDlgItem(native_window,CHAT_WORKDIR)==native_top_buttons[2]);
 GetWindowTextW(native_top_buttons[2],input,64);assert(!wcscmp(input,L"工作目录"));
 assert(IsWindowEnabled(native_top_buttons[2])&&IsWindowEnabled(native_top_buttons[3])&&IsWindowEnabled(native_top_buttons[4]));
 SendMessageW(native_top_buttons[2],BM_CLICK,0,0);assert(workdir_action==WORKDIR_PICK);
 workdir_action=0;native_set_busy(1);SendMessageW(native_window,WM_COMMAND,CHAT_WORKDIR,0);assert(!workdir_action);native_set_busy(0);
 puts("Working directory: create, switch and current-directory tools, plus busy protection PASS");
 check_company_menu();
 puts("Company standards: accepted part shortcut, reference directories and archive tools PASS");
 SendMessageW(native_top_buttons[4],BM_CLICK,0,0);assert(markup_action==MARKUP_OPEN);
 markup_action=0;native_set_busy(1);SendMessageW(native_window,WM_COMMAND,CHAT_RESERVED_3,0);assert(!markup_action);native_set_busy(0);
 puts("Screenshot markup: starts directly and respects the busy state PASS");
 assert(!GetDlgItem(native_window,4110)); /* former upgrade button */
 for(i=0;i<_countof(native_features);i++){
  assert(wcscmp(native_features[i].label,L"应用已验升级"));
  assert(wcscmp(native_features[i].label,L"开发或调整插件"));
 }
 for(i=640;i<=980;i+=340){
  RECT features,company,read,send,attach,clear;
  SetWindowPos(native_window,NULL,0,0,i,650,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
  GetWindowRect(native_top_buttons[0],&features);GetWindowRect(native_top_buttons[1],&company);
  GetWindowRect(native_buttons[0],&read);GetWindowRect(native_buttons[1],&send);
  assert(features.left==read.left&&features.right<company.left);
  GetWindowRect(native_attach_button,&attach);GetWindowRect(native_clear_button,&clear);
  assert(read.top==send.top&&read.right<attach.left&&attach.right<clear.left&&clear.right<send.left);
 }
 puts("Unified chat: no mode selector, development entry or upgrade button; layout PASS at minimum and default widths");
 assert(!(GetWindowLongPtrW(native_preview,GWL_STYLE)&WS_VISIBLE));
 {RECT pane,edit;GetWindowRect(native_history,&pane);GetWindowRect(native_prompt,&edit);assert(edit.top-pane.bottom==MulDiv(10,native_dpi,96));}
 native_set_busy(1);SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);assert(sends==0&&!IsWindowEnabled(native_buttons[1]));
 GetWindowTextW(native_buttons[1],input,64);assert(!wcscmp(input,L"处理中…"));
 native_set_busy(0);SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);assert(sends==1&&IsWindowEnabled(native_buttons[1]));
 GetWindowTextW(native_buttons[1],input,64);assert(!wcscmp(input,L"发送给 AI"));
 native_set_busy(1);assert(IsWindowEnabled(native_stop_button));native_set_busy(0);assert(!IsWindowEnabled(native_stop_button));
 assert(native_ui_history(L"【AI】\n**已经完成**，参照 `EDGE:12`。\n- 第一项\n"));
 {wchar_t shown[512];FINDTEXTEXW find={{0,-1},L"已经完成",{0,0}};CHARFORMAT2W format={0};GetWindowTextW(native_history,shown,512);assert(wcsstr(shown,L"已经完成")&&!wcsstr(shown,L"**")&&!wcsstr(shown,L"`"));assert(SendMessageW(native_history,EM_FINDTEXTEXW,FR_DOWN,(LPARAM)&find)>=0);SendMessageW(native_history,EM_EXSETSEL,0,(LPARAM)&find.chrgText);format.cbSize=sizeof(format);SendMessageW(native_history,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);assert(format.dwEffects&CFE_BOLD);}
 puts("Smart UI: busy/double-send guard, stop, and literal-safe bold/code/list rendering PASS");
 native_ui_stop_thread();
 puts("HIDDEN NATIVE CONTROL TESTS PASSED; no screenshots or desktop input");return 0;
}
