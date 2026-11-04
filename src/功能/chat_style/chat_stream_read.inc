static DWORD CALLBACK chat_stream_read(DWORD_PTR cookie,LPBYTE buffer,LONG size,LONG *written){
    ChatStream *s=(ChatStream *)cookie;size_t n=s->length-s->offset;
    if(n>(size_t)size)n=(size_t)size;memcpy(buffer,s->text+s->offset,n);s->offset+=n;*written=(LONG)n;return 0;
}
