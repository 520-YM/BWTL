
typedef struct { unsigned from,until; } StdRuleDates;

static unsigned std_date(const wchar_t *value){
    if(wcslen(value)!=10||value[4]!=L'-'||value[7]!=L'-')return 0;
    unsigned part[3]={0};
    for(int i=0;i<10;i++){
        if(i==4||i==7)continue;
        if(value[i]<L'0'||value[i]>L'9')return 0;
        int j=(i>4)+(i>7);
        part[j]=part[j]*10+value[i]-L'0';
    }
    SYSTEMTIME time={0};
    time.wYear=(WORD)part[0];time.wMonth=(WORD)part[1];time.wDay=(WORD)part[2];
    FILETIME checked;
    return part[0]>=1900&&SystemTimeToFileTime(&time,&checked)?part[0]*10000+part[1]*100+part[2]:0;
}

static int std_rule_path(wchar_t path[MAX_PATH],int create){
    wchar_t folder[MAX_PATH];
    return std_dir(folder,create)&&swprintf_s(path,MAX_PATH,L"%ls\\工艺规则.tsv",folder)>=0;
}

static int std_rule_valid(const OrRow *r,StdRuleDates *date){
    const wchar_t (*v)[OR_CELL]=r->value;
    date->from=std_date(v[4]);
    date->until=!wcscmp(v[5],L"-")?20991231:std_date(v[5]);
    double number;
    return (or_safe_id(v[0])||!wcscmp(v[0],L"*"))&&(or_safe_id(v[1])||!wcscmp(v[1],L"*"))&&
        or_safe_id(v[2])&&or_safe_id(v[3])&&date->from&&date->until>=date->from&&
        or_number(v[6],&number)&&number>=0;
}

static ProError std_rule_import(const wchar_t *input,PartText *out){
    wchar_t path[MAX_PATH],temp[MAX_PATH],real[MAX_PATH];
    OrTable rows={0};
    if(!std_rule_path(path,1)||!or_real_path(input,0,real))return PRO_TK_E_NOT_FOUND;
    ProError error=or_table_load(&rows,real,8,out)?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    StdRuleDates dates[OR_ROWS]={0};
    for(int i=0;!error&&i<rows.size;i++){
        const OrRow *rule=&rows.rows[i];
        if(!std_rule_valid(rule,&dates[i])){
            part_text_add(out,L"第 %u 行版本、日期或工艺值无效。\n",rule->line);
            error=PRO_TK_BAD_INPUTS;
        }
        for(int j=0;!error&&j<i;j++){
            const OrRow *previous=&rows.rows[j];
            if(_wcsicmp(previous->value[0],rule->value[0])||_wcsicmp(previous->value[1],rule->value[1])||
               _wcsicmp(previous->value[2],rule->value[2]))continue;
            if(!_wcsicmp(previous->value[3],rule->value[3])||
                (dates[j].from<=dates[i].until&&dates[i].from<=dates[j].until)){
                part_text_add(out,L"第 %u、%u 行的修订重复或有效期重叠，请修正工艺表。\n",previous->line,rule->line);
                error=PRO_TK_E_FOUND;
            }
        }
    }
    if(!error){
        FILE *file=ax_study_atomic(path,temp);
        if(!file)error=PRO_TK_CANT_WRITE;
        else{
            for(int i=0;i<rows.size;i++){
                for(int j=0;j<8;j++)fwprintf(file,L"%ls%lc",rows.rows[i].value[j],j==7?L'\n':L'\t');
            }
            if(!ax_study_commit(file,temp,path))error=PRO_TK_CANT_WRITE;
        }
    }
    if(!error)part_text_add(out,L"已登记 %d 条工艺规则，按机型、供应商和生效日期调用。\n%ls\n",rows.size,path);
    or_free(&rows);
    return error;
}

static ProError std_rule_query(const wchar_t *series,const wchar_t *vendor,const wchar_t *group,const wchar_t *date,
    PartText *out){
    unsigned day=std_date(date);
    wchar_t path[MAX_PATH];
    if(!or_safe_id(series)||!or_safe_id(vendor)||!or_safe_id(group)||!day)return PRO_TK_BAD_INPUTS;
    OrTable rows={0};
    if(!std_rule_path(path,0)||!or_table_load(&rows,path,8,out)){or_free(&rows);return PRO_TK_E_NOT_FOUND;}
    int best=-1,priority=-1,tied=0;
    for(int i=0;i<rows.size;i++){
        const OrRow *r=&rows.rows[i];
        StdRuleDates validity;
        if(!std_rule_valid(r,&validity)||_wcsicmp(r->value[2],group)||day<validity.from||day>validity.until)continue;
        int specific_series=!_wcsicmp(series,r->value[0]),specific_vendor=!_wcsicmp(vendor,r->value[1]);
        if((!specific_series&&wcscmp(r->value[0],L"*"))||(!specific_vendor&&wcscmp(r->value[1],L"*")))continue;
        int score=specific_series+specific_vendor;
        if(score>priority){best=i;priority=score;tied=0;}
        else if(score==priority)tied=1;
    }
    if(best>=0&&!tied){
        const OrRow *row=&rows.rows[best];
        part_text_add(out,L"%ls / %ls 在 %ls 采用 %ls 修订 %ls，数值 %ls mm，有效期 %ls 至 %ls。\n依据 %ls。\n",
            series,vendor,date,group,row->value[3],row->value[6],row->value[4],row->value[5],row->value[7]);
    }else part_text_add(out,tied?L"有两条同等适用的有效规则，请核对机型与供应商配置。\n":L"该日期没有适用工艺规则。\n");
    or_free(&rows);
    return tied?PRO_TK_E_FOUND:best<0?PRO_TK_E_NOT_FOUND:PRO_TK_NO_ERROR;
}
