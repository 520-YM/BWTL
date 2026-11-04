static int part_name(const wchar_t *s) {
    size_t i,n=wcslen(s); if(!n || n>31)return 0;
    for(i=0;i<n;i++)if(!((s[i]>=L'a'&&s[i]<=L'z')||(s[i]>=L'A'&&s[i]<=L'Z')||
        (s[i]>=L'0'&&s[i]<=L'9')||s[i]==L'_'||s[i]==L'-'))return 0;
    return 1;
}
