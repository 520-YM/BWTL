static void read_model(char *dialog, char *component, ProAppData data) {
    static PartSnapshot snapshot={0};
    static wchar_t details[PART_TEXT_MAX];
    ProMdl model;
    ProMdlName name;
    wchar_t message[512];
    ProError error = part_target_get(&model);
    trace("ProMdlCurrentGet", error);
    if (error != PRO_TK_NO_ERROR) { append(L"当前没有活动模型，或模型接口不可用。"); return; }
    error = ProMdlMdlnameGet(model, name);
    trace("ProMdlMdlnameGet", error);
    if (error != PRO_TK_NO_ERROR) { append(L"读取模型名称失败。"); return; }
    error=data?part_snapshot_take(model,&snapshot):part_snapshot_take_basic(model,&snapshot);
    if(!error){
        if(data)part_snapshot_format_selected(&snapshot,details,PART_TEXT_MAX,(unsigned)(UINT_PTR)data);
        else part_snapshot_format_basic(&snapshot,details,PART_TEXT_MAX);
        append_card(L"模型信息",details);
    }
    else {swprintf_s(message,512,L"当前模型：%ls；零件详细读取失败（错误码%d）；请检查是否有活动的零件或装配。",name,error);append_card(L"执行结果",message);}
    part_snapshot_clear(&snapshot);
}
