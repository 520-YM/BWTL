static int model_empty_request(const wchar_t *text){
    static const struct {const wchar_t *label; int kind;} models[]={
        {L"钣金零件",2},{L"钣金件",2},{L"钣金空零件",2},{L"空钣金零件",2},{L"空钣金件",2},
        {L"实体零件",1},{L"空零件",1},{L"实体空零件",1},
        {L"装配",3},{L"空装配",3}
    };
    static const wchar_t *const verbs[]={L"创建",L"新建",L"另建"};
    const wchar_t *part=model_request_action_start(text);
    size_t i;
    for(i=0;i<_countof(verbs);i++){
        size_t length=wcslen(verbs[i]);
        if(!wcsncmp(part,verbs[i],length)){part+=length;break;}
    }
    if(i==_countof(verbs))return 0;
    while(iswspace(*part))part++;
    if(!wcsncmp(part,L"一个",2))part+=2;
    else if(*part==L'个')part++;
    while(iswspace(*part))part++;
    size_t n=wcslen(part);
    while(n&&(iswspace(part[n-1])||wcschr(L"。！.!",part[n-1])))n--;
    for(i=0;i<_countof(models);i++)
        if(n==wcslen(models[i].label)&&!wcsncmp(part,models[i].label,n))return models[i].kind;
    return 0;
}
