static const wchar_t *part_basic_material(const PartSnapshot *s){
    const wchar_t *keys[]={L"PTC_REPORTED_MATERIAL",L"材料",L"PTC_MATERIAL_NAME",L"材质"};int i,j;
    for(i=0;i<4;i++)for(j=0;j<s->parameters;j++){
        const PartParameter *p=&s->parameter[j];
        if(!_wcsicmp(p->name,keys[i])&&p->value.type==PRO_PARAM_STRING&&p->value.value.s_val[0]&&
           _wcsicmp(p->value.value.s_val,L"ASSY")&&_wcsicmp(p->value.value.s_val,L"PTC_SYSTEM_MTRL_PROPS"))return p->value.value.s_val;
    }return L"未指定";
}
