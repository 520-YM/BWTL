static int fd_entity_token(const wchar_t *s){
    wchar_t number[32];const wchar_t *p=wcschr(s,L':');int id;
    if(!p)return part_integer(s,&id);
    if(p-s<1||p-s>=32)return 0;wcsncpy_s(number,32,s,(size_t)(p-s));
    return part_integer(number,&id)&&(!wcscmp(p+1,L"START")||!wcscmp(p+1,L"END")||!wcscmp(p+1,L"CENTER")||!wcscmp(p+1,L"WHOLE"));
}
