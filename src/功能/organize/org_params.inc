enum {ORG_PARAM_LIMIT=160};
typedef struct {ProName name;ProParamvalue value;ProUnititem unit;int found;} OrganizeParameter;
typedef struct {OrganizeParameter values[ORG_PARAM_LIMIT];int count;} OrganizeParameters;

static ProError org_parameter_visit(ProParameter *param,ProError status,ProAppData data){
    if(status)return status;
    OrganizeParameters *s=data;
    if(s->count==ORG_PARAM_LIMIT)return PRO_TK_OUT_OF_RANGE;
    OrganizeParameter *p=&s->values[s->count++];
    wcscpy_s(p->name,PRO_NAME_SIZE,param->id);
    return ProParameterValueWithUnitsGet(param,&p->value,&p->unit);
}

static ProError org_parameter_read(ProMdl model,OrganizeParameters *set){
    ProModelitem owner;
    ProError error=ProMdlToModelitem(model,&owner);
    if(!error)error=ProParameterVisit(&owner,NULL,org_parameter_visit,set);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}

static void org_parameter_value(const OrganizeParameter *p,wchar_t *text,size_t capacity){
    const ProParamvalue *v=&p->value;
    switch(v->type){
    case PRO_PARAM_STRING:swprintf_s(text,capacity,L"%ls",v->value.s_val);break;
    case PRO_PARAM_INTEGER:swprintf_s(text,capacity,L"%d",v->value.i_val);break;
    case PRO_PARAM_BOOLEAN:swprintf_s(text,capacity,L"%ls",v->value.l_val?L"是":L"否");break;
    case PRO_PARAM_DOUBLE:swprintf_s(text,capacity,L"%.10g%ls%ls",v->value.d_val,p->unit.name[0]?L" ":L"",p->unit.name);break;
    default:swprintf_s(text,capacity,L"类型 %d",v->type);break;
    }
}

/* 1: equivalent, 0: different, -1: unsupported or unit mismatch. */
static int org_parameter_compare(const OrganizeParameter *a,const OrganizeParameter *b){
    if(a->value.type!=b->value.type)return 0;
    if(a->value.type==PRO_PARAM_DOUBLE){
        if((a->unit.name[0]!=0)!=(b->unit.name[0]!=0))return -1;
        double converted=b->value.value.d_val;
        if(a->unit.name[0]&&_wcsicmp(a->unit.name,b->unit.name)){
            ProUnitConversion conversion;
            if(ProUnitConversionCalculate((ProUnititem *)&b->unit,(ProUnititem *)&a->unit,&conversion))return -1;
            converted=converted*conversion.scale+conversion.offset;
        }
        return part_close(a->value.value.d_val,converted);
    }
    if(a->value.type==PRO_PARAM_STRING||a->value.type==PRO_PARAM_INTEGER||a->value.type==PRO_PARAM_BOOLEAN)
        return part_param_equal(&a->value,&b->value);
    return -1;
}

static ProError org_params(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    ProMdl second=NULL;
    ProError error=ProMdlnameInit((wchar_t *)cmd->arg[2],PRO_MDL_PART,&second);
    if(error||second==view->model)return error?error:PRO_TK_BAD_CONTEXT;
    OrganizeParameters *sets=calloc(2,sizeof(*sets));
    if(!sets)return PRO_TK_OUT_OF_MEMORY;
    OrganizeParameters *a=sets,*b=sets+1;
    error=org_parameter_read(view->model,a);
    if(!error)error=org_parameter_read(second,b);
    if(error){free(sets);return error;}
    int changed=0,added=0,removed=0,same=0,unknown=0,shown=0;
    part_text_add(out,L"模型级参数对照 %ls → %ls\n",cmd->arg[1],cmd->arg[2]);
    for(int i=0;i<a->count;i++){
        OrganizeParameter *first=&a->values[i],*other=NULL;
        for(int j=0;j<b->count;j++)if(!_wcsicmp(first->name,b->values[j].name)){other=&b->values[j];break;}
        int compare=other?org_parameter_compare(first,other):0;
        if(other)other->found=1;
        if(compare==1){same++;continue;}
        if(!other)removed++;
        else if(compare<0)unknown++;
        else changed++;
        if(shown++<60){
            wchar_t before[PRO_LINE_SIZE+32],after[PRO_LINE_SIZE+32];
            org_parameter_value(first,before,_countof(before));
            if(other)org_parameter_value(other,after,_countof(after));else wcscpy_s(after,_countof(after),L"缺少");
            part_text_add(out,L"%ls｜%ls → %ls%ls\n",first->name,before,after,compare<0?L"（待复核类型或单位）":L"");
        }
    }
    for(int j=0;j<b->count;j++)if(!b->values[j].found){
        added++;
        if(shown++<60){
            wchar_t after[PRO_LINE_SIZE+32];
            org_parameter_value(&b->values[j],after,_countof(after));
            part_text_add(out,L"%ls｜缺少 → %ls\n",b->values[j].name,after);
        }
    }
    part_text_add(out,L"相同 %d，修改 %d，新增 %d，缺少 %d，待复核 %d。%ls",same,changed,added,removed,unknown,
        shown>60?L"已列出前 60 条差异。":L"");
    free(sets);
    return PRO_TK_NO_ERROR;
}
