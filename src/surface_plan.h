#ifndef BWTL_SURFACE_PLAN_H
#define BWTL_SURFACE_PLAN_H
enum {SF_NONE,SF_EXTRUDE,SF_REVOLVE,SF_BOUNDARY,SF_FILL,SF_OFFSET,SF_TRIM,SF_EXTEND,SF_MERGE,SF_SOLIDIFY,SF_THICKEN,SF_BODY_EXTRUDE,SF_BOOLEAN,SF_INFO};
static int sf_kind(const wchar_t *verb){
    const wchar_t *verbs[]={L"SURFACE_EXTRUDE",L"SURFACE_REVOLVE",L"SURFACE_BOUNDARY",L"SURFACE_FILL",L"SURFACE_OFFSET",L"SURFACE_TRIM",L"SURFACE_EXTEND",L"SURFACE_MERGE",L"SURFACE_SOLIDIFY",L"SURFACE_THICKEN",L"BODY_EXTRUDE",L"BODY_BOOLEAN",L"SURFACE_INFO"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return SF_NONE;
}
static int sf_id(const wchar_t *token,const wchar_t *prefix){
    int id;
    size_t length=wcslen(prefix);
    return !wcsncmp(token,prefix,length)&&part_integer(token+length,&id)&&id>0;
}
static int sf_quilt(const wchar_t *token){
    return sf_id(token,L"QUILT:")||(!wcsncmp(token,L"QUILT:",6)&&fd_name(token+6));
}
static int surface_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int kind=sf_kind(a[0]);
    double value;
    if(!kind)return 0;
    if(kind==SF_INFO)return n==2?1:-1;
    if(n<4||!fd_name(a[2]))return -1;
    switch(kind){
    case SF_EXTRUDE:case SF_BODY_EXTRUDE:
        return n==6&&fd_name(a[3])&&sx_dim(a[4],1000000)&&sx_side(a[5])?1:-1;
    case SF_REVOLVE:
        return n==7&&fd_name(a[3])&&ad_ref(a[4],L"AXIS:")&&sx_dim(a[5],360)&&sx_side(a[6])?1:-1;
    case SF_FILL:return n==4&&fd_name(a[3])?1:-1;
    case SF_BOUNDARY:
        if(n!=5||!_wcsicmp(a[3],a[4]))return -1;
        for(int i=3;i<n;i++)if(!sx_curve(a[i]))return -1;
        return 1;
    case SF_OFFSET:
        return n==5&&sf_quilt(a[3])&&part_number(a[4],&value)&&fabs(value)>=0.001&&fabs(value)<=10000?1:-1;
    case SF_TRIM:
        return n==6&&sf_quilt(a[3])&&(ad_plane(a[4])||sf_quilt(a[4])||sx_curve(a[4]))&&sx_side(a[5])?1:-1;
    case SF_EXTEND:return n==6&&sf_quilt(a[3])&&sf_id(a[4],L"EDGE:")&&sx_dim(a[5],10000)?1:-1;
    case SF_MERGE:
        if(n<5||n>10)return -1;
        for(int i=3;i<n;i++){
            if(!sf_quilt(a[i]))return -1;
            for(int j=3;j<i;j++)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    case SF_SOLIDIFY:return n==4&&sf_quilt(a[3])?1:-1;
    case SF_THICKEN:return n==6&&sf_quilt(a[3])&&sx_dim(a[4],10000)&&(!wcscmp(a[5],L"SYM")||sx_side(a[5]))?1:-1;
    case SF_BOOLEAN:
        return n==6&&(!wcscmp(a[3],L"MERGE")||!wcscmp(a[3],L"SUBTRACT")||!wcscmp(a[3],L"INTERSECT"))&&
            sf_id(a[4],L"BODY:")&&sf_id(a[5],L"BODY:")&&wcscmp(a[4],a[5])?1:-1;
    }
    return -1;
}
#endif
