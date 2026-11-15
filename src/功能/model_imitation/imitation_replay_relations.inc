static ProError replay_relation_symbol(ImitationReplay *replay,ProModelitem *a,ProModelitem *b,
                                       const wchar_t *token,int length,ProName mapped)
{
    if(length>=PRO_NAME_SIZE)return PRO_TK_UNSUPPORTED;
    ProName symbol;
    wmemcpy(symbol,token,length);
    symbol[length]=0;
    UxDimension dimension;
    ProError error=ux_dimension(replay->source,symbol,&dimension);
    if(!error){
        ProModelitem target;
        error=replay_dimension_item(replay,replay->source,dimension.dim.id,&target);
        int native_name=towlower(symbol[0])==L'd'&&iswdigit(symbol[1]);
        if(!error&&!native_name){
            ProName previous;
            error=ProDimensionSymbolGet(&target,previous);
            if(!error&&_wcsicmp(previous,symbol))error=ProDimensionSymbolSet(&target,symbol);
        }
        if(!error)error=ProDimensionSymbolGet(&target,mapped);
        return error;
    }
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    ProParameter parameter;
    error=ProParameterInit(a,symbol,&parameter);
    if(error)return error;
    ProParamvalue value;
    ProUnititem unit={0};
    error=ProParameterValueWithUnitsGet(&parameter,&value,&unit);
    if(!error&&(unit.name[0]||(value.type!=PRO_PARAM_DOUBLE&&value.type!=PRO_PARAM_INTEGER)))
        error=PRO_TK_UNSUPPORTED;
    if(!error){
        error=ProParameterInit(b,symbol,&parameter);
        if(error==PRO_TK_E_NOT_FOUND)error=ProParameterWithUnitsCreate(b,symbol,&value,NULL,&parameter);
        else if(!error)error=ProParameterValueWithUnitsSet(&parameter,&value,NULL);
    }
    if(!error)wcscpy_s(mapped,PRO_NAME_SIZE,symbol);
    return error;
}

static ProError replay_relation_line(ImitationReplay *replay,ProModelitem *a,ProModelitem *b,
                                     const wchar_t *source,ProLine line)
{
    size_t used=0;
    int assignment=0;
    while(*source){
        if(source[0]==L'/'&&source[1]==L'*'){
            if(wcslen(source)>=PRO_LINE_SIZE-used)return PRO_TK_LINE_TOO_LONG;
            wcscpy_s(line+used,PRO_LINE_SIZE-used,source);
            return PRO_TK_NO_ERROR;
        }
        if(iswalpha(*source)||*source==L'_'){
            const wchar_t *begin=source++;
            while(iswalnum(*source)||*source==L'_')source++;
            ProName mapped;
            ProError error=replay_relation_symbol(replay,a,b,begin,(int)(source-begin),mapped);
            if(error)return error;
            const size_t length=wcslen(mapped);
            if(length>=PRO_LINE_SIZE-used)return PRO_TK_LINE_TOO_LONG;
            wmemcpy(line+used,mapped,length);
            used+=length;
        }else{
            if(*source==L'=')assignment++;
            else if(!iswdigit(*source)&&!iswspace(*source)&&!wcschr(L".+-*/()",*source))return PRO_TK_UNSUPPORTED;
            if(used>=PRO_LINE_SIZE-1)return PRO_TK_LINE_TOO_LONG;
            line[used++]=*source++;
        }
    }
    line[used]=0;
    return assignment==1||!used?PRO_TK_NO_ERROR:PRO_TK_UNSUPPORTED;
}

static ProError replay_relations(ImitationReplay *replay)
{
    ProModelitem a,b;
    ProRelset source,target;
    ProWstring *lines=NULL,*rebuilt=NULL;
    ProLine *storage=NULL;
    int count=0;
    ProError error=ProMdlToModelitem(replay->source,&a);
    if(!error)error=ProModelitemToRelset(&a,&source);
    if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    if(!error)error=ProArrayAlloc(0,sizeof(*lines),8,(ProArray *)&lines);
    if(!error)error=ProRelsetRelationsGet(&source,&lines);
    if(error==PRO_TK_E_NOT_FOUND||error==PRO_TK_EMPTY)error=PRO_TK_NO_ERROR;
    if(!error&&lines)error=ProArraySizeGet((ProArray)lines,&count);
    if(!error&&count>1000)error=PRO_TK_OUT_OF_RANGE;
    if(!error&&count){
        storage=calloc(count,sizeof(*storage));
        if(!storage)error=PRO_TK_OUT_OF_MEMORY;
        if(!error)error=ProArrayAlloc(count,sizeof(*rebuilt),1,(ProArray *)&rebuilt);
        if(!error)error=ProMdlToModelitem(replay->target,&b);
        if(!error){
            error=ProModelitemToRelset(&b,&target);
            if(error==PRO_TK_E_NOT_FOUND)error=ProRelsetCreate(&b,&target);
        }
        ProWstring *defaults=NULL;
        int existing=0;
        if(!error)error=ProArrayAlloc(0,sizeof(*defaults),8,(ProArray *)&defaults);
        if(!error)error=ProRelsetRelationsGet(&target,&defaults);
        if(error==PRO_TK_E_NOT_FOUND||error==PRO_TK_EMPTY)error=PRO_TK_NO_ERROR;
        if(!error&&defaults)error=ProArraySizeGet((ProArray)defaults,&existing);
        int prefix=0;
        if(!error&&existing<=count){
            while(prefix<existing&&!wcscmp(lines[prefix],defaults[prefix]))prefix++;
            if(prefix!=existing)prefix=0;
        }
        for(int i=0;!error&&i<prefix;i++)rebuilt[i]=lines[i];
        if(defaults)ProWstringproarrayFree(defaults);
        if(!error&&prefix==count)goto done;
        for(int i=prefix;!error&&i<count;i++){
            rebuilt[i]=storage[i];
            error=replay_relation_line(replay,&a,&b,lines[i],storage[i]);
            if(error)part_text_add(replay->out,L"零件关系第 %d 行无法按原尺寸或参数绑定，Creo %d。\n",i+1,error);
        }
        ProBool units;
        if(!error)error=ProRelsetIsUnitsSensitive(&source,&units);
        if(!error)error=ProRelsetUnitsSensitiveSet(&target,units);
        if(!error)error=ProRelsetRelationsSet(&target,rebuilt,count);
        if(!error)error=ProRelsetRegenerate(&target);
        if(!error)error=part_regenerate(replay->target);
        if(!error)part_text_add(replay->out,L"已重建 %d 行零件关系并核对再生，沿用 %d 行模板关系。\n",count-prefix,prefix);
    }
done:
    if(rebuilt)ProArrayFree((ProArray *)&rebuilt);
    free(storage);
    if(lines)ProWstringproarrayFree(lines);
    return error;
}
