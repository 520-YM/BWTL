#ifndef BWTL_ADVANCED_MACHINING_PLAN_H
#define BWTL_ADVANCED_MACHINING_PLAN_H
enum {AM_NONE,AM_RADIAL,AM_AXIS,AM_THREAD_THRU,AM_THREAD_STD,AM_SPOTFACE,AM_KEYWAY,AM_RING,AM_VARIABLE,AM_TANGENT,AM_CHAMFER,AM_INFO};
static int am_kind(const wchar_t *verb){
    const wchar_t *v[]={L"HOLE_RADIAL",L"HOLE_AXIS",L"HOLE_THREAD_THRU",L"HOLE_THREAD_STD",L"SPOTFACE",L"KEYWAY",L"RING_GROOVE",L"EDGE_ROUND_VARIABLE",L"EDGE_ROUND_TANGENT",L"EDGE_CHAMFER_ADV",L"ADV_MACHINING_INFO"};
    for(int i=0;i<_countof(v);i++)if(!wcscmp(v[i],verb))return i+1;
    return 0;
}
static int am_chart_size(const wchar_t *s){
    if(!*s||wcslen(s)>30)return 0;
    for(;*s;s++)if(!iswalnum(*s)&&!wcschr(L"./-",*s))return 0;
    return 1;
}
static int advanced_machining_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int k=am_kind(a[0]);double x,y,z;
    if(!k)return 0;
    if(k==AM_INFO)return n==2?1:-1;
    if(n<4||!fd_name(a[2]))return -1;
    switch(k){
    case AM_RADIAL:return n==11&&sf_id(a[3],L"SURFACE:")&&ad_plane(a[4])&&part_number(a[5],&x)&&fabs(x)<=360&&ad_plane(a[6])&&part_number(a[7],&x)&&fabs(x)<=1000000&&sx_dim(a[8],10000)&&part_number(a[10],&y)&&mc_hole_depth(a[9],y)?1:-1;
    case AM_AXIS:return n==9&&ad_ref(a[3],L"POINT:")&&ad_ref(a[4],L"AXIS:")&&sx_dim(a[5],10000)&&part_number(a[7],&x)&&mc_hole_depth(a[6],x)&&sx_side(a[8])?1:-1;
    case AM_SPOTFACE:return n==8&&ad_ref(a[3],L"POINT:")&&ad_ref(a[4],L"AXIS:")&&sx_dim(a[5],10000)&&sx_dim(a[6],10000)&&sx_side(a[7])?1:-1;
    case AM_THREAD_THRU:case AM_THREAD_STD:
        if(n!=13||!sf_id(a[3],L"SURFACE:")||!mc_plane_token(a[4])||!mc_plane_token(a[6])||!part_number(a[5],&x)||!part_number(a[7],&y)||fabs(x)>1000000||fabs(y)>1000000||!am_chart_size(a[9])||!part_number(a[11],&x)||!part_number(a[12],&y))return -1;
        if(k==AM_THREAD_THRU&&wcscmp(a[8],L"ISO"))return -1;
        if(k==AM_THREAD_STD&&wcscmp(a[8],L"UNC")&&wcscmp(a[8],L"UNF"))return -1;
        return (!wcscmp(a[10],L"THROUGH")&&x==0&&y==0)||
            (k==AM_THREAD_STD&&!wcscmp(a[10],L"BLIND")&&x>=0.01&&y>x&&y<=10000)?1:-1;
    case AM_KEYWAY:case AM_RING:
        return n==(k==AM_KEYWAY?10:9)&&(!wcscmp(a[3],L"SHAFT")||!wcscmp(a[3],L"HUB"))&&sf_id(a[4],L"SURFACE:")&&ad_ref(a[5],L"CSYS:")&&part_number(a[6],&x)&&fabs(x)<=1000000&&sx_dim(a[7],10000)&&sx_dim(a[8],10000)&&
            (k==AM_RING||(sx_dim(a[9],10000)&&part_number(a[7],&y)&&part_number(a[8],&z)&&z>y))?1:-1;
    case AM_VARIABLE:return n==6&&sf_id(a[3],L"EDGE:")&&sx_dim(a[4],10000)&&sx_dim(a[5],10000)&&part_number(a[4],&x)&&part_number(a[5],&y)&&x!=y?1:-1;
    case AM_TANGENT:return n==5&&sf_id(a[3],L"EDGE:")&&sx_dim(a[4],10000)?1:-1;
    case AM_CHAMFER:return n==8&&sf_id(a[3],L"EDGE:")&&sf_id(a[4],L"SURFACE:")&&sx_dim(a[7],10000)&&
        ((!wcscmp(a[5],L"DD")&&sx_dim(a[6],10000))||(!wcscmp(a[5],L"AD")&&part_number(a[6],&x)&&x>=1&&x<=89))?1:-1;
    }
    return -1;
}
#endif
