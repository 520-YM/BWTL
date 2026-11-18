static int component_open_chat(const wchar_t *text){
    static const struct {const wchar_t *prefix;ProMdlType type;int selected;} phrases[]={
        {L"打开选中的子装配",PRO_MDL_ASSEMBLY,1},
        {L"打开选中的子零件",PRO_MDL_PART,1},
        {L"打开选中的组件",PRO_MDL_UNUSED,1},
        {L"打开子装配",PRO_MDL_ASSEMBLY,0},
        {L"打开子零件",PRO_MDL_PART,0},
        {L"打开装配组件",PRO_MDL_UNUSED,0}
    };
    const wchar_t *cursor=text;
    if(!wcsncmp(cursor,L"请",1))cursor++;
    int phrase=-1;
    for(int i=0;i<_countof(phrases);i++){
        size_t length=wcslen(phrases[i].prefix);
        if(!wcsncmp(cursor,phrases[i].prefix,length)){
            phrase=i;cursor+=length;break;
        }
    }
    if(phrase<0)return 0;
    if(native_busy||smart_active||ai_process){
        append(L"请等待当前任务完成后再打开组件。");return 1;
    }
    while(iswspace(*cursor))cursor++;
    size_t length=wcslen(cursor);
    while(length&&wcschr(L"。！!",cursor[length-1]))length--;
    while(length&&iswspace(cursor[length-1]))length--;
    if(length>=2&&((cursor[0]==L'「'&&cursor[length-1]==L'」')||
                  (cursor[0]==L'“'&&cursor[length-1]==L'”')||
                  (cursor[0]==L'"'&&cursor[length-1]==L'"'))){cursor++;length-=2;}
    ProMdlType requested=phrases[phrase].type;
    const wchar_t *extension=requested==PRO_MDL_PART?L".PRT":requested==PRO_MDL_ASSEMBLY?L".ASM":L"";
    wchar_t selector[PART_TOKEN_MAX];
    int ids[AX_PATH_DEPTH];
    int explicit_id=0,wrong_type=0;
    if(!length){
        if(swprintf_s(selector,_countof(selector),L"SELECTED%ls",extension)<0)return 0;
    }else{
        if(phrases[phrase].selected||length>=_countof(selector)||
           wcsncpy_s(selector,_countof(selector),cursor,length)||!ax_open_selector_valid(selector))return 0;
        explicit_id=ax_path_parse(selector,ids);
        const wchar_t *suffix=wcsrchr(selector,L'.');
        if(!explicit_id&&*extension){
            if(suffix)wrong_type=_wcsicmp(suffix,extension)!=0;
            else if(wcscat_s(selector,_countof(selector),extension))return 0;
        }
    }
    append_card(L"你",text);save_turn(L"用户",text);
    native_prompt_set(L"");smart_begin(text);smart_local_plan=1;
    if(wrong_type){
        smart_reply(L"指令中的模型类型和文件后缀不一致，请核对 .PRT 或 .ASM。");return 1;
    }
    ProMdl assembly=NULL;
    ProMdlType type;
    if(ProMdlCurrentGet(&assembly)||!assembly||ProMdlTypeGet(assembly,&type)||type!=PRO_MDL_ASSEMBLY){
        smart_reply(L"请先在 Creo 中激活包含该组件的装配窗口。");return 1;
    }
    if(explicit_id&&requested!=PRO_MDL_UNUSED){
        AxComponent child={0};
        if(!ax_component_resolve(assembly,selector,&child)&&
           !ProMdlTypeGet(child.model,&type)&&type!=requested){
            smart_reply(L"这个实例路径的实际模型类型与指令不一致。");return 1;
        }
    }
    part_snapshot_clear(&request_snapshot);
    if(part_snapshot_take_mode(assembly,&request_snapshot,0)){
        smart_reply(L"Creo 未能读取当前装配的身份，无法核对要打开的组件。");return 1;
    }
    wchar_t protocol[512];
    if(swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_OPEN %ls %ls",request_snapshot.name,selector)<0||
       part_command_parse(protocol,&pending_part)!=1){
        smart_reply(L"请给出 Creo 树里的组件名称、实例路径，或先选中一个组件。");return 1;
    }
    execute_part_command();
    return 1;
}
