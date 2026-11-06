static int model_request_current(const wchar_t *text){
    int explicit_new=wcsstr(text,L"新建")||wcsstr(text,L"独立零件")||wcsstr(text,L"另建");
    if(wcsstr(text,L"不新建")||wcsstr(text,L"不要新建")||wcsstr(text,L"别新建")||wcsstr(text,L"不另建"))explicit_new=0;
    return !explicit_new && (wcsstr(text,L"当前")||wcsstr(text,L"这个零件")||wcsstr(text,L"这个模型")||wcsstr(text,L"拉伸")||wcsstr(text,L"旋转切除"));
}
