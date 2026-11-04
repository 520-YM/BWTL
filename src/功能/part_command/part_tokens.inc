static int part_tokens(const wchar_t *s,size_t length,PartCommand *out) {
    size_t i=0,j;int quoted;
    while(i<length){
        while(i<length && (s[i]==L' '||s[i]==L'\t'||s[i]==L'\r'))i++;
        if(i==length)break;
        if(out->count>=PART_ARG_MAX)return 0;
        quoted=s[i]==L'"';if(quoted)i++;j=0;
        while(i<length){
            wchar_t c=s[i++];
            if(quoted && c==L'"'){quoted=2;break;}
            if(!quoted && (c==L' '||c==L'\t'||c==L'\r'))break;
            if(c<L' ' || (!quoted && c==L'"'))return 0;
            if(quoted && c==L'\\'){
                if(i==length || (s[i]!=L'"'&&s[i]!=L'\\'))return 0;
                c=s[i++];
            }
            if(j+1>=PART_TOKEN_MAX)return 0;
            out->arg[out->count][j++]=c;
        }
        if(quoted==1 || (quoted==2 && i<length && s[i]!=L' '&&s[i]!=L'\t'&&s[i]!=L'\r'))return 0;
        out->arg[out->count++][j]=0;
    }
    return 1;
}
