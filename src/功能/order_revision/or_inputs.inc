
static const wchar_t *or_field(const wchar_t *name){
    static const struct {const wchar_t *field,*aliases[4];} list[]={
        {L"外宽",{L"宽度",L"整机宽度",L"外形宽度",NULL}},
        {L"外高",{L"高度",L"整机高度",L"外形高度",NULL}},
        {L"外深",{L"深度",L"整机深度",L"外形深度",NULL}},
        {L"屏尺寸",{L"屏幕尺寸",L"显示屏尺寸",NULL}},
        {L"板厚",{L"材料厚度",L"钣金厚度",NULL}},
        {L"屏型号",{L"屏幕型号",L"显示屏型号",NULL}},
        {L"安装方式",{L"结构型式",L"安装型式",NULL}},
        {L"散热方案",{L"散热方式",L"冷却方式",NULL}},
        {L"开门方向",{L"检修方向",L"门开方向",NULL}},
        {L"交付格式",{L"出图格式",L"交付文件",NULL}}
    };
    for(int i=0;i<_countof(list);i++){
        if(!wcscmp(name,list[i].field))return list[i].field;
        for(int j=0;j<_countof(list[i].aliases)&&list[i].aliases[j];j++)
            if(!wcscmp(name,list[i].aliases[j]))return list[i].field;
    }
    return NULL;
}

static void or_trim(wchar_t *text){
    wchar_t *p=text;
    while(iswspace(*p))p++;
    if(p!=text)memmove(text,p,(wcslen(p)+1)*sizeof(*text));
    size_t n=wcslen(text);
    while(n&&iswspace(text[n-1]))text[--n]=0;
}

static ProError or_import(const wchar_t *order,const wchar_t *source,PartText *out){
    if(!or_safe_id(order))return PRO_TK_BAD_INPUTS;
    FILE *input=ax_learning_open(source);
    if(!input)return PRO_TK_E_NOT_FOUND;
    OrOutput result;
    if(!or_output_begin(&result,order,L"订单规格.tsv")){fclose(input);return PRO_TK_CANT_WRITE;}
    fwprintf(result.file,L"# 字段\t原文值\t来源行\n");
    wchar_t line[1024];
    const wchar_t *keys[12]={0};
    unsigned n=0,count=0,ignored=0;
    int valid=1;
    while(valid&&fgetws(line,_countof(line),input)){
        n++;
        wchar_t *begin=line[0]==0xfeff?line+1:line;
        if(!wcschr(begin,L'\n')&&!feof(input)){valid=0;break;}
        wchar_t *mark=wcspbrk(begin,L"=：:\t");
        if(!mark){ignored++;continue;}
        *mark++=0;
        or_trim(begin);
        or_trim(mark);
        const wchar_t *key=or_field(begin);
        if(!key){ignored++;continue;}
        if(!*mark||wcslen(mark)>=OR_CELL||wcschr(mark,L'\t')){valid=0;break;}
        for(int i=0;i<(int)count;i++)if(!wcscmp(keys[i],key))valid=0;
        if(!valid){
            part_text_add(out,L"第 %u 行重复给出 %ls，先核对采用值。\n",n,key);
            break;
        }
        if((!wcscmp(key,L"外宽")||!wcscmp(key,L"外高")||!wcscmp(key,L"外深")||!wcscmp(key,L"板厚"))){
            double mm=0;
            if(!or_number(mark,&mm)||mm<=0){
                part_text_add(out,L"第 %u 行的 %ls 需要明确毫米值。\n",n,key);
                valid=0;break;
            }
        }
        if(count==_countof(keys)){valid=0;break;}
        keys[count++]=key;
        fwprintf(result.file,L"%ls\t%ls\t输入第 %u 行\n",key,mark,n);
    }
    if(ferror(input))valid=0;
    fclose(input);
    if(!or_output_end(&result,valid&&count))return valid&&count?PRO_TK_CANT_WRITE:PRO_TK_BAD_INPUTS;
    part_text_add(out,L"已提取 %u 个订单字段。其他 %u 行留待人工核对。\n%ls\n",count,ignored,result.final);
    return PRO_TK_NO_ERROR;
}

static ProError or_templates(const wchar_t *order,const wchar_t *catalog,PartText *out){
    wchar_t source[MAX_PATH];
    if(!or_path(order,L"订单规格.tsv",source,0))return PRO_TK_E_NOT_FOUND;
    OrTable input={0},models={0};
    int valid=or_table_load(&input,source,3,out)&&or_table_load(&models,catalog,6,out)&&or_unique_column(&models,0,out);
    const wchar_t *keys[]={L"屏尺寸",L"安装方式",L"散热方案",L"开门方向"};
    const wchar_t *want[4]={0};
    int best=-1,leaders=0,points[OR_ROWS]={0};
    if(valid){
        for(int i=0;i<_countof(keys);i++){
            int p=or_find(&input,0,keys[i]);
            if(p>=0)want[i]=input.rows[p].value[1];
        }
        for(int i=0;i<models.size;i++){
            for(int col=0;col<4;col++)if(want[col]&&!_wcsicmp(want[col],models.rows[i].value[col+1]))points[i]+=col==0?4:2;
            if(points[i]>best){best=points[i];leaders=1;}
            else if(points[i]==best)leaders++;
        }
    }
    if(valid&&best>0){
        part_text_add(out,L"匹配最高 %d 分，共 %d 个候选。屏尺寸 4 分，安装、散热和开门分别 2 分。\n",best,leaders);
        for(int i=0;i<models.size;i++)if(points[i]==best){
            part_text_add(out,L"%ls",models.rows[i].value[0]);
            for(int col=0;col<4;col++)
                if(want[col]&&_wcsicmp(want[col],models.rows[i].value[col+1]))
                    part_text_add(out,L" · %ls %ls → %ls",keys[col],models.rows[i].value[col+1],want[col]);
            part_text_add(out,L"\n");
        }
    }else if(valid)part_text_add(out,L"订单字段与机型表没有合适的候选。请补充屏尺寸、安装、散热或开门信息。\n");
    or_free(&input);or_free(&models);
    return valid&&best>0?PRO_TK_NO_ERROR:valid?PRO_TK_E_NOT_FOUND:PRO_TK_BAD_INPUTS;
}

static ProError or_conflicts(const wchar_t *path,PartText *out){
    OrTable input={0};
    int valid=or_table_load(&input,path,3,out);
    double values[OR_ROWS]={0};
    int conflicts=0;
    for(int i=0;valid&&i<input.size;i++){
        if(!or_number(input.rows[i].value[1],&values[i])){
            part_text_add(out,L"第 %u 行 %ls 的尺寸无法按毫米读取。\n",input.rows[i].line,input.rows[i].value[0]);
            valid=0;
        }
    }
    for(int i=0;valid&&i<input.size;i++)
        for(int j=0;j<i;j++)
            if(!_wcsicmp(input.rows[i].value[0],input.rows[j].value[0])&&fabs(values[i]-values[j])>OR_TOL){
                conflicts++;
                part_text_add(out,L"%ls 冲突，%ls %.9g mm，%ls %.9g mm。\n",input.rows[i].value[0],
                    input.rows[j].value[2],values[j],input.rows[i].value[2],values[i]);
            }
    if(valid)part_text_add(out,L"读取 %d 条尺寸证据，发现 %d 对差异。\n",input.size,conflicts);
    or_free(&input);
    return valid?(conflicts?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR):PRO_TK_BAD_INPUTS;
}
