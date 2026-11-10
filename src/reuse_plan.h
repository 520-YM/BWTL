#ifndef BWTL_REUSE_PLAN_H
#define BWTL_REUSE_PLAN_H
enum {UX_NONE,UX_INFO,UX_MIRROR,UX_GRID,UX_CURVE,UX_POINTS,UX_POINT_SKETCH,UX_SKIP,UX_GROUP,UX_UDF_SAVE,UX_UDF_INFO,UX_UDF_PLACE,UX_PARAM,UX_RELATION,UX_FAMILY};
static int ux_kind(const wchar_t *v){const wchar_t *names[]={L"REUSE_INFO",L"REUSE_MIRROR",L"REUSE_GRID",L"REUSE_CURVE",L"REUSE_POINTS",L"REUSE_POINT_SKETCH",L"REUSE_SKIP",L"REUSE_GROUP",L"REUSE_UDF_SAVE",L"REUSE_UDF_INFO",L"REUSE_UDF_PLACE",L"REUSE_PARAM",L"REUSE_RELATION",L"REUSE_FAMILY"};for(int i=0;i<_countof(names);i++)if(!wcscmp(v,names[i]))return i+1;return 0;}
static int ux_count(const wchar_t *s,int low,int high){int n;return part_integer(s,&n)&&n>=low&&n<=high;}
static int ux_expr(const wchar_t *s){if(!*s||wcslen(s)>200)return 0;for(;*s;s++)if(!iswalnum(*s)&&!wcschr(L"_+-*/().",*s))return 0;return 1;}
static int reuse_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
 int k=ux_kind(a[0]),n1,n2;double v;if(!k)return 0;if(k==UX_INFO)return n==2?1:-1;if(n<3||!fd_name(a[2]))return -1;
 switch(k){
 case UX_MIRROR:if(n<6||n>12||!ad_plane(a[3]))return -1;for(int i=4;i<n;i++){if(!fd_name(a[i]))return -1;for(int j=4;j<i;j++)if(!_wcsicmp(a[j],a[i]))return -1;}return 1;
 case UX_GRID:return n==12&&fd_name(a[3])&&ad_plane(a[4])&&ux_count(a[5],2,20)&&sx_dim(a[6],10000)&&sx_side(a[7])&&ad_plane(a[8])&&ux_count(a[9],2,20)&&sx_dim(a[10],10000)&&sx_side(a[11])&&part_integer(a[5],&n1)&&part_integer(a[9],&n2)&&n1*n2<=64?1:-1;
 case UX_CURVE:return n==8&&fd_name(a[3])&&sx_curve(a[4])&&(!wcscmp(a[5],L"COUNT")?ux_count(a[6],2,64):!wcscmp(a[5],L"SPACING")&&sx_dim(a[6],10000))&&sx_side(a[7])?1:-1;
 case UX_POINTS:return n==5&&fd_name(a[3])&&sh_sketch_token(a[4])?1:-1;
 case UX_POINT_SKETCH:if(n<9||n>29||!(n%2)||!ad_plane(a[3])||!ad_plane(a[4]))return -1;for(int i=5;i<n;i++)if(!part_number(a[i],&v)||fabs(v)>1000000)return -1;for(int i=5;i<n;i+=2)for(int j=5;j<i;j+=2)if(wcstod(a[i],NULL)==wcstod(a[j],NULL)&&wcstod(a[i+1],NULL)==wcstod(a[j+1],NULL))return -1;return 1;
 case UX_SKIP:return n==5&&ux_count(a[3],2,64)&&(!wcscmp(a[4],L"SKIP")||!wcscmp(a[4],L"RESTORE"))?1:-1;
 case UX_GROUP:if(n<5||n>11)return -1;for(int i=3;i<n;i++){if(!fd_name(a[i]))return -1;for(int j=3;j<i;j++)if(!_wcsicmp(a[i],a[j]))return -1;}return 1;
 case UX_UDF_SAVE:return n==4&&fd_name(a[3])?1:-1;
 case UX_UDF_INFO:return n==3?1:-1;
 case UX_UDF_PLACE:if(n<4||n>12||!fd_name(a[3]))return -1;for(int i=4;i<n;i++)if(!fd_reference_token(a[i]))return -1;return 1;
 case UX_PARAM:if(n!=5&&n!=6)return -1;if(!wcscmp(a[3],L"STRING"))return n==5&&a[4][0]&&wcslen(a[4])<80?1:-1;if(!wcscmp(a[3],L"BOOLEAN"))return n==5&&(!wcscmp(a[4],L"TRUE")||!wcscmp(a[4],L"FALSE"))?1:-1;if(!wcscmp(a[3],L"INTEGER"))return n==5&&part_number(a[4],&v)&&fabs(v)<=2147483647&&v==floor(v)?1:-1;return !wcscmp(a[3],L"REAL")&&part_number(a[4],&v)&&fabs(v)<=1e9&&(n==5||!wcscmp(a[5],L"MM"))?1:-1;
 case UX_RELATION:return n==4&&ux_expr(a[3])?1:-1;
 case UX_FAMILY:if(n<6||n>12||wcslen(a[2])>24||wcsncmp(a[3],L"DIM:",4)||!fd_name(a[3]+4))return -1;for(int i=4;i<n;i++){if(!sx_dim(a[i],10000))return -1;for(int j=4;j<i;j++)if(wcstod(a[i],NULL)==wcstod(a[j],NULL))return -1;}return 1;
 }return -1;
}
#endif
