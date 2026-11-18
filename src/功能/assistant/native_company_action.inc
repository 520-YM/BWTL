static int native_company_folder(const wchar_t *path){
    DWORD attributes;
    if(CreateDirectoryW(path,NULL))return 1;
    if(GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    attributes=GetFileAttributesW(path);
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY);
}

static void native_company_action(void){
    CompanyFastenerCatalog catalog={0};
    company_fastener_catalog_build(&catalog);
    HMENU menu=native_company_menu_create(&catalog);
    if(!menu){free(catalog.items);return;}
    UINT action=native_grouped_menu(native_top_buttons[1],menu,L"标准文件",NATIVE_GROUPED_STANDARD);
    DestroyMenu(menu);
    if(action>=COMPANY_FASTENER_MODEL_FIRST&&action-COMPANY_FASTENER_MODEL_FIRST<catalog.count){
        if(!native_busy&&native_prompt){
            const CompanyFastenerItem *item=&catalog.items[action-COMPANY_FASTENER_MODEL_FIRST];
            wchar_t *path=_wcsdup(item->path);
            if(path&&!PostMessageW(native_backend_window,BWTL_FASTENER_MENU_REQUEST,0,(LPARAM)path))free(path);
        }
        free(catalog.items);
        return;
    }
    free(catalog.items);
    if(action==COMPANY_RULE_VIEW){
        PostMessageW(native_backend_window,BWTL_MODELING_RULE_VIEW,0,0);
        return;
    }
    if(action==COMPANY_RULE_EDIT||action==COMPANY_RULE_RESTORE){
        if(native_busy||!native_prompt)return;
        wchar_t draft[96];
        if(action==COMPANY_RULE_EDIT){
            ModelingRuleState state;
            int status=modeling_rule_read(&state);
            if(status!=MODELING_RULE_OK){
                MessageBoxW(native_window,modeling_rule_status(status),L"建模规则",MB_OK|MB_ICONWARNING);
                return;
            }
            swprintf_s(draft,_countof(draft),L"修改建模规则：默认版本 %ls",state.current.revision);
        }else wcscpy_s(draft,_countof(draft),L"恢复上一版建模规则");
        if(GetWindowTextLengthW(native_prompt)){
            MessageBoxW(native_window,L"聊天输入框已有草稿。处理草稿后，可在「建模规则」重新选择这项操作。",
                L"建模规则",MB_OK|MB_ICONINFORMATION);
        }else{
            native_prompt_set(draft);
            const size_t end=wcslen(draft);
            SendMessageW(native_prompt,EM_SETSEL,action==COMPANY_RULE_EDIT?end-3:end,end);
            SetFocus(native_prompt);
        }
        return;
    }
    if(action==IMITATION_OPEN_STANDARD||action==IMITATION_LIBRARY||action==IMITATION_BATCH_LIBRARY){
        if(!native_busy)native_imitation_action(action);
        return;
    }
    if(action!=COMPANY_OPEN_STANDARDS&&action!=COMPANY_OPEN_TECHNICAL&&action!=COMPANY_OPEN_ROOT)return;

    wchar_t parent[MAX_PATH],root[MAX_PATH],standards[MAX_PATH],technical[MAX_PATH],*leaf;
    wcscpy_s(parent,MAX_PATH,runtime_data);
    leaf=wcsrchr(parent,L'\\');
    if(leaf&&!_wcsicmp(leaf+1,L"data"))*leaf=0;
    if(swprintf_s(root,MAX_PATH,L"%ls\\公司资料",parent)<0||
        swprintf_s(standards,MAX_PATH,L"%ls\\公司标准",root)<0||
        swprintf_s(technical,MAX_PATH,L"%ls\\技术文件",root)<0||
        !native_company_folder(root)||!native_company_folder(standards)||!native_company_folder(technical)){
        MessageBoxW(native_window,L"无法建立公司资料目录，请检查本机目录权限。",L"BWTL",MB_OK|MB_ICONWARNING);
        return;
    }
    const wchar_t *path=root;
    if(action==COMPANY_OPEN_STANDARDS)path=standards;
    if(action==COMPANY_OPEN_TECHNICAL)path=technical;
    if((INT_PTR)ShellExecuteW(native_window,L"open",path,NULL,NULL,SW_SHOWNORMAL)<=32)
        MessageBoxW(native_window,path,L"请在文件资源管理器中打开此目录",MB_OK|MB_ICONINFORMATION);
}
