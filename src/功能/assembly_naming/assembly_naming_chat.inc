enum {ASSEMBLY_NAMING_NONE,ASSEMBLY_NAMING_CREATE,ASSEMBLY_NAMING_RENAME,ASSEMBLY_NAMING_INVALID};

static int assembly_naming_target(const wchar_t *text,ProMdlName name){
    while(wcschr(L"，,：:、",*text)&&*text)text++;
    if(!wcsncmp(text,L"图号",2)){
        text+=2;
        while(wcschr(L"：:为",*text)&&*text)text++;
    }
    size_t length=wcslen(text);
    while(length&&wcschr(L"。！!",text[length-1]))--length;
    if(length>=4&&!_wcsnicmp(text+length-4,L".asm",4))length-=4;
    if(!length||length>31)return 0;
    if(length==2&&text[0]>=L'0'&&text[0]<=L'9'&&text[1]>=L'0'&&text[1]<=L'9'){
        SYSTEMTIME local;
        GetLocalTime(&local);
        return swprintf_s(name,PRO_NAME_SIZE,L"M%02uA%lc%lc00000V00",local.wYear%100,text[0],text[1])>0;
    }
    wmemcpy(name,text,length);
    name[length]=0;
    return model_assembly_name_valid(name);
}

static int assembly_naming_request(const wchar_t *text,ProMdlName name){
    static const wchar_t *const create[]={L"创建总装",L"新建总装",L"创建空装配",L"新建空装配",L"创建装配",L"新建装配",L"建总装"};
    static const wchar_t *const rename[]={L"把当前装配改名为",L"把当前装配重命名为",L"把当前装配改成",L"把当前装配改为",
        L"将当前装配改名为",L"将当前装配重命名为",L"给当前装配改名为",L"给当前装配重命名为",
        L"把装配改名为",L"当前装配改名为",L"重命名当前装配为",L"装配改名为",L"重命名装配为",L"装配改名"};
    wchar_t compact[240];
    size_t n=0;
    name[0]=0;
    for(;*text;text++){
        if(iswspace(*text))continue;
        if(n+1>=_countof(compact))return ASSEMBLY_NAMING_NONE;
        compact[n++]=*text;
    }
    compact[n]=0;
    const wchar_t *start=compact;
    if(!wcsncmp(start,L"请",1))start++;
    if(!wcsncmp(start,L"帮我",2))start+=2;
    for(size_t i=0;i<_countof(rename);i++){
        size_t prefix=wcslen(rename[i]);
        if(!wcsncmp(start,rename[i],prefix)){
            if(wcspbrk(start+prefix,L"？，?；;"))return ASSEMBLY_NAMING_NONE;
            return assembly_naming_target(start+prefix,name)?ASSEMBLY_NAMING_RENAME:ASSEMBLY_NAMING_INVALID;
        }
    }
    for(size_t i=0;i<_countof(create);i++){
        size_t prefix=wcslen(create[i]);
        if(wcsncmp(start,create[i],prefix))continue;
        const wchar_t *target=start+prefix;
        while(*target&&wcschr(L"，,：:",*target))target++;
        if(*target&&!wcspbrk(target,L"？，?；;")&&assembly_naming_target(target,name))
            return ASSEMBLY_NAMING_CREATE;
    }
    const wchar_t *model=start;
    if(!wcsncmp(model,L"创建",2)||!wcsncmp(model,L"新建",2)){
        model+=2;
        if(!wcsncmp(model,L"一个",2))model+=2;
        size_t length=wcslen(model);
        while(length&&wcschr(L"。！!",model[length-1]))--length;
        static const wchar_t *const ending[]={L"总装",L"装配"};
        for(size_t i=0;i<_countof(ending);i++){
            size_t suffix=wcslen(ending[i]);
            if(length<=suffix||wcsncmp(model+length-suffix,ending[i],suffix))continue;
            wchar_t target[96];
            if(length-suffix>=_countof(target))return ASSEMBLY_NAMING_NONE;
            wmemcpy(target,model,length-suffix);
            target[length-suffix]=0;
            if(assembly_naming_target(target,name))return ASSEMBLY_NAMING_CREATE;
        }
    }
    return ASSEMBLY_NAMING_NONE;
}

static int assembly_naming_chat(const wchar_t *text){
    ProMdlName name;
    int action=assembly_naming_request(text,name);
    if(action==ASSEMBLY_NAMING_NONE)return 0;
    if(native_busy||smart_active||ai_process){
        append(L"当前有 Creo 任务正在执行。请在任务结束后再创建或改名。");
        return 1;
    }
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    smart_begin(text);
    smart_local_plan=1;
    smart_readonly=0;
    if(action==ASSEMBLY_NAMING_INVALID){
        smart_reply(L"请给出新的装配名，例如「把当前装配改成 M26A0100000V00」。");
        return 1;
    }
    if(action==ASSEMBLY_NAMING_CREATE){
        pending_plan=(ModelPlan){.empty_kind=3,.ready=1};
        wcscpy_s(pending_plan.assembly_name,_countof(pending_plan.assembly_name),name);
        execute_model_plan();
        return 1;
    }
    ProMdl model=NULL;
    ProMdlType type;
    if(ProMdlCurrentGet(&model)||ProMdlTypeGet(model,&type)||type!=PRO_MDL_ASSEMBLY){
        smart_reply(L"请先激活要改名的 Creo 装配。");
        return 1;
    }
    wchar_t text_out[2048]=L"";
    PartText report={text_out,_countof(text_out),0};
    native_set_busy(3);
    ProError error=assembly_rename_apply(model,name,&report);
    native_set_busy(0);
    if(error&&!text_out[0])
        swprintf_s(text_out,_countof(text_out),L"装配改名未完成（Creo %d）。Creo 无法核对当前装配或其文件。",error);
    smart_step(NULL,L"装配改名",error,text_out,!error,1,0);
    return 1;
}