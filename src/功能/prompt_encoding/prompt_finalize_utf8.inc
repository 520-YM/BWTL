static int prompt_finalize_utf8(const wchar_t *path,int *bytes){
    FILE *f=_wfopen(path,L"rb");long size;wchar_t *wide;char *utf8;int n,ok=0;
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)||(size=ftell(f))<2||size>2097152||size%2||fseek(f,0,SEEK_SET)){fclose(f);return 0;}
    wide=(wchar_t *)calloc((size_t)size+2,1);if(!wide){fclose(f);return 0;}
    if(fread(wide,1,(size_t)size,f)!=(size_t)size||wide[0]!=0xfeff){fclose(f);free(wide);return 0;}fclose(f);
    n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide+1,(int)(size/2-1),NULL,0,NULL,NULL);
    if(n<=0){free(wide);return 0;}utf8=(char *)malloc((size_t)n);
    if(!utf8){free(wide);return 0;}
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide+1,(int)(size/2-1),utf8,n,NULL,NULL)==n){
        f=_wfopen(path,L"wb");if(f){ok=fwrite(utf8,1,(size_t)n,f)==(size_t)n;if(fclose(f))ok=0;}
    }
    free(utf8);free(wide);if(ok&&bytes)*bytes=n;return ok;
}
