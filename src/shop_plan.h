#ifndef BWTL_SHOP_PLAN_H
#define BWTL_SHOP_PLAN_H
enum ShopKind {MP_NONE,MP_STEP,MP_CROSS,MP_BACK,MP_DEBURR,MP_ISLAND,MP_LEVEL,MP_DOG,MP_FLAT,MP_GROOVE,MP_TOOL};

static int mp_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"SHOP_STEP",L"SHOP_CROSS",L"SHOP_BACK",L"SHOP_DEBURR",L"SHOP_ISLAND",
        L"SHOP_LEVEL",L"SHOP_DOG",L"SHOP_FLAT",L"SHOP_GROOVE",L"SHOP_TOOL"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return MP_NONE;
}

static int mp_request(const wchar_t *text){
    static const wchar_t *const terms[]={L"SHOP_",L"多级台阶孔",L"交叉通道",L"交叉油道",L"孔背面沉台",L"孔口去毛刺",
        L"带岛型腔",L"分层型腔",L"内角过切",L"狗骨避让",L"孔内方位平面",L"轴肩退刀槽",L"螺钉头工具让位"};
    for(int i=0;i<_countof(terms);i++)if(wcsstr(text,terms[i]))return 1;
    return 0;
}

static int mp_coord(const wchar_t *v){return qa_number(v,-10000,10000);}
static int mp_size(const wchar_t *v){return qa_number(v,0.1,10000);}
static int mp_surface(const wchar_t *v){return sf_id(v,L"SURFACE:");}
static int mp_named_ref(const wchar_t *v,const wchar_t *prefix){
    size_t n=wcslen(prefix);
    return !wcsncmp(v,prefix,n)&&fd_name(v+n);
}
static int mp_frame_tokens(wchar_t a[][PART_TOKEN_MAX],int at){
    return mp_surface(a[at])&&mp_named_ref(a[at+1],L"CSYS:")&&mp_named_ref(a[at+2],L"PLANE:");
}
static int shop_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int k=mp_kind(a[0]),count;
    if(!k)return 0;
    if(n<7||!fd_name(a[1])||!fd_name(a[2])||wcslen(a[2])>22)return -1;
    if(k==MP_DEBURR){
        if(!mp_size(a[3])||!part_integer(a[4],&count)||count<1||count>6||n!=5+2*count)return -1;
        for(int i=5;i<n;i+=2)if(!mp_surface(a[i])||!mp_surface(a[i+1]))return -1;
        return 1;
    }
    if(k==MP_GROOVE){
        if(n!=12||!mp_surface(a[3])||!mp_named_ref(a[4],L"CSYS:")||!mp_named_ref(a[5],L"AXIS:")||
           !mp_named_ref(a[6],L"PLANE:")||!mp_named_ref(a[7],L"PLANE:")||!mp_coord(a[8]))return -1;
        for(int i=9;i<12;i++)if(!mp_size(a[i]))return -1;
        return 2*wcstod(a[11],NULL)<wcstod(a[9],NULL)&&wcstod(a[11],NULL)<wcstod(a[10],NULL)?1:-1;
    }
    if(!mp_frame_tokens(a,3))return -1;
    if(k==MP_BACK||k==MP_TOOL||k==MP_FLAT){
        if(n!=(k==MP_TOOL?11:10)||!mp_surface(a[6]))return -1;
        for(int i=7;i<n;i++){
            if(k==MP_FLAT&&i==9){if(!qa_number(a[i],-360,360))return -1;}
            else if(k==MP_TOOL&&i==8){if(!qa_number(a[i],0,1000))return -1;}
            else if(!mp_size(a[i]))return -1;
        }
        return 1;
    }
    if(!mp_coord(a[6])||!mp_coord(a[7]))return -1;
    if(k==MP_CROSS)
        return n==17&&mp_size(a[8])&&mp_size(a[9])&&mp_frame_tokens(a,10)&&mp_coord(a[13])&&mp_coord(a[14])&&
            mp_size(a[15])&&mp_size(a[16])?1:-1;
    if(k==MP_DOG){
        if(n!=12)return -1;
        for(int i=8;i<12;i++)if(!mp_size(a[i]))return -1;
        return wcstod(a[11],NULL)<fmin(wcstod(a[8],NULL),wcstod(a[9],NULL))/2?1:-1;
    }
    if(k==MP_ISLAND){
        if(n<16||!mp_size(a[8])||!mp_size(a[9])||!mp_size(a[10])||
           !part_integer(a[11],&count)||count<1||count>4||n!=12+4*count)return -1;
        for(int i=12;i<n;i+=4)
            if(!mp_coord(a[i])||!mp_coord(a[i+1])||!mp_size(a[i+2])||!mp_size(a[i+3]))return -1;
        return 1;
    }
    int stride=k==MP_STEP?2:3;
    if(!part_integer(a[8],&count)||count<(k==MP_STEP?3:2)||count>5||n!=9+stride*count)return -1;
    for(int i=9;i<n;i++)if(!mp_size(a[i]))return -1;
    for(int i=9+stride;i<n;i+=stride){
        for(int j=0;j<stride-1;j++)if(wcstod(a[i+j],NULL)>=wcstod(a[i+j-stride],NULL)-0.1)return -1;
        if(wcstod(a[i+stride-1],NULL)<=wcstod(a[i-1],NULL)+0.1)return -1;
    }
    return 1;
}
#endif
