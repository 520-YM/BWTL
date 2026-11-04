static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){
    wchar_t path[MAX_PATH];if(!runtime_data[0])return NULL;
    if(swprintf_s(path,MAX_PATH,L"%ls\\%ls",runtime_data,name)<0)return NULL;return _wfopen(path,mode);
}
