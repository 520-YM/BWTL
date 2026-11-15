static const wchar_t *markup_skip_space(const wchar_t *text){
    while(iswspace(*text))text++;
    return text;
}

static int markup_take_phrase(const wchar_t **text,const wchar_t *const *phrases,size_t count){
    for(size_t i=0;i<count;i++){
        size_t length=wcslen(phrases[i]);
        if(!wcsncmp(*text,phrases[i],length)){
            *text=markup_skip_space(*text+length);
            return 1;
        }
    }
    return 0;
}

/* Accept one absolute target diameter, including common variations in Chinese. */
static int markup_parse_diameter(const wchar_t *text,double *diameter){
    static const wchar_t *const subjects[]={L"这个孔",L"该孔",L"孔"};
    static const wchar_t *const diameter_words[]={L"直径",L"孔径"};
    static const wchar_t *const edit_words[]={L"修改为",L"修改成",L"修改到",L"改成",L"改为",L"改到",L"设为",L"调到",L"改"};
    text=markup_skip_space(text);
    if(*text==L'把'||*text==L'将')text=markup_skip_space(text+1);
    if(wcsncmp(text,L"孔径",2)&&markup_take_phrase(&text,subjects,_countof(subjects))&&*text==L'的')
        text=markup_skip_space(text+1);
    int named=markup_take_phrase(&text,diameter_words,_countof(diameter_words));
    int edit=markup_take_phrase(&text,edit_words,_countof(edit_words));
    while(iswspace(*text)||*text==L':'||*text==L'：')text++;
    int symbol=*text==L'Ø'||*text==L'ø'||*text==L'Φ'||*text==L'φ'||*text==L'⌀';
    if(symbol)text=markup_skip_space(text+1);
    if(!(named||edit||symbol)||*text<L'0'||*text>L'9')return 0;
    double number=0;
    while(*text>=L'0'&&*text<=L'9'){
        number=number*10+(*text++-L'0');
        if(number>10000)return 0;
    }
    if(*text==L'.'){
        text++;
        if(*text<L'0'||*text>L'9')return 0;
        double place=0.1;
        while(*text>=L'0'&&*text<=L'9'){
            number+=(*text++-L'0')*place;
            place*=0.1;
        }
    }
    while(iswspace(*text))text++;
    if(!_wcsnicmp(text,L"mm",2)||!wcsncmp(text,L"毫米",2))text+=2;
    while(iswspace(*text)||*text==L'。'||*text==L'！'||*text==L'!')text++;
    if(*text||number<0.1||number>10000)return 0;
    *diameter=number;
    return 1;
}

static int markup_named_hole(const wchar_t *text){
    return wcsstr(text,L"孔径")||wcsstr(text,L"直径")||wcsstr(text,L"这个孔")||wcsstr(text,L"该孔")||
        wcspbrk(text,L"ØøΦφ⌀");
}

