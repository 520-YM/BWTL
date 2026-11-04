static int fd_name(const wchar_t *s){
    size_t i,n=wcslen(s);if(!n||n>31)return 0;
    for(i=0;i<n;i++)if(s[i]<=32||wcschr(L"\\/:*?\"<>|;[]{}",s[i]))return 0;
    return 1;
}
