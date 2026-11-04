static int part_integer(const wchar_t *s, int *v) {
    wchar_t *end; long n; errno=0; n=wcstol(s,&end,10);
    if(!*s || end==s || *end || errno || n<0 || n>2147483647L)return 0;
    *v=(int)n;return 1;
}
