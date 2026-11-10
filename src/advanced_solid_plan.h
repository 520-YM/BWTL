#ifndef BWTL_ADVANCED_SOLID_PLAN_H
#define BWTL_ADVANCED_SOLID_PLAN_H
enum {SX_NONE,SX_SYMMETRIC,SX_REFERENCE,SX_TWO_SIDE,SX_THIN,SX_PATH,SX_PROFILE,SX_VARIABLE,SX_CUT,SX_LOFT,SX_HELIX};

static int sx_kind(const wchar_t *verb){
    const wchar_t *verbs[]={L"SOLID_EXTRUDE_SYM",L"SOLID_EXTRUDE_TO",L"SOLID_EXTRUDE_TWO",L"SOLID_EXTRUDE_THIN",L"SOLID_SWEEP_CHAIN",L"SOLID_SWEEP_PROFILE",L"SOLID_SWEEP_VARIABLE",L"SOLID_SWEEP_CUT",L"SOLID_LOFT",L"SOLID_HELIX"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return SX_NONE;
}
static int sx_dim(const wchar_t *text,double max){double v;return part_number(text,&v)&&v>=0.01&&v<=max;}
static int sx_side(const wchar_t *text){return !wcscmp(text,L"POS")||!wcscmp(text,L"NEG");}
static int sx_material(const wchar_t *text){return !wcscmp(text,L"ADD")||!wcscmp(text,L"CUT");}
static int sx_curve(const wchar_t *text){return !wcsncmp(text,L"CURVE:",6)&&fd_name(text+6);}

static int sx_section(int n,wchar_t a[][PART_TOKEN_MAX],int first){
    int count=n-first;
    if(count<2)return 0;
    if(!wcscmp(a[first],L"CIRCLE"))return count==2&&sx_dim(a[first+1],10000);
    if(!wcscmp(a[first],L"RECT")||!wcscmp(a[first],L"ELLIPSE"))
        return count==3&&sx_dim(a[first+1],10000)&&sx_dim(a[first+2],10000);
    return count==2&&!wcscmp(a[first],L"SKETCH")&&fd_name(a[first+1]);
}

static int advanced_solid_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int kind=sx_kind(a[0]);
    if(!kind)return 0;
    if(n<4||!fd_name(a[2]))return -1;
    switch(kind){
    case SX_SYMMETRIC:
        return n==6&&fd_name(a[3])&&sx_material(a[4])&&sx_dim(a[5],1000000)?1:-1;
    case SX_REFERENCE:
        return n==7&&fd_name(a[3])&&sx_material(a[4])&&sx_side(a[6])&&
            (!wcscmp(a[5],L"NEXT")||((!wcsncmp(a[5],L"PLANE:",6)||!wcsncmp(a[5],L"SURFACE:",8))&&fd_reference_token(a[5])))?1:-1;
    case SX_TWO_SIDE:
        return n==8&&fd_name(a[3])&&sx_material(a[4])&&sx_dim(a[5],1000000)&&sx_dim(a[6],1000000)&&sx_side(a[7])?1:-1;
    case SX_THIN:
        return n==8&&fd_name(a[3])&&sx_dim(a[4],1000000)&&sx_dim(a[5],10000)&&
            (!wcscmp(a[6],L"SYM")||sx_side(a[6]))&&sx_side(a[7])?1:-1;
    case SX_PATH:return n==5&&sx_curve(a[3])&&sx_dim(a[4],10000)?1:-1;
    case SX_PROFILE:case SX_CUT:return sx_curve(a[3])&&sx_section(n,a,4)?1:-1;
    case SX_VARIABLE:{
        double first,last;
        return n==6&&sx_curve(a[3])&&sx_dim(a[4],10000)&&sx_dim(a[5],10000)&&
            part_number(a[4],&first)&&part_number(a[5],&last)&&first!=last?1:-1;
    }
    case SX_LOFT:
        if(n<6||n>10)return -1;
        for(int i=3;i<n;i++){if(!fd_name(a[i]))return -1;for(int j=3;j<i;j++)if(!_wcsicmp(a[i],a[j]))return -1;}
        return 1;
    case SX_HELIX:{
        double turns;
        return n>=10&&n<=11&&sx_material(a[3])&&!wcsncmp(a[4],L"CSYS:",5)&&fd_name(a[4]+5)&&
            sx_dim(a[5],10000)&&sx_dim(a[6],10000)&&part_number(a[7],&turns)&&turns>=0.25&&turns<=20&&
            (!wcscmp(a[8],L"RH")||!wcscmp(a[8],L"LH"))&&sx_section(n,a,9)&&wcscmp(a[9],L"SKETCH")?1:-1;
    }
    }
    return -1;
}
#endif
