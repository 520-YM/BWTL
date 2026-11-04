static void chat_rtf_add(ChatRtf *r,const char *s){
    size_t n=strlen(s);if(r->length+n>=r->capacity){r->failed=1;return;}
    memcpy(r->text+r->length,s,n);r->length+=n;r->text[r->length]=0;
}
