static int part_param_equal(const ProParamvalue *a,const ProParamvalue *b){
    if(a->type!=b->type)return 0;
    switch(a->type){case PRO_PARAM_STRING:return !wcscmp(a->value.s_val,b->value.s_val);
    case PRO_PARAM_DOUBLE:return part_close(a->value.d_val,b->value.d_val);
    case PRO_PARAM_INTEGER:return a->value.i_val==b->value.i_val;
    case PRO_PARAM_BOOLEAN:return a->value.l_val==b->value.l_val;default:return 0;}
}
