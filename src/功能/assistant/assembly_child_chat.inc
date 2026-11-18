/* This state is only live between creating a requested assembly and the
   existing active-window verification callback. */
static int assembly_child_pending;

static int assembly_take(const wchar_t **input,const wchar_t *word){
    size_t size=wcslen(word);
    if(wcsncmp(*input,word,size))return 0;
    *input+=size;
    return 1;
}

static int assembly_child_request(const wchar_t *text,int *new_assembly){
    wchar_t compact[256];
    size_t used=0;
    *new_assembly=0;
    for(;*text;text++){
        if(iswspace(*text))continue;
        if(used+1==_countof(compact))return 0;
        compact[used++]=*text;
    }
    while(used&&wcschr(L"。.!！",compact[used-1]))used--;
    compact[used]=0;

    const wchar_t *request=compact;
    size_t split=wcscspn(request,L"，,。；;");
    if(request[split]&&split<80){
        wchar_t first[80];
        wmemcpy(first,request,split);
        first[split]=0;
        if(model_empty_request(first)==3){
            *new_assembly=1;
            request+=split+1;
            if(assembly_take(&request,L"然后")==0)assembly_take(&request,L"再");
        }
    }

    if(assembly_take(&request,L"请")==0)assembly_take(&request,L"想办法");
    if(!assembly_take(&request,L"在"))return 0;
    if(!assembly_take(&request,L"当前")&&!assembly_take(&request,L"这个"))
        assembly_take(&request,L"该");
    if(!assembly_take(&request,L"空装配体")&&!assembly_take(&request,L"空装配")&&
       !assembly_take(&request,L"装配体")&&!assembly_take(&request,L"装配"))return 0;
    if(!assembly_take(&request,L"里面")&&!assembly_take(&request,L"内部")&&
       !assembly_take(&request,L"中")&&!assembly_take(&request,L"里")&&
       !assembly_take(&request,L"内"))return 0;
    if(!assembly_take(&request,L"给我"))assembly_take(&request,L"帮我");
    if(!assembly_take(&request,L"创建")&&!assembly_take(&request,L"新建")&&
       !assembly_take(&request,L"建"))return 0;
    if(!assembly_take(&request,L"一个"))assembly_take(&request,L"1个");
    int kind=0;
    if(assembly_take(&request,L"空钣金零件")||assembly_take(&request,L"钣金空零件"))kind=2;
    else if(assembly_take(&request,L"空实体零件")||assembly_take(&request,L"空零件")||
            assembly_take(&request,L"空子零件"))kind=1;
    if(*request&&wcscmp(request,L"，一定要做到")&&wcscmp(request,L",一定要做到")&&
       wcscmp(request,L"，直接执行")&&wcscmp(request,L",直接执行")&&
       wcscmp(request,L"，不保存")&&wcscmp(request,L",不保存"))return 0;
    return kind;
}

static ProError assembly_child_apply(ProMdl target,int kind,wchar_t *report,size_t capacity){
    return assembly_apply_protocol(target,L"ASM_NEW_PART",L"AUTO",
        kind==2?L"SHEETMETAL":L"SOLID",report,capacity);
}

static int assembly_child_after_create(ProMdl target,const wchar_t *name,int pinned){
    const int kind=assembly_child_pending;
    if(!kind)return 0;
    assembly_child_pending=0;
    wchar_t details[2048]=L"",receipt[2700];
    ProError error=PRO_TK_BAD_CONTEXT;
    if(pinned)error=assembly_child_apply(target,kind,details,_countof(details));
    else wcscpy_s(details,_countof(details),L"当前装配窗口或工作目录的核对未通过。");
    if(error&&!details[0])swprintf_s(details,_countof(details),L"装配内新建子零件未完成（Creo %d）。",error);
    swprintf_s(receipt,_countof(receipt),L"已创建装配 %ls.asm。\n%ls",name,details);
    smart_step(NULL,L"新建装配和空子零件",error,receipt,1,1,!pinned||ax_recovery_uncertain);
    return 1;
}

static int assembly_child_chat(const wchar_t *text){
    int new_assembly=0,kind=assembly_child_request(text,&new_assembly);
    if(!kind||native_busy||smart_active||ai_process)return 0;
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    smart_begin(text);
    smart_local_plan=1;
    smart_readonly=0;
    if(!storage_prepare_output()){
        smart_reply(storage_output_message);
        return 1;
    }
    if(new_assembly){
        assembly_child_pending=kind;
        pending_plan=(ModelPlan){.empty_kind=3,.ready=1};
        execute_model_plan();
        if(!smart_active)assembly_child_pending=0;
        return 1;
    }
    ProMdl target=NULL;
    ProMdlType type;
    if(ProMdlCurrentGet(&target)||ProMdlTypeGet(target,&type)||type!=PRO_MDL_ASSEMBLY){
        smart_reply(L"请先激活要创建子零件的 Creo 装配体。");
        return 1;
    }
    wchar_t details[2048]=L"";
    ProError error=assembly_child_apply(target,kind,details,_countof(details));
    if(error&&!details[0])swprintf_s(details,_countof(details),L"装配内新建子零件未完成（Creo %d）。",error);
    smart_step(NULL,L"装配内新建空子零件",error,details,!error,1,ax_recovery_uncertain);
    return 1;
}