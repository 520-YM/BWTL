enum { PROJECT_NAME_INPUT=1001 };

#pragma pack(push, 2)
typedef struct {
    DLGTEMPLATE dialog;
    WORD menu, window_class;
    wchar_t title[_countof(L"创建新项目")];
    WORD font_size;
    wchar_t font[_countof(L"Microsoft YaHei UI")];
} ProjectDialogTemplate;
#pragma pack(pop)

static HWND project_dialog_control(HWND dialog,DWORD ex,const wchar_t *class_name,
    const wchar_t *text,DWORD style,int id,RECT units){
    if(!MapDialogRect(dialog,&units))return NULL;
    HWND control=CreateWindowExW(ex,class_name,text,WS_CHILD|WS_VISIBLE|style,
        units.left,units.top,units.right-units.left,units.bottom-units.top,
        dialog,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    if(control)SendMessageW(control,WM_SETFONT,SendMessageW(dialog,WM_GETFONT,0,0),TRUE);
    return control;
}

static INT_PTR CALLBACK project_dialog_proc(HWND dialog,UINT message,WPARAM w,LPARAM l){
    if(message==WM_INITDIALOG){
        SetWindowLongPtrW(dialog,DWLP_USER,l);
        HWND label=project_dialog_control(dialog,0,L"STATIC",L"完整文件夹名称（留空使用当前时间）",
            SS_LEFT,0,(RECT){10,10,218,22});
        HWND input=project_dialog_control(dialog,WS_EX_CLIENTEDGE,L"EDIT",L"",
            WS_TABSTOP|ES_AUTOHSCROLL,PROJECT_NAME_INPUT,(RECT){10,25,218,41});
        HWND create=project_dialog_control(dialog,0,L"BUTTON",L"创建",
            WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,(RECT){117,51,165,66});
        HWND cancel=project_dialog_control(dialog,0,L"BUTTON",L"取消",
            WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL,(RECT){170,51,218,66});
        if(!label||!input||!create||!cancel){EndDialog(dialog,-1);return TRUE;}
        SendMessageW(input,EM_SETLIMITTEXT,MAX_PATH-1,0);
        SendMessageW(dialog,DM_SETDEFID,IDOK,0);
        SetFocus(input);
        return FALSE;
    }
    if(message==WM_COMMAND&&LOWORD(w)==IDOK){
        wchar_t *name=(wchar_t *)GetWindowLongPtrW(dialog,DWLP_USER);
        GetDlgItemTextW(dialog,PROJECT_NAME_INPUT,name,MAX_PATH);
        EndDialog(dialog,IDOK);
        return TRUE;
    }
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(w)==IDCANCEL)){
        EndDialog(dialog,IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static wchar_t *project_dialog_get_name(HWND parent){
    __declspec(align(4)) static const ProjectDialogTemplate layout={
        {WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER|DS_SETFONT,0,0,0,0,228,76},
        0,0,L"创建新项目",9,L"Microsoft YaHei UI"
    };
    wchar_t *name=calloc(MAX_PATH,sizeof(*name));
    if(!name)return NULL;
    INT_PTR result=DialogBoxIndirectParamW(GetModuleHandleW(NULL),&layout.dialog,parent,project_dialog_proc,(LPARAM)name);
    if(result==IDOK)return name;
    if(result==-1)MessageBoxW(parent,L"无法打开项目名称输入框。",L"BWTL",MB_OK|MB_ICONWARNING);
    free(name);
    return NULL;
}