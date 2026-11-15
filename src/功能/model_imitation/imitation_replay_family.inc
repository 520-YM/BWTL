typedef struct {
    ProFaminstance row;
    ProMdl source,target;
    PartText *out;
    int dimensions,parameters,features;
} ReplayFamily;

static ProError replay_family_item(ProFamtableItem *column,ProError status,ProAppData data)
{
    if(status)return status;
    ReplayFamily *family=data;
    const wchar_t *kind;
    ProParamvalue effective;
    if(column->type==PRO_FAM_DIMENSION){family->dimensions++;kind=L"尺寸";}
    else if(column->type==PRO_FAM_FEATURE){family->features++;kind=L"特征开关";}
    else if(column->type==PRO_FAM_USER_PARAM){family->parameters++;kind=L"参数";}
    else return family->target?PRO_TK_UNSUPPORTED:PRO_TK_NO_ERROR;
    ProError error=ProFaminstanceValueGet(&family->row,column,&effective);
    if(error)return error;
    if(family->out&&family->dimensions+family->features+family->parameters<=24){
        part_text_add(family->out,L"  族表%ls %ls",kind,column->string);
        if(effective.type==PRO_PARAM_DOUBLE)part_text_add(family->out,L" = %.10g",effective.value.d_val);
        else if(effective.type==PRO_PARAM_INTEGER)part_text_add(family->out,L" = %d",effective.value.i_val);
        else if(effective.type==PRO_PARAM_STRING)part_text_add(family->out,L" = %ls",effective.value.s_val);
        else if(effective.type==PRO_PARAM_BOOLEAN)part_text_add(family->out,L" = %ls",effective.value.l_val?L"启用":L"停用");
        part_text_add(family->out,L"。\n");
    }
    if(column->type!=PRO_FAM_USER_PARAM)return PRO_TK_NO_ERROR;
    ProParameter input,output;
    ProModelitem source,target;
    ProParamvalue value;
    ProUnititem unit={0};
    error=ProMdlToModelitem(family->source,&source);
    if(!error)error=ProParameterInit(&source,column->string,&input);
    if(!error)error=ProParameterValueWithUnitsGet(&input,&value,&unit);
    if(!error&&unit.name[0]&&family->target)error=PRO_TK_UNSUPPORTED;
    if(!error&&family->target){
        error=ProMdlToModelitem(family->target,&target);
        if(!error){
            error=ProParameterInit(&target,column->string,&output);
            if(error==PRO_TK_E_NOT_FOUND)error=ProParameterWithUnitsCreate(&target,column->string,&value,NULL,&output);
            else if(!error)error=ProParameterValueWithUnitsSet(&output,&value,NULL);
        }
    }
    return error;
}

static ProError replay_family_read(ProMdl source,ProMdl target,PartText *out)
{
    ProMdl generic=NULL;
    ProError error=ProFaminstanceGenericGet(source,1,&generic);
    if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    if(error)return error;
    ProMdlName name,base;
    ProFamtable table;
    ReplayFamily family={.source=source,.target=target,.out=out};
    error=ProMdlMdlnameGet(source,name);
    if(!error)error=ProMdlMdlnameGet(generic,base);
    if(!error)error=ProFamtableInit(generic,&table);
    if(!error)error=ProFaminstanceInit(name,&table,&family.row);
    if(!error)error=ProFamtableItemVisit(&table,replay_family_item,NULL,&family);
    if(!error&&out)part_text_add(out,L"族表实例 %ls，母件 %ls，%ls（尺寸 %d、参数 %d、特征开关 %d）。\n",
        name,base,target?L"按实例有效值重建":L"已读取实例有效值",family.dimensions,family.parameters,family.features);
    return error;
}
