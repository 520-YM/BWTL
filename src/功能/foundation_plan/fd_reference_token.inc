static int fd_reference_token(const wchar_t *s){
    const wchar_t *colon=wcschr(s,L':');wchar_t type[24];int id;size_t n;
    if(!wcscmp(s,L"SELECTED"))return 1;
    if(!colon)return fd_name(s);n=(size_t)(colon-s);if(!n||n>=24)return 0;
    wcsncpy_s(type,24,s,n);
    if(!wcscmp(type,L"SURFACE")||!wcscmp(type,L"EDGE"))return part_integer(colon+1,&id);
    if(wcscmp(type,L"PLANE")&&wcscmp(type,L"AXIS")&&wcscmp(type,L"POINT")&&wcscmp(type,L"CSYS")&&wcscmp(type,L"CURVE"))return 0;
    return fd_name(colon+1);
}
