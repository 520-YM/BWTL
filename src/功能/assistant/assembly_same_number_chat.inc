static int assembly_same_number_sheet_request(const wchar_t *text){
    wchar_t compact[160];
    size_t length=0;
    for(;*text;text++){
        if(iswspace(*text))continue;
        if(length+1==_countof(compact))return 0;
        compact[length++]=*text;
    }
    while(length&&wcschr(L"。！!.",compact[length-1]))length--;
    compact[length]=0;
    const wchar_t *p=compact;
    assembly_take(&p,L"请");
    if(!assembly_take(&p,L"在当前装配里面")&&!assembly_take(&p,L"在当前装配中")&&
       !assembly_take(&p,L"在当前装配里")&&!assembly_take(&p,L"在当前装配内")&&
       !assembly_take(&p,L"在装配里面")&&!assembly_take(&p,L"在装配中")&&
       !assembly_take(&p,L"在装配里")&&!assembly_take(&p,L"在装配内")&&
       !assembly_take(&p,L"在里面"))return 0;
    if(!assembly_take(&p,L"新建")&&!assembly_take(&p,L"创建")&&!assembly_take(&p,L"建"))return 0;
    assembly_take(&p,L"一个");
    if(!assembly_take(&p,L"同图号的")&&!assembly_take(&p,L"同图号")&&!assembly_take(&p,L"同号的")&&
       !assembly_take(&p,L"同号")&&!assembly_take(&p,L"相同图号的")&&!assembly_take(&p,L"相同图号"))return 0;
    assembly_take(&p,L"空");
    return !wcscmp(p,L"钣金件")||!wcscmp(p,L"钣金零件")||!wcscmp(p,L"钣金子零件");
}

static int assembly_same_number_sheet_chat(const wchar_t *text){
    if(!assembly_same_number_sheet_request(text))return 0;
    if(native_busy||smart_active||ai_process){append(L"请等待当前任务完成。");return 1;}
    append_card(L"你",text);
    save_turn(L"用户",text);
    native_prompt_set(L"");
    wchar_t reply[2400]=L"";
    ProMdl assembly=NULL;
    ProMdlType type;
    if(ProMdlCurrentGet(&assembly)||ProMdlTypeGet(assembly,&type)||type!=PRO_MDL_ASSEMBLY)
        wcscpy_s(reply,_countof(reply),L"请先激活要创建同号钣金件的 Creo 装配。");
    else if(!storage_prepare_output())
        wcscpy_s(reply,_countof(reply),storage_output_message);
    else{
        native_set_busy(3);
        ProError error=assembly_apply_protocol(assembly,L"ASM_NEW_PART",L"SAME",L"SHEETMETAL",reply,_countof(reply));
        native_set_busy(0);
        if(error&&!reply[0])swprintf_s(reply,_countof(reply),L"同号钣金件未创建。Creo 返回 %d。",error);
    }
    append_card(L"AI",reply);
    save_turn(L"助手",reply);
    return 1;
}