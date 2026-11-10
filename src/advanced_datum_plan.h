#ifndef BWTL_ADVANCED_DATUM_PLAN_H
#define BWTL_ADVANCED_DATUM_PLAN_H

typedef enum {AD_NONE,AD_ANGLE,AD_THREE,AD_MID,AD_TANGENT,AD_NORMAL,AD_AXIS_POINTS,AD_POINT_CURVE,AD_INTERSECTION,AD_SPLINE,AD_SYM_CSYS} AdKind;

static AdKind ad_kind(const wchar_t *verb){
    const wchar_t *names[]={L"DATUM_PLANE_ANGLE",L"DATUM_PLANE_3POINT",L"DATUM_PLANE_MID",L"DATUM_PLANE_TANGENT",L"DATUM_PLANE_NORMAL",
        L"DATUM_AXIS_2POINT",L"DATUM_POINT_CURVE",L"DATUM_CURVE_INTERSECT",L"DATUM_CURVE_SPLINE",L"DATUM_CSYS_SYM"};
    int i;
    for(i=0;i<_countof(names);i++)if(!wcscmp(verb,names[i]))return (AdKind)(i+1);
    return AD_NONE;
}

static int ad_ref(const wchar_t *s,const wchar_t *prefix){return !wcsncmp(s,prefix,wcslen(prefix))&&fd_reference_token(s);}
static int ad_plane(const wchar_t *s){return ad_ref(s,L"PLANE:")||ad_ref(s,L"SURFACE:");}
static int ad_sign(const wchar_t *s){return !wcscmp(s,L"POS")||!wcscmp(s,L"NEG");}

static int advanced_datum_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    AdKind kind=ad_kind(a[0]);
    int i;
    double value;
    if(!kind)return 0;
    if(n<5||!fd_name(a[2]))return -1;
    switch(kind){
    case AD_ANGLE:return n==6&&ad_plane(a[3])&&(ad_ref(a[4],L"AXIS:")||ad_ref(a[4],L"EDGE:"))&&
        part_number(a[5],&value)&&fabs(value)>=0.1&&fabs(value)<180?1:-1;
    case AD_THREE:return n==6&&ad_ref(a[3],L"POINT:")&&ad_ref(a[4],L"POINT:")&&ad_ref(a[5],L"POINT:")?1:-1;
    case AD_MID:case AD_INTERSECTION:return n==5&&ad_plane(a[3])&&ad_plane(a[4])&&_wcsicmp(a[3],a[4])?1:-1;
    case AD_TANGENT:return n==5&&ad_ref(a[3],L"SURFACE:")&&ad_ref(a[4],L"POINT:")?1:-1;
    case AD_NORMAL:return n==5&&ad_ref(a[3],L"CURVE:")&&ad_ref(a[4],L"POINT:")?1:-1;
    case AD_AXIS_POINTS:return n==5&&ad_ref(a[3],L"POINT:")&&ad_ref(a[4],L"POINT:")&&_wcsicmp(a[3],a[4])?1:-1;
    case AD_POINT_CURVE:
        if(n!=6||!ad_ref(a[3],L"CURVE:")||!part_number(a[5],&value)||value<0)return -1;
        if(!wcscmp(a[4],L"PERCENT"))return value<=100?1:-1;
        if(!wcscmp(a[4],L"LENGTH"))return value<=1000000?1:-1;
        return (!wcscmp(a[4],L"START")||!wcscmp(a[4],L"END"))&&value==0?1:-1;
    case AD_SPLINE:
        if(n<13||n>28||(n-4)%3||!ad_ref(a[3],L"CSYS:"))return -1;
        for(i=4;i<n;i++)if(!part_number(a[i],&value)||fabs(value)>1000000)return -1;
        return 1;
    case AD_SYM_CSYS:return n==8&&ad_plane(a[3])&&ad_plane(a[4])&&ad_plane(a[5])&&ad_sign(a[6])&&ad_sign(a[7])?1:-1;
    default:return -1;
    }
}
#endif
