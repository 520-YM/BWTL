#include "../workspace_projects/project_dialog.inc"
static HMENU native_workdir_menu_create(void){
    HMENU menu=CreatePopupMenu(),current=CreatePopupMenu();
    if(!menu||!current||
       !AppendMenuW(menu,MF_STRING,WORKDIR_CREATE,L"创建新项目")||
       !AppendMenuW(menu,MF_STRING,WORKDIR_PICK,L"切换工作目录…")||
       !AppendMenuW(current,MF_STRING,WORKDIR_VIEW,L"查看当前工作目录")||
       !AppendMenuW(current,MF_STRING,WORKDIR_OPEN,L"打开当前工作目录")||
       !AppendMenuW(current,MF_STRING,WORKDIR_COPY,L"复制当前工作目录路径")||
       !AppendMenuW(menu,MF_POPUP,(UINT_PTR)current,L"当前目录")){
        if(current)DestroyMenu(current);
        if(menu)DestroyMenu(menu);
        return NULL;
    }
    return menu;
}

static void native_workdir_menu(HWND button){
    HMENU menu=native_workdir_menu_create();
    if(!menu)return;
    UINT action=native_grouped_menu(button,menu,L"工作目录",NATIVE_GROUPED_COMPACT);
    DestroyMenu(menu);
    if(!action||native_busy)return;
    if(action==WORKDIR_CREATE){
        wchar_t *name=project_dialog_get_name(native_window);
        if(name)native_workdir_action(action,name);
    }else native_workdir_action(action,NULL);
}
