static int chat_line_role(const wchar_t *s,size_t n,size_t *prefix){
    const wchar_t *labels[]={L"【你】",L"【AI】",L"【执行结果】",L"【模型信息】",L"【提示】",
        L"用户：",L"助手：",L"原生执行结果：",L"你：",L"AI：",L"AI:",L"自检结果："};
    const int roles[]={1,2,3,4,5,1,2,3,1,2,2,3};size_t i;
    for(i=0;i<sizeof(roles)/sizeof(roles[0]);i++){
        size_t length=wcslen(labels[i]);if(n>=length&&!wcsncmp(s,labels[i],length)){*prefix=length;return roles[i];}
    }*prefix=0;return 0;
}
