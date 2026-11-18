typedef struct {
    ProMdl model;
    ProMdlName name;
} DefaultAssemblySource;

static DefaultAssemblySource default_assembly_pending;
static DefaultAssemblySource default_assembly_context;

static int assembly_default_source(DefaultAssemblySource *source){
    memset(source,0,sizeof(*source));
    ProMdl part=NULL;
    ProMdlType type;
    if(ProMdlCurrentGet(&part)||ProMdlTypeGet(part,&type))return 0;
    if(type==PRO_MDL_ASSEMBLY){
        if(ProMdlActiveGet(&part)||ProMdlTypeGet(part,&type))return 0;
    }
    if(type!=PRO_MDL_PART||ProMdlMdlnameGet(part,source->name))return 0;
    source->model=part;
    return 1;
}

static int assembly_default_source_is_live(const DefaultAssemblySource *source){
    ProMdl actual=NULL;
    return source->model&&!ProMdlnameInit((wchar_t *)source->name,PRO_MDL_PART,&actual)&&actual==source->model;
}

/* A local shortcut for a complete request: new blank assembly and one
   explicitly identified current part with Creo's Default placement. */
static int assembly_default_new_request(const wchar_t *text){
    static const wchar_t *const starts[]={L"把这个",L"把当前",L"将这个",L"将当前"};
    static const wchar_t *const finishes[]={L"默认装配到里面",L"默认装到里面",L"默认装进去",
        L"按默认定位装进去",L"按默认约束装进去",L"按默认约束装配进去",L"按默认定位装入新装配"};
    size_t first=wcsspn(text,L" \t");
    text+=first;
    size_t separator=wcscspn(text,L"，,。；;\n");
    wchar_t opening[80];
    if(!text[separator]||separator>=_countof(opening))return 0;
    wmemcpy(opening,text,separator);opening[separator]=0;
    if(model_empty_request(opening)!=3)return 0;
    const wchar_t *request=text+separator+1;
    while(iswspace(*request))request++;
    size_t n=wcslen(request);
    while(n&&(iswspace(request[n-1])||wcschr(L"。！!.",request[n-1])))n--;
    if(n>120||wcscspn(request,L"？，,?；;\n“”「」")<n)return 0;
    for(size_t i=0;i<_countof(starts);i++){
        size_t prefix=wcslen(starts[i]);
        if(n<=prefix||wcsncmp(request,starts[i],prefix))continue;
        for(size_t j=0;j<_countof(finishes);j++){
            size_t tail=wcslen(finishes[j]);
            if(n>prefix+tail&&!wcsncmp(request+n-tail,finishes[j],tail)){
                wchar_t target[96];
                size_t length=n-prefix-tail;
                if(length>=_countof(target))return 0;
                wmemcpy(target,request+prefix,length);target[length]=0;
                static const wchar_t *const ambiguous[]={L"不",L"别",L"同时",L"和",L"以及",L"还是",L"还是先",L"再"};
                return !smart_has_text(target,ambiguous,_countof(ambiguous));
            }
        }
    }
    return 0;
}

static ProError assembly_apply_protocol(ProMdl assembly,const wchar_t *operation,const wchar_t *part,const wchar_t *mode,wchar_t *report,size_t capacity){
    ProMdlName name;
    ProMdlType type;
    ProError error=ProMdlTypeGet(assembly,&type);
    if(!error&&type!=PRO_MDL_ASSEMBLY)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProMdlMdlnameGet(assembly,name);
    if(error)return error;
    wchar_t protocol[512];
    PartCommand command={0};
    if(swprintf_s(protocol,_countof(protocol),L"CREO_OP %ls %ls %ls %ls",operation,name,part,mode)<0||
        part_command_parse(protocol,&command)!=1)return PRO_TK_BAD_INPUTS;
    PartSnapshot snapshot={0};
    error=part_snapshot_take_mode(assembly,&snapshot,0);
    if(!error)error=part_execute(&command,&snapshot,report,capacity);
    part_snapshot_clear(&snapshot);
    return error;
}

static ProError assembly_default_apply(ProMdl assembly,const wchar_t *part,wchar_t *report,size_t capacity){
    return assembly_apply_protocol(assembly,L"ASM_INSERT",part,L"DEFAULT",report,capacity);
}

static int assembly_default_after_create(ProMdl assembly,const wchar_t *name,int pinned){
    if(!default_assembly_pending.model)return 0;
    DefaultAssemblySource source=default_assembly_pending;
    memset(&default_assembly_pending,0,sizeof(default_assembly_pending));
    wchar_t details[2048]=L"",receipt[2700];
    ProError error=PRO_TK_BAD_CONTEXT;
    if(!pinned)wcscpy_s(details,_countof(details),L"活动装配或工作目录尚未核对，暂未装入零件。");
    else if(!assembly_default_source_is_live(&source))
        swprintf_s(details,_countof(details),L"源零件 %ls 已退出当前 Creo 会话。请重新打开对应零件。",source.name);
    else error=assembly_default_apply(assembly,source.name,details,_countof(details));
    if(error&&!details[0])swprintf_s(details,_countof(details),L"默认装配未完成（Creo %d）。",error);
    swprintf_s(receipt,_countof(receipt),L"已创建装配 %ls.asm，未自动保存。\n%ls",name,details);
    smart_step(NULL,L"新建装配及默认定位",error,receipt,1,1,!pinned||ax_recovery_uncertain);
    return 1;
}

static int assembly_default_chat(const wchar_t *text){
    static const wchar_t prefix[]=L"默认装配 ";
    int named=!wcsncmp(text,prefix,_countof(prefix)-1), create=assembly_default_new_request(text);
    if(!named&&!create)return 0;
    if(native_busy||smart_active||ai_process)return 0;
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    if(!storage_prepare_output()){
        append_card(L"AI",storage_output_message);
        save_turn(L"助手",storage_output_message);
        return 1;
    }
    wchar_t reply[2048]=L"";
    if(create){
        DefaultAssemblySource source;
        if(assembly_default_source(&source)&&assembly_default_source_is_live(&source)){
            smart_begin(text);
            smart_local_plan=1;
            smart_readonly=0;
            default_assembly_pending=source;
            pending_plan=(ModelPlan){.empty_kind=3,.ready=1};
            execute_model_plan();
            if(!smart_active)memset(&default_assembly_pending,0,sizeof(default_assembly_pending));
            return 1;
        }
        wcscpy_s(reply,_countof(reply),L"请先激活要装入的 Creo 零件，我会在切换到新装配前记住它。");
    }else{
        const wchar_t *part=text+_countof(prefix)-1;
        while(iswspace(*part))part++;
        ProMdl model=NULL;
        ProMdlType type;
        if(ProMdlCurrentGet(&model)||ProMdlTypeGet(model,&type)||type!=PRO_MDL_ASSEMBLY)
            wcscpy_s(reply,_countof(reply),L"请先激活要装入零件的 Creo 装配体。");
        else if(!dv_safe_leaf(part))
            wcscpy_s(reply,_countof(reply),L"请输入「默认装配 零件名」，名称填 Creo 零件名，不带 .prt 或路径。");
        else{
            native_set_busy(3);
            ProError error=assembly_default_apply(model,part,reply,_countof(reply));
            if(error&&!reply[0])swprintf_s(reply,_countof(reply),L"默认定位未完成（Creo %d）。",error);
            native_set_busy(0);
        }
    }
    append_card(L"AI",reply);
    save_turn(L"助手",reply);
    return 1;
}