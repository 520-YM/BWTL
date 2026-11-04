static void part_text_quote(PartText *out,const wchar_t *s){
    part_text_add(out,L"\"");
    while(*s){
        if(*s==L'"'||*s==L'\\')part_text_add(out,L"\\%lc",*s);
        else if(*s<L' ')part_text_add(out,L" ");
        else part_text_add(out,L"%lc",*s);
        s++;
    }part_text_add(out,L"\"");
}
