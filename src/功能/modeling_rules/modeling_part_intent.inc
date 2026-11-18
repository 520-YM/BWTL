/* Typed blank-part requests. Each clause is consumed before native writes. */
enum { MODELING_PART_NEW=1, MODELING_PART_SINGLE, MODELING_PART_CURRENT };
typedef struct {
    int scope,kind,invalid,same_name,require_company;
    wchar_t name[PRO_NAME_SIZE],revision[4];
} ModelingPartIntent;

static const wchar_t *modeling_part_skip(const wchar_t *p){
    while(*p&&(iswspace(*p)||wcschr(L"，,：:、；;。！!（）()",*p)))p++;
    return p;
}

static int modeling_part_take_any(const wchar_t **p,const wchar_t *const words[],size_t count){
    for(size_t i=0;i<count;i++)if(assembly_take(p,words[i]))return 1;
    return 0;
}

static int modeling_part_revision(const wchar_t **input,wchar_t revision[4]){
    const wchar_t *p=*input;
    if(*p==L'v'||*p==L'V')p++;
    size_t count=wcsspn(p,L"0123456789");
    if(!count||count>2)return 0;
    revision[0]=L'V';
    revision[1]=count==2?p[0]:L'0';
    revision[2]=p[count-1];
    revision[3]=0;
    *input=p+count;
    return 1;
}

static int modeling_part_name(const wchar_t **input,wchar_t name[PRO_NAME_SIZE]){
    const wchar_t *p=*input;
    const wchar_t *quotes=L"「“\"'";
    const wchar_t *open=wcschr(quotes,*p);
    const wchar_t *close=L"」”\"'";
    if(*p&&open)p++;
    else open=NULL;
    size_t count=wcsspn(p,L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-");
    if(!count||count>31)return 0;
    if(open&&p[count]!=close[open-quotes])return 0;
    wmemcpy(name,p,count);
    name[count]=0;
    if(!model_assembly_name_valid(name)||!dv_safe_leaf(name))return 0;
    *input=p+count+(open!=NULL);
    return 1;
}

static int modeling_part_take_standalone(const wchar_t **p){
    return assembly_take(p,L"（单独）")||assembly_take(p,L"(单独)")||assembly_take(p,L"单独");
}

static int modeling_part_parse(const wchar_t *text,ModelingPartIntent *intent){
    static const wchar_t *const negative_single[]={
        L"不要单独创建",L"不要单独新建",L"不要单独建",L"不要单独",
        L"不用单独创建",L"不用单独新建",L"不用单独建",L"不用单独",L"别单独创建",L"别单独新建",L"别单独建",L"别单独"
    };
    static const wchar_t *const positive_tail[]={
        L"单独创建",L"单独新建",L"单独建",L"单独"
    };
    static const wchar_t *const parents[]={
        L"在当前空装配体",L"在当前空装配",L"在当前装配体",L"在当前装配",
        L"在这个空装配体",L"在这个空装配",L"在这个装配体",L"在这个装配",
        L"在该空装配体",L"在该空装配",L"在该装配体",L"在该装配",
        L"在空装配体",L"在空装配",L"在装配体",L"在装配"
    };
    static const wchar_t *const interiors[]={L"里面",L"内部",L"中",L"里",L"内"};
    static const wchar_t *const verbs[]={L"创建",L"新建",L"另建",L"建立",L"建"};
    static const wchar_t *const quantity[]={L"一个",L"1个",L"一件",L"个"};
    static const wchar_t *const sheet[]={L"钣金空零件",L"钣金子零件",L"钣金零件",L"钣金件"};
    static const wchar_t *const solid[]={L"实体子零件",L"实体零件",L"实体件"};
    static const wchar_t *const generic[]={L"子零件",L"子件",L"零件"};
    static const wchar_t *const qualifiers[]={L"新的",L"新",L"空白",L"空"};
    static const wchar_t *const preserve_parent[]={
        L"不要修改当前装配",L"不要改当前装配",L"不要动当前装配",L"别动当前装配",L"保留当前装配"
    };
    static const wchar_t *const company_flow[]={L"按照公司流程",L"按公司流程"};
    static const wchar_t *const unchanged[]={
        L"暂不保存",L"不用保存",L"不保存",L"直接执行",L"一定要做到",L"默认装配",L"默认定位",
        L"自动打开钣金件",L"自动打开子件",L"打开新建的钣金零件",L"然后打开钣金件"
    };
    memset(intent,0,sizeof(*intent));
    const wchar_t *p=model_request_action_start(text);
    int standalone=0,in_parent=0,keep_parent=0;
    for(;;){
        if(modeling_part_take_any(&p,company_flow,_countof(company_flow))){
            intent->require_company=1;
        }else if(modeling_part_take_any(&p,negative_single,_countof(negative_single))){
            if(!*p||!wcschr(L"，,；;。",*p))return 0;
            intent->require_company=1;
        }else if(modeling_part_take_any(&p,preserve_parent,_countof(preserve_parent))){
            if(!*p||!wcschr(L"，,；;。",*p))return 0;
            keep_parent=1;
        }else break;
        p=model_request_action_start(modeling_part_skip(p));
    }
    if(modeling_part_take_any(&p,parents,_countof(parents))){
        modeling_part_take_any(&p,interiors,_countof(interiors));
        in_parent=1;
        p=model_request_action_start(p);
    }
    if(modeling_part_take_standalone(&p))standalone=1;
    p=model_request_action_start(p);
    if(!modeling_part_take_any(&p,verbs,_countof(verbs)))return 0;
    p=model_request_action_start(p);
    if(!standalone&&modeling_part_take_standalone(&p))standalone=1;
    p=modeling_part_skip(p);
    modeling_part_take_any(&p,quantity,_countof(quantity));
    p=modeling_part_skip(p);
    if(!standalone&&modeling_part_take_standalone(&p)){standalone=1;assembly_take(&p,L"的");}
    p=modeling_part_skip(p);
    while(modeling_part_take_any(&p,qualifiers,_countof(qualifiers)))p=modeling_part_skip(p);
    if(modeling_part_take_any(&p,sheet,_countof(sheet)))intent->kind=2;
    else if(modeling_part_take_any(&p,solid,_countof(solid)))intent->kind=1;
    else if(!modeling_part_take_any(&p,generic,_countof(generic)))return 0;

    for(;;){
        p=modeling_part_skip(p);
        if(!*p)break;
        const wchar_t *next=p;
        if(assembly_take(&next,L"图号")||assembly_take(&next,L"零件名")){
            next=modeling_part_skip(next);
            if(assembly_take(&next,L"为")||assembly_take(&next,L"是"))next=modeling_part_skip(next);
            if(intent->name[0]||intent->same_name||!modeling_part_name(&next,intent->name)){intent->invalid=1;break;}
        }else if(assembly_take(&next,L"版本号")||assembly_take(&next,L"版本")){
            next=modeling_part_skip(next);
            if(assembly_take(&next,L"为")||assembly_take(&next,L"是"))next=modeling_part_skip(next);
            if(intent->revision[0]||!modeling_part_revision(&next,intent->revision)){intent->invalid=1;break;}
        }else if(modeling_part_take_any(&next,negative_single,_countof(negative_single)))intent->require_company=1;
        else if(modeling_part_take_any(&next,positive_tail,_countof(positive_tail)))standalone=1;
        else if(in_parent&&(assembly_take(&next,L"和装配同号")||assembly_take(&next,L"跟装配同号")||
                           assembly_take(&next,L"与装配同号")||assembly_take(&next,L"同图号")||assembly_take(&next,L"同号")))
            intent->same_name=1;
        else if(modeling_part_take_any(&next,preserve_parent,_countof(preserve_parent)))keep_parent=1;
        else if(modeling_part_take_any(&next,unchanged,_countof(unchanged))){}
        else if(!intent->name[0]&&!intent->same_name&&
                ((*next>=L'A'&&*next<=L'Z')||(*next>=L'a'&&*next<=L'z')||wcschr(L"「“\"'",*next))&&
            modeling_part_name(&next,intent->name)){}
        else return 0;
        p=next;
    }
    intent->scope=in_parent?MODELING_PART_CURRENT:standalone?MODELING_PART_SINGLE:MODELING_PART_NEW;
    if((in_parent&&keep_parent)||(standalone&&(in_parent||intent->require_company))||
        (intent->same_name&&(intent->name[0]||intent->revision[0])))intent->invalid=1;
    return 1;
}
