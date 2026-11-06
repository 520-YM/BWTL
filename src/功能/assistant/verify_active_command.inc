static int verify_active_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active;ProMdlName name;wchar_t message[256];
    ProError error;
    if(!smart_active)return 0;
    error=ProMdlCurrentGet(&active);trace("Verify active model",error);
    if(!error)error=ProMdlMdlnameGet(active,name);
    if(!error && !_wcsicmp(name,expected_active_model)){
        smart_target_model=active;
        swprintf_s(message,256,L"当前活动模型：%ls。",name);smart_after_tool(0,smart_new_model_result[0]?smart_new_model_result:message,1);
    }else{
        trace("Active model mismatch",error?error:-1);
        if(smart_new_model_result[0])smart_record_tool(smart_new_model_result);
        smart_reply(L"新模型已创建，但活动窗口检查未通过。请查看执行明细。");
    }
    return 0;
}
