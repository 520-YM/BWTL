/* A switch to a verified assembly window is local navigation. */
static int return_assembly_waiting,return_assembly_continue_task;

static int return_assembly_request(const wchar_t *text,ProMdlName name){
    static const wchar_t *const commands[]={
        L"放回到装配体",L"放回到装配",L"放回装配体",L"放回装配",
        L"回到原来的装配",L"回到父装配",L"回到原装配",L"回到装配体",L"回到装配",
        L"返回到父装配",L"返回原装配",L"返回父装配",L"返回装配窗口",L"返回装配体",L"返回装配",
        L"切回父装配",L"切回原装配",L"切回装配窗口",L"切回装配体",L"切回装配"
    };
    const wchar_t *tail=model_request_action_start(text);
    size_t i;
    for(i=0;i<_countof(commands);i++){
        if(assembly_take(&tail,commands[i]))break;
    }
    if(i==_countof(commands))return 0;
    while(*tail&&(iswspace(*tail)||wcschr(L"，,：:",*tail)))tail++;
    size_t length=wcslen(tail);
    while(length&&(iswspace(tail[length-1])||wcschr(L"。！!",tail[length-1])))length--;
    if(length&&tail[length-1]==L'吧'){
        length--;
        while(length&&iswspace(tail[length-1]))length--;
    }
    if(length>=2&&((tail[0]==L'「'&&tail[length-1]==L'」')||
        (tail[0]==L'“'&&tail[length-1]==L'”')||(tail[0]==L'"'&&tail[length-1]==L'"'))){
        tail++;length-=2;
    }
    if(length>=4&&!_wcsnicmp(tail+length-4,L".ASM",4)){
        if(length==4)return 0;
        length-=4;
    }
    if(!length){name[0]=0;return 1;}
    return length<=31&&!wcsncpy_s(name,PRO_NAME_SIZE,tail,length)&&model_assembly_name_valid(name);
}

static void return_assembly_cancel(void){
    if(!return_assembly_waiting)return;
    return_assembly_waiting=return_assembly_continue_task=0;
    ax_return_cancel();
}

static int return_assembly_confirm(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(!return_assembly_waiting)return 0;
    if(!smart_active){return_assembly_cancel();return 0;}
    const int continuing=return_assembly_continue_task;
    const AxReturnLink expected=smart_parent_link;
    return_assembly_waiting=return_assembly_continue_task=0;
    wchar_t receipt[1700]=L"";
    ProError error=ax_return_readback(receipt,_countof(receipt));
    if(!error&&continuing){
        if(!ax_return_link_live(&expected)||!smart_navigation_binding(expected.root,expected.directory)){
            error=PRO_TK_BAD_CONTEXT;
            wcscpy_s(receipt,_countof(receipt),L"Creo 已切换装配窗口，但未核对到本轮原装配和新快照。已保留之前完成的操作。");
        }else{
            smart_parent_link=(AxReturnLink){0};
            wcscat_s(receipt,_countof(receipt),L" 已核对原装配和组件关系，可用当前装配快照继续。");
            trace("Verified and bound task parent assembly window",0);
        }
    }
    smart_step(NULL,L"回到装配",error,receipt[0]?receipt:L"Creo 没有确认目标装配窗口。",0,!continuing,
               continuing&&error&&!smart_binding_valid());
    return 0;
}

static void return_assembly_start_task(void){
    const AxReturnLink *expected=&smart_parent_link;
    if(!smart_active||smart_readonly||!expected->root||smart_target_model!=expected->child||
       !smart_binding_valid()||!ax_return_link_live(expected)){
        resident_tool_result(PRO_TK_BAD_CONTEXT,L"未找到本轮已核对且仍有效的父装配实例。已保留本轮完成的操作。");
        return;
    }
    wchar_t receipt[700]=L"";
    PartText out={receipt,_countof(receipt),0};
    ProMdlName name;
    ProError error=ProMdlMdlnameGet(expected->root,name);
    if(!error)error=ax_return_parent(name,&out);
    if(!error){
        return_assembly_waiting=return_assembly_continue_task=1;
        error=ProMacroLoad(L"~ Command \x0060" L"BWTLReturnAssemblyConfirm\x0060;");
        if(!error)return;
        return_assembly_cancel();
    }
    resident_tool_result(error,receipt[0]?receipt:L"Creo 尚未确认原装配已打开。请核对当前活动窗口。");
}

static int return_assembly_chat(const wchar_t *text){
    ProMdlName requested;
    if(!return_assembly_request(text,requested))return 0;
    if(native_busy||smart_active||ai_process){append(L"请等当前操作结束后再切换装配窗口。");return 1;}
    smart_begin(text);
    smart_local_plan=1;
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    wchar_t receipt[1800]=L"";
    PartText out={receipt,_countof(receipt),0};
    ProError error=ax_return_parent(requested,&out);
    if(!error){
        return_assembly_waiting=1;
        error=ProMacroLoad(L"~ Command \x0060" L"BWTLReturnAssemblyConfirm\x0060;");
        if(!error)return 1;
        return_assembly_cancel();
        wcscpy_s(receipt,_countof(receipt),L"Creo 已收到切换请求，活动装配窗口尚未核实。");
    }
    if(!receipt[0])swprintf_s(receipt,_countof(receipt),L"Creo 未能找到可核对的装配窗口（Creo %d）。",error);
    smart_record_tool(receipt);
    smart_reply(receipt);
    return 1;
}
