/* The feature menu and direct input use the same validated numeric protocol. */
static int model_centered_request(const wchar_t *text,ModelPlan *plan){
    static const wchar_t prefix[]=L"新建原点居中圆柱";
    const size_t length=_countof(prefix)-1;
    wchar_t protocol[256];
    if(wcsncmp(text,prefix,length)|| (text[length]&&!iswspace(text[length])))return 0;
    if(wcslen(text+length)>200||wcspbrk(text,L"\r\n")||
       swprintf_s(protocol,_countof(protocol),L"CREO_PLAN CENTERED_CYLINDER%ls",text+length)<0)return -1;
    return model_plan_parse(protocol,plan)?1:-1;
}
