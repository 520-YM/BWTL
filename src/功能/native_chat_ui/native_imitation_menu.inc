typedef struct {
    UINT id;
    const wchar_t *label;
} ImitationMenuEntry;

static HMENU imitation_menu_section(HMENU parent,const wchar_t *label,
    const ImitationMenuEntry *entries,size_t count){
    HMENU section=CreatePopupMenu();
    if(!section)return NULL;
    for(size_t index=0;index<count;index++){
        if(!AppendMenuW(section,MF_STRING,entries[index].id,entries[index].label)){
            DestroyMenu(section);
            return NULL;
        }
    }
    if(!AppendMenuW(parent,MF_POPUP,(UINT_PTR)section,label)){
        DestroyMenu(section);
        return NULL;
    }
    return section;
}

static HMENU native_imitation_menu_create(void){
    static const ImitationMenuEntry rebuild[]={
        {IMITATION_STRICT,L"重建并试改尺寸"},
        {IMITATION_MANUAL,L"参照不明确时手动选择"},
        {IMITATION_NEUTRAL,L"从 STEP／IGES 推断并重建…"}
    };
    static const ImitationMenuEntry review[]={
        {IMITATION_DIAGNOSE,L"诊断当前模型与参照"},
        {IMITATION_COMPARE,L"与另一零件比对…"},
        {IMITATION_COMPARE_FLAT,L"比对钣金展开…"}
    };
    static const ImitationMenuEntry batch[]={
        {IMITATION_BATCH_PREVIEW,L"批量预检模型与参照…"},
        {IMITATION_BATCH,L"批量按原步骤重建…"}
    };
    static const ImitationMenuEntry learning[]={
        {IMITATION_LEARN,L"保存最近的建模方法"},
        {IMITATION_ASSEMBLY_LEARN,L"装配学习"},
        {IMITATION_STUDY,L"装配学习工具"},
        {IMITATION_MEMORY,L"查看学习记忆"}
    };
    static const ImitationMenuEntry methods[]={
        {IMITATION_CSYS_METHOD,L"装配坐标系定位"},
        {IMITATION_SHEET_CUT_METHOD,L"钣金首壁与独立切口"}
    };
    HMENU menu=CreatePopupMenu();
    if(!menu)return NULL;
    if(AppendMenuW(menu,MF_STRING,IMITATION_CURRENT,L"重建当前模型")&&
       AppendMenuW(menu,MF_STRING,IMITATION_FILE,L"选择模型文件并重建…")&&
       AppendMenuW(menu,MF_SEPARATOR,0,NULL)&&
       imitation_menu_section(menu,L"重建与试改",rebuild,_countof(rebuild))&&
       imitation_menu_section(menu,L"检查与比对",review,_countof(review))&&
       imitation_menu_section(menu,L"批量处理",batch,_countof(batch))){
        HMENU learn=imitation_menu_section(menu,L"学习与复用",learning,_countof(learning));
        if(learn&&imitation_menu_section(learn,L"已验证的做法",methods,_countof(methods))&&
           AppendMenuW(menu,MF_SEPARATOR,0,NULL)&&AppendMenuW(menu,MF_STRING,IMITATION_COVERAGE,L"已验收范围")&&
           AppendMenuW(menu,MF_STRING,IMITATION_GO_COMPANY,L"公司标准件归档与打开 → 标准文件"))
            return menu;
    }
    DestroyMenu(menu);
    return NULL;
}

static void native_imitation_menu(HWND button){
    HMENU menu=native_imitation_menu_create();
    if(!menu)return;
    UINT action=native_grouped_menu(button,menu,L"仿照模型",NATIVE_GROUPED_IMITATION);
    DestroyMenu(menu);
    if(!action||native_busy)return;
    if(action==IMITATION_GO_COMPANY){
        native_company_action();
        return;
    }
    const wchar_t *draft=NULL;
    if(action==IMITATION_CSYS_METHOD)
        draft=L"用已学的坐标系定位法，把当前总装里的组件〔待填：组件型号或当前实例 ID〕"
            L"按案例〔待填：源装配名，或上次学习的案例〕定位。核对新组件和当前总装的两个坐标系后直接执行。";
    else if(action==IMITATION_SHEET_CUT_METHOD)
        draft=L"按已学的钣金首壁与独立切口建模方法，新建一块矩形钣金板。"
            L"长〔待填：毫米〕，宽〔待填：毫米〕，板厚〔待填：毫米〕，"
            L"圆形贯穿切口直径〔待填：毫米〕，圆心〔待填：草绘坐标 X、Y，或居中〕。直接建模。";
    if(draft){native_prompt_set(draft);SetFocus(native_prompt);}
    else native_imitation_action(action);
}