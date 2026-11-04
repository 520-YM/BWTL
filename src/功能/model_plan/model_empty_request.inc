static int model_empty_request(const wchar_t *text){
    const wchar_t *phrases[]={L"创建一个钣金零件",L"创建钣金零件",L"新建一个钣金零件",L"新建钣金零件",L"创建一个钣金空零件",L"新建钣金空零件",
        L"创建一个实体零件",L"新建一个实体零件",L"创建一个空零件",L"新建一个空零件",L"创建实体空零件",
        L"创建一个装配",L"新建一个装配",L"创建一个空装配",L"新建空装配"};
    wchar_t value[80];size_t n=wcslen(text);int i;
    if(n>=80)return 0;wcscpy_s(value,80,text);
    while(n && (iswspace(value[n-1])||value[n-1]==L'。'||value[n-1]==L'！'||value[n-1]==L'.'||value[n-1]==L'!'))value[--n]=0;
    for(i=0;i<15;i++)if(!wcscmp(value,phrases[i]))return i<6?2:i<11?1:3;
    return 0;
}
