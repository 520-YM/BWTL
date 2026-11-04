static int verify_active_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active;ProMdlName name;wchar_t message[256];
    ProError error=ProMdlCurrentGet(&active);trace("Verify active model",error);
    if(!error)error=ProMdlMdlnameGet(active,name);
    if(!error && !_wcsicmp(name,expected_active_model)){
        swprintf_s(message,256,L"当前活动模型：%ls。",name);append_card(L"执行结果",message);
    }else{trace("Active model mismatch",error?error:-1);append(L"新模型已创建，但活动窗口检查未通过，请查看日志。");}
    return 0;
}
