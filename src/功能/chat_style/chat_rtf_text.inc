static void chat_rtf_text(ChatRtf *r,const wchar_t *s,size_t n){
    size_t i;char value[32];
    for(i=0;i<n;i++){
        unsigned c=(unsigned short)s[i];
        if(c=='\\'||c=='{'||c=='}'){value[0]='\\';value[1]=(char)c;value[2]=0;}
        else if(c=='\t')strcpy_s(value,sizeof(value),"\\tab ");
        else if(c>=32&&c<127){value[0]=(char)c;value[1]=0;}
        else {sprintf_s(value,sizeof(value),"\\u%d?",(int)(short)c);}
        chat_rtf_add(r,value);
    }
}
