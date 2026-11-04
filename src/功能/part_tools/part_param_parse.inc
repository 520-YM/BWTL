static int part_param_parse(const wchar_t *kind,const wchar_t *value,ProParamvalue *out){
    double number;
    memset(out,0,sizeof(*out));
    if(!wcscmp(kind,L"STRING")){out->type=PRO_PARAM_STRING;return wcscpy_s(out->value.s_val,PRO_LINE_SIZE,value)==0;}
    if(!part_number(value,&number))return 0;
    if(!wcscmp(kind,L"DOUBLE")){out->type=PRO_PARAM_DOUBLE;out->value.d_val=number;return fabs(number)<=1e12;}
    if(!wcscmp(kind,L"INTEGER")){if(number<-2147483647.0||number>2147483647.0||floor(number)!=number)return 0;out->type=PRO_PARAM_INTEGER;out->value.i_val=(int)number;return 1;}
    if(!wcscmp(kind,L"BOOLEAN")&&(number==0||number==1)){out->type=PRO_PARAM_BOOLEAN;out->value.l_val=(short)number;return 1;}return 0;
}
