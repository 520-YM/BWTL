#ifndef BWTL_SHEETMETAL_PLAN_H
#define BWTL_SHEETMETAL_PLAN_H
enum {SH_NONE,SH_INFO,SH_SKETCH,SH_CONVERT,SH_FIRST,SH_FLANGE,SH_BEND,SH_HEM,SH_RELIEF,SH_UNBEND,SH_REBEND,SH_CUT,SH_FORM,SH_ALLOWANCE,SH_DXF};
static int sh_kind(const wchar_t *v){
    const wchar_t *verbs[]={L"SMT_INFO",L"SMT_SKETCH",L"SMT_CONVERT",L"SMT_FIRST_WALL",L"SMT_FLANGE",L"SMT_BEND",L"SMT_HEM",L"SMT_RELIEF",L"SMT_UNBEND",L"SMT_REBEND",L"SMT_CUT",L"SMT_FORM",L"SMT_ALLOWANCE",L"SMT_DXF"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(v,verbs[i]))return i+1;return 0;
}
static int sh_angle(const wchar_t *v){double x;return part_number(v,&x)&&x>=1&&x<=175;}
static int sh_sketch_token(const wchar_t *v){return !wcsncmp(v,L"SKETCH:",7)&&fd_name(v+7);}
static int sheetmetal_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int k=sh_kind(a[0]);double x;if(!k)return 0;if(k==SH_INFO)return n==2?1:-1;
    if(n<3||!fd_name(a[2]))return -1;
    switch(k){
    case SH_SKETCH:return n>=9&&ad_plane(a[3])&&ad_plane(a[4])&&fd_shape_valid(n,a,5)?1:-1;
    case SH_CONVERT:return n==5&&sf_id(a[3],L"SURFACE:")&&sx_dim(a[4],1000)?1:-1;
    case SH_FIRST:return n==6&&sh_sketch_token(a[3])&&sx_dim(a[4],1000)&&sx_side(a[5])?1:-1;
    case SH_FLANGE:return n==8&&sf_id(a[3],L"EDGE:")&&sx_dim(a[4],10000)&&sh_angle(a[5])&&sx_dim(a[6],1000)&&sx_side(a[7])?1:-1;
    case SH_BEND:return n==8&&sx_curve(a[3])&&sh_angle(a[4])&&sx_dim(a[5],1000)&&sx_side(a[6])&&(!wcscmp(a[7],L"FIX1")||!wcscmp(a[7],L"FIX2"))?1:-1;
    case SH_HEM:return n==8&&sf_id(a[3],L"EDGE:")&&(!wcscmp(a[4],L"OPEN")||!wcscmp(a[4],L"ROUND"))&&sx_dim(a[5],10000)&&sx_dim(a[6],1000)&&sx_side(a[7])?1:-1;
    case SH_RELIEF:return n==8&&!wcscmp(a[3],L"ALL")&&(!wcscmp(a[4],L"RECT")||!wcscmp(a[4],L"OBROUND"))&&sx_dim(a[5],1000)&&sx_dim(a[6],1000)&&sx_dim(a[7],1000)?1:-1;
    case SH_UNBEND:case SH_REBEND:return n==4&&sf_id(a[3],L"SURFACE:")?1:-1;
    case SH_CUT:return n==5&&sh_sketch_token(a[3])&&sx_side(a[4])?1:-1;
    case SH_FORM:return n==7&&sh_sketch_token(a[3])&&sx_dim(a[4],1000)&&sx_dim(a[5],1000)&&sx_side(a[6])?1:-1;
    case SH_ALLOWANCE:return n==5&&(!wcscmp(a[3],L"K")?part_number(a[4],&x)&&x>0&&x<1:!wcscmp(a[3],L"BD")?sx_dim(a[4],10000):!wcscmp(a[3],L"TABLE")?fd_name(a[4]):0)?1:-1;
    case SH_DXF:return n==4&&sf_id(a[3],L"SURFACE:")?1:-1;
    }return -1;
}
#endif
