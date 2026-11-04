static void part_text_add(PartText *out,const wchar_t *fmt,...) {
    size_t n=wcslen(out->text);int result;va_list args;
    if(n+2>=out->size){out->truncated=1;return;}
    va_start(args,fmt);result=_vsnwprintf_s(out->text+n,out->size-n,_TRUNCATE,fmt,args);va_end(args);
    if(result<0)out->truncated=1;
}
