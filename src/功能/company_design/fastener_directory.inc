/* Share the primary company library read-only across managed BWTL profiles. */
static int company_fastener_primary_root(wchar_t root[MAX_PATH]){
    wchar_t parent[MAX_PATH];
    if(wcscpy_s(parent,_countof(parent),runtime_data))return 0;
    company_trim_path(parent);
    wchar_t *leaf=wcsrchr(parent,L'\\');
    if(!leaf||_wcsicmp(leaf+1,L"data"))return 0;
    *leaf=0;
    leaf=wcsrchr(parent,L'\\');
    if(!leaf||wcslen(leaf+1)!=1||leaf[1]<L'2'||leaf[1]>L'8')return 0;
    *leaf=0;
    leaf=wcsrchr(parent,L'\\');
    if(!leaf||_wcsicmp(leaf+1,L"BWTL-Instances"))return 0;
    *leaf=0;
    return swprintf_s(root,MAX_PATH,L"%ls\\BWTL\\公司资料",parent)>=0;
}

static int company_fastener_in_root(const wchar_t *root,const wchar_t *folder,wchar_t path[MAX_PATH]){
    wchar_t expected[MAX_PATH];
    if(swprintf_s(expected,_countof(expected),L"%ls\\公司标准\\紧固件\\%ls",root,folder)<0)return 0;
    HANDLE directory=company_open_path(expected,1,path);
    if(directory==INVALID_HANDLE_VALUE)return 0;
    CloseHandle(directory);
    return 1;
}

static int company_fastener_resolve(UINT action,wchar_t path[MAX_PATH]){
    if(action<COMPANY_OPEN_FASTENER_FIRST||
       action-COMPANY_OPEN_FASTENER_FIRST>=_countof(native_company_fasteners))return 0;
    const wchar_t *folder=native_company_fasteners[action-COMPANY_OPEN_FASTENER_FIRST].folder;
    wchar_t root[MAX_PATH];
    if(company_default_root(root)&&company_fastener_in_root(root,folder,path))return 1;
    return company_fastener_primary_root(root)&&company_fastener_in_root(root,folder,path);
}
