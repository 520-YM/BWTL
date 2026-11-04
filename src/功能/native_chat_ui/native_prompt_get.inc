static int native_prompt_get(wchar_t **value){
    int length;
    *value=NULL;if(!IsWindow(native_prompt))return 0;
    length=GetWindowTextLengthW(native_prompt);
    *value=(wchar_t *)calloc((size_t)length+1,sizeof(wchar_t));
    if(!*value)return 0;
    GetWindowTextW(native_prompt,*value,length+1);return 1;
}
