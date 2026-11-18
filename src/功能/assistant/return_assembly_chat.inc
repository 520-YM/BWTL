/* A switch to a verified assembly window is local navigation. */
static int return_assembly_waiting;

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
    return_assembly_waiting=0;
    ax_return_cancel();
}

static int return_assembly_confirm(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(!return_assembly_waiting)return 0;
    if(!smart_active){return_assembly_cancel();return 0;}
    return_assembly_waiting=0;
    wchar_t receipt[1700]=L"";
    ProError error=ax_return_readback(receipt,_countof(receipt));
    smart_step(NULL,L"回到装配",error,receipt[0]?receipt:L"Creo 没有确认目标装配窗口。",0,1,0);
    return 0;
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
