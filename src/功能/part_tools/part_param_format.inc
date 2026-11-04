static void part_param_format(PartText *out,const ProParamvalue *p){
    switch(p->type){
    case PRO_PARAM_STRING:part_text_quote(out,p->value.s_val);break;
    case PRO_PARAM_DOUBLE:part_text_add(out,L"%.17g",p->value.d_val);break;
    case PRO_PARAM_INTEGER:part_text_add(out,L"%d",p->value.i_val);break;
    case PRO_PARAM_BOOLEAN:part_text_add(out,L"%d",p->value.l_val?1:0);break;
    default:part_text_add(out,L"[不支持的参数类型]");break;
    }
}
