#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static HWND native_window;
static HFONT native_font;
static int native_dpi=96;
#include "../src/功能/native_chat_ui/native_grouped_menu.inc"

static HWND native_prompt;
static int native_busy;
static void native_feature_list_action(void){}
static void native_cancel_action(void){}
#include "../src/native_feature_catalog.h"

static int scenario, saw, bounds, reached;
static HWND anchor;

enum { FIXTURE_FEATURES=95 };

static LRESULT CALLBACK owner_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_COMMAND&&LOWORD(w)==FIXTURE_FEATURES){
        native_feature_menu(anchor);
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}

static int click(GroupedMenu *menu, int action,UINT command,HMENU target){
    for(int i=0;i<menu->count;i++){
        GroupedMenuHit hit=menu->hits[i];
        if(hit.action!=action||(command&&hit.command!=command)||(target&&hit.target!=target))continue;
        SendMessageW(menu->window,WM_LBUTTONUP,0,MAKELPARAM((hit.rect.left+hit.rect.right)/2,(hit.rect.top+hit.rect.bottom)/2));
        return 1;
    }
    return 0;
}

static void CALLBACK exercise(HWND window,UINT message,UINT_PTR timer,DWORD ticks){
    (void)window;(void)message;(void)ticks;
    KillTimer(NULL,timer);
    HWND popup=FindWindowW(L"BWTLGroupedMenu",NULL);
    if(!popup)return;
    GroupedMenu *menu=(GroupedMenu *)GetWindowLongPtrW(popup,GWLP_USERDATA);
    if(!menu)return;
    RECT size,parent;
    MONITORINFO screen={sizeof(screen)};
    saw++;
    GetWindowRect(popup,&size);
    GetWindowRect(native_window,&parent);
    GetMonitorInfoW(MonitorFromWindow(anchor,MONITOR_DEFAULTTONEAREST),&screen);
    if(size.left>=screen.rcWork.left&&size.top>=screen.rcWork.top&&size.right<=screen.rcWork.right&&
       size.bottom<=screen.rcWork.bottom&&size.right-size.left<=parent.right-parent.left)bounds++;
    switch(scenario){
    case 1: /* Imitation. Use the direct primary action on a 640-pixel owner. */
        reached=click(menu,GROUPED_LEAF,101,NULL);
        break;
    case 2: /* Feature category, then the requested tab, then its exact action. */
        {
            HMENU branch=GetSubMenu(menu->root,1);
            HMENU page=GetSubMenu(branch,1);
            reached=click(menu,GROUPED_RAIL,0,branch)&&click(menu,GROUPED_TAB,0,page)&&
                click(menu,GROUPED_LEAF,204,NULL);
        }
        break;
    case 3: /* Keyboard focus and the outside navigation affordance remain separate commands. */
        SendMessageW(popup,WM_KEYDOWN,VK_TAB,0);
        SendMessageW(popup,WM_KEYDOWN,VK_TAB,0);
        SendMessageW(popup,WM_KEYDOWN,VK_RETURN,0);
        reached=1;
        break;
    case 4: /* Scroll long compact groups. */
        SendMessageW(popup,WM_KEYDOWN,VK_NEXT,0);
        SendMessageW(popup,WM_KEYDOWN,VK_NEXT,0);
        SendMessageW(popup,WM_KEYDOWN,VK_NEXT,0);
        SendMessageW(popup,WM_KEYDOWN,VK_NEXT,0);
        reached=menu->scroll>0&&click(menu,GROUPED_LEAF,430,NULL);
        break;
    case 5:
        SendMessageW(popup,WM_KEYDOWN,VK_ESCAPE,0);
        reached=1;
        break;
    case 6:
        for(size_t index=0;index<_countof(native_features);index++){
            if(wcscmp(native_features[index].label,L"创建零件（同号装配与钣金件）"))continue;
            reached=click(menu,GROUPED_LEAF,(UINT)index+1,NULL);
            break;
        }
        break;
    }
    if(IsWindow(popup))SendMessageW(popup,WM_CLOSE,0,0);
}

static UINT select_menu(HMENU tree,const wchar_t *title,int mode,int which){
    scenario=which;reached=0;
    UINT_PTR timer=SetTimer(NULL,0,80,exercise);
    if(!timer)return 0;
    UINT result=native_grouped_menu(anchor,tree,title,mode);
    KillTimer(NULL,timer);
    return result;
}

static HMENU submenu(HMENU parent,const wchar_t *label){
    HMENU group=CreatePopupMenu();
    if(!group)return NULL;
    if(!AppendMenuW(parent,MF_POPUP,(UINT_PTR)group,label)){
        DestroyMenu(group);
        return NULL;
    }
    return group;
}

static HMENU imitation_fixture(void){
    HMENU root=CreatePopupMenu();
    AppendMenuW(root,MF_STRING,101,L"重建当前模型");
    AppendMenuW(root,MF_STRING,102,L"选择模型文件并重建…");
    const wchar_t *const groups[]={L"重建与试改",L"检查与比对",L"批量处理",L"学习与复用"};
    for(int i=0;i<4;i++){
        HMENU group=submenu(root,groups[i]);
        AppendMenuW(group,MF_STRING,(UINT)(110+i),L"原命令");
    }
    AppendMenuW(root,MF_STRING,118,L"已验收范围");
    AppendMenuW(root,MF_STRING,119,L"公司标准");
    return root;
}

static HMENU features_fixture(void){
    HMENU root=CreatePopupMenu();
    AppendMenuW(root,MF_STRING,201,L"功能列表");
    HMENU parts=submenu(root,L"零件建模");
    HMENU datums=submenu(parts,L"基准");
    AppendMenuW(datums,MF_STRING,202,L"创建基准轴");
    HMENU solids=submenu(parts,L"实体");
    AppendMenuW(solids,MF_STRING,204,L"创建拉伸");
    AppendMenuW(solids,MF_STRING|MF_GRAYED,205,L"灰色命令");
    HMENU assembly=submenu(root,L"装配与定位");
    HMENU locating=submenu(assembly,L"定位");
    AppendMenuW(locating,MF_STRING,203,L"组件定位");
    return root;
}

static HMENU compact_fixture(void){
    HMENU root=CreatePopupMenu();
    AppendMenuW(root,MF_STRING,300,L"创建新项目");
    HMENU group=submenu(root,L"当前目录");
    for(UINT i=400;i<=430;i++){
        wchar_t label[32];
        swprintf_s(label,_countof(label),L"查看目录 %u",i);
        AppendMenuW(group,MF_STRING,i,label);
    }
    return root;
}

static void verify(int condition,const char *label,int *pass){
    printf("%s=%d\n",label,!!condition);
    if(!condition)*pass=0;
}

int wmain(void){
    HDESK desktop=CreateDesktopW(L"BWTLGroupedMenuNativeTest",NULL,NULL,0,GENERIC_ALL,NULL);
    if(!desktop||!SetThreadDesktop(desktop)){
        printf("PRIVATE_DESKTOP=%lu\n",GetLastError());
        return 1;
    }
    WNDCLASSW owner={0};
    owner.hInstance=GetModuleHandleW(NULL);
    owner.lpszClassName=L"BWTLGroupedMenuOwnerFixture";
    owner.lpfnWndProc=owner_proc;
    if(!RegisterClassW(&owner))return 2;
    native_window=CreateWindowExW(0,owner.lpszClassName,L"BWTL fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        20,20,640,620,NULL,NULL,owner.hInstance,NULL);
    anchor=CreateWindowExW(0,L"BUTTON",L"插件功能",WS_VISIBLE|WS_CHILD,20,60,105,30,native_window,(HMENU)(INT_PTR)FIXTURE_FEATURES,owner.hInstance,NULL);
    native_prompt=CreateWindowExW(0,L"EDIT",L"",WS_VISIBLE|WS_CHILD,20,550,360,24,native_window,NULL,owner.hInstance,NULL);
    if(!native_window||!anchor||!native_prompt)return 3;
    native_font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    int pass=1;
    HMENU imitation=imitation_fixture(),features=features_fixture(),compact=compact_fixture();
    verify(select_menu(imitation,L"仿照模型",NATIVE_GROUPED_IMITATION,1)==101&&reached,"IMITATION_PRIMARY", &pass);
    verify(select_menu(features,L"插件功能",NATIVE_GROUPED_FEATURES,2)==204&&reached,"FEATURE_CATEGORY_TAB_DISPATCH", &pass);
    verify(select_menu(features,L"插件功能",NATIVE_GROUPED_FEATURES,3)==201&&reached,"FEATURE_LIST_KEYBOARD", &pass);
    verify(select_menu(compact,L"工作目录",NATIVE_GROUPED_COMPACT,4)==430&&reached,"COMPACT_LONG_LIST_SCROLL", &pass);
    verify(select_menu(imitation,L"仿照模型",NATIVE_GROUPED_IMITATION,5)==0&&reached,"ESCAPE_CANCEL", &pass);
    scenario=6;reached=0;
    UINT_PTR timer=SetTimer(NULL,0,80,exercise);
    if(!timer)return 4;
    SendMessageW(anchor,BM_CLICK,0,0);
    KillTimer(NULL,timer);
    wchar_t draft[160];
    GetWindowTextW(native_prompt,draft,_countof(draft));
    verify(reached&&!wcscmp(draft,L"创建一个零件"),"PRODUCTION_FEATURE_BUTTON_WITHOUT_MODEL", &pass);
    verify(saw==6&&bounds==6&&IsWindowEnabled(native_window),"NARROW_SCREEN_BOUNDS_AND_OWNER_RESTORATION",&pass);
    DestroyMenu(compact);DestroyMenu(features);DestroyMenu(imitation);
    DestroyWindow(native_window);
    if(native_font)DeleteObject(native_font);
    printf("GROUPED_MENU_NATIVE_PASS=%d\n",pass);
    return pass?0:1;
}