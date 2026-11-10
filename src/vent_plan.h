#ifndef BWTL_VENT_PLAN_H
#define BWTL_VENT_PLAN_H
enum VentKind {VT_NONE,VT_ROUND,VT_SLOT,VT_LOUVER,VT_GRID,VT_DRAIN,VT_GLAND,VT_GROMMET,VT_STOP,VT_SEAL,VT_ROUTE};

static int vt_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"VENT_ROUND",L"VENT_SLOT",L"VENT_LOUVER",L"VENT_GRID",L"VENT_DRAIN",
        L"VENT_GLAND",L"VENT_GROMMET",L"VENT_STOP",L"VENT_SEAL",L"VENT_ROUTE"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return VT_NONE;
}

static int vt_request(const wchar_t *text){
    static const wchar_t *const terms[]={L"VENT_",L"圆孔散热",L"散热圆孔",L"长槽散热",L"散热长槽",L"百叶通风",
        L"冲孔网格",L"底部排水孔",L"防水接头孔",L"格兰头孔",L"护线圈安装",L"挡水台阶",L"矩形密封槽",L"线束转弯"};
    for(int i=0;i<_countof(terms);i++)if(wcsstr(text,terms[i]))return 1;
    return 0;
}

static int vt_integer(const wchar_t *v,int max){
    int i;
    return part_integer(v,&i)&&i>=1&&i<=max;
}

static int vent_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int k=vt_kind(a[0]);
    if(!k)return 0;
    if(n<9||!fd_name(a[1]))return -1;
    if(k==VT_ROUTE){
        return n==9&&mp_named_ref(a[2],L"CSYS:")&&mp_coord(a[3])&&mp_coord(a[4])&&mp_coord(a[5])&&
            mp_size(a[6])&&mp_size(a[7])&&qa_number(a[8],0,1000)&&
            wcstod(a[6],NULL)>wcstod(a[7],NULL)/2+wcstod(a[8],NULL)?1:-1;
    }
    if(!fd_name(a[2])||wcslen(a[2])>16||!mp_frame_tokens(a,3)||!mp_coord(a[6])||!mp_coord(a[7])||!mp_size(a[8]))return -1;
    if(k==VT_ROUND||k==VT_SLOT||k==VT_GRID){
        if(n!=15||!vt_integer(a[9],8)||!vt_integer(a[10],8)||!mp_size(a[11])||!mp_size(a[12])||!mp_size(a[13]))return -1;
        if(k==VT_ROUND)return !wcscmp(a[14],L"GRID")||!wcscmp(a[14],L"STAGGER")?1:-1;
        return mp_size(a[14])&&(k!=VT_SLOT||wcstod(a[13],NULL)>wcstod(a[14],NULL)+0.1)?1:-1;
    }
    if(k==VT_LOUVER){
        if(n!=16||!vt_integer(a[9],4)||!qa_number(a[13],10,65))return -1;
        for(int i=10;i<16;i++)if(!mp_size(a[i]))return -1;
        return 1;
    }
    if(k==VT_DRAIN)return n==12&&sf_id(a[9],L"EDGE:")&&mp_size(a[7])&&mp_size(a[10])&&mp_size(a[11])&&
        wcstod(a[10],NULL)>wcstod(a[11],NULL)+0.1?1:-1;
    if(k==VT_GLAND||k==VT_GROMMET){
        if(n!=(k==VT_GLAND?14:15)||!mp_size(a[10])||!mp_size(a[n-2])||!mp_size(a[n-1])||
           wcstod(a[n-2],NULL)>wcstod(a[n-1],NULL))return -1;
        double size=wcstod(a[10],NULL);
        if(!wcscmp(a[9],L"ROUND"))return !wcscmp(a[11],L"0")&&(k==VT_GLAND||!wcscmp(a[12],L"0"))?1:-1;
        if(k==VT_GLAND)return !wcscmp(a[9],L"D")&&qa_number(a[11],0.1,size/2-0.1)?1:-1;
        return !wcscmp(a[9],L"RECT")&&mp_size(a[11])&&qa_number(a[12],0.1,fmin(size,wcstod(a[11],NULL))/2-0.01)?1:-1;
    }
    if(n!=(k==VT_STOP?12:14))return -1;
    for(int i=9;i<n;i++)if(!mp_size(a[i]))return -1;
    if(k==VT_STOP)return 1;
    return 2*wcstod(a[11],NULL)+0.2<fmin(wcstod(a[9],NULL),wcstod(a[10],NULL))&&
        wcstod(a[12],NULL)+wcstod(a[13],NULL)<=wcstod(a[8],NULL)+0.000001?1:-1;
}
#endif
