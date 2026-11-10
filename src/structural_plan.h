#ifndef BWTL_STRUCTURAL_PLAN_H
#define BWTL_STRUCTURAL_PLAN_H
enum {MD_NONE,MD_SHELL_MULTI,MD_SHELL_LOCAL,MD_DRAFT_MULTI,MD_DRAFT_SPLIT,MD_DRAFT_VARIABLE,MD_RIB,MD_BOSS,MD_SNAP,MD_WINDOW,MD_SEAL_FACE,MD_SEAL_RADIAL,MD_STOCK};
static int md_kind(const wchar_t *v){
    const wchar_t *names[]={L"SHELL_MULTI",L"SHELL_LOCAL",L"DRAFT_MULTI",L"DRAFT_SPLIT",L"DRAFT_VARIABLE",L"DRAFT_PATH_RIB",L"SCREW_BOSS",L"SNAP_HOOK",L"SNAP_WINDOW",L"SEAL_GROOVE_FACE",L"SEAL_GROOVE_RADIAL",L"STOCK_ALLOWANCE"};
    for(int i=0;i<_countof(names);i++)if(!wcscmp(v,names[i]))return i+1;return 0;
}
static int md_angle(const wchar_t *s){double d;return part_number(s,&d)&&fabs(d)>=0.1&&fabs(d)<=30;}
static int md_same_face(const wchar_t *a,const wchar_t *b){int x,y;return part_integer(a+8,&x)&&part_integer(b+8,&y)&&x==y;}
static int md_faces(int n,wchar_t a[][PART_TOKEN_MAX],int first,int maximum){
    if(n-first<1||n-first>maximum)return 0;
    for(int i=first;i<n;i++){
        if(!sf_id(a[i],L"SURFACE:"))return 0;
        for(int j=first;j<i;j++){int x,y;part_integer(a[i]+8,&x);part_integer(a[j]+8,&y);if(x==y)return 0;}
    }return 1;
}
static int structural_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int k=md_kind(a[0]);double x,y,z;
    if(!k)return 0;if(n<4||!fd_name(a[2]))return -1;
    switch(k){
    case MD_SHELL_MULTI:return n>=6&&sx_dim(a[3],10000)&&md_faces(n,a,4,8)?1:-1;
    case MD_SHELL_LOCAL:
        if(n<7||(n-5)%2||n>15||!sx_dim(a[3],10000)||!sf_id(a[4],L"SURFACE:"))return -1;
        for(int i=5;i<n;i+=2){if(!sf_id(a[i],L"SURFACE:")||!sx_dim(a[i+1],10000)||md_same_face(a[i],a[4]))return -1;
            for(int j=5;j<i;j+=2)if(md_same_face(a[i],a[j]))return -1;}
        return 1;
    case MD_DRAFT_MULTI:return n>=8&&ad_plane(a[3])&&ad_plane(a[4])&&md_angle(a[5])&&sx_side(a[6])&&md_faces(n,a,7,8)?1:-1;
    case MD_DRAFT_SPLIT:return n>=9&&ad_plane(a[3])&&ad_plane(a[4])&&md_angle(a[5])&&md_angle(a[6])&&sx_side(a[7])&&md_faces(n,a,8,8)?1:-1;
    case MD_DRAFT_VARIABLE:return n==11&&sf_id(a[3],L"SURFACE:")&&ad_plane(a[4])&&ad_plane(a[5])&&ad_ref(a[6],L"POINT:")&&md_angle(a[7])&&ad_ref(a[8],L"POINT:")&&md_angle(a[9])&&sx_side(a[10])&&wcscmp(a[6],a[8])?1:-1;
    case MD_RIB:return n==8&&sx_curve(a[3])&&ad_plane(a[4])&&sx_dim(a[5],10000)&&sx_dim(a[6],10000)&&part_number(a[7],&x)&&x>=0.1&&x<=30?1:-1;
    case MD_BOSS:return n==9&&sf_id(a[3],L"SURFACE:")&&ad_ref(a[4],L"CSYS:")&&sx_dim(a[5],10000)&&sx_dim(a[6],10000)&&part_number(a[7],&x)&&part_number(a[8],&y)&&part_number(a[5],&z)&&x>=0&&x<z&&(x==0?y==0:y>=0.01)&&part_number(a[6],&z)&&y<=z?1:-1;
    case MD_SNAP:return n==10&&sf_id(a[3],L"SURFACE:")&&ad_ref(a[4],L"CSYS:")&&sx_dim(a[5],10000)&&sx_dim(a[6],10000)&&sx_dim(a[7],10000)&&sx_dim(a[8],10000)&&sx_dim(a[9],10000)&&part_number(a[9],&x)&&part_number(a[5],&y)&&x<y?1:-1;
    case MD_WINDOW:return n==10&&sf_id(a[3],L"SURFACE:")&&ad_ref(a[4],L"CSYS:")&&sx_dim(a[5],10000)&&sx_dim(a[6],10000)&&sx_dim(a[7],10000)&&part_number(a[8],&x)&&x>=0&&x<=1000&&sx_dim(a[9],10000)?1:-1;
    case MD_SEAL_FACE:return n==9&&sf_id(a[3],L"SURFACE:")&&ad_ref(a[4],L"CSYS:")&&sx_dim(a[5],10000)&&sx_dim(a[6],10000)&&part_number(a[7],&x)&&x>=0&&x<=40&&part_number(a[8],&y)&&part_number(a[6],&z)&&y>=z&&part_number(a[5],&z)&&y<z?1:-1;
    case MD_SEAL_RADIAL:return n==10&&(!wcscmp(a[3],L"SHAFT")||!wcscmp(a[3],L"HUB"))&&sf_id(a[4],L"SURFACE:")&&ad_ref(a[5],L"CSYS:")&&part_number(a[6],&x)&&fabs(x)<=1000000&&sx_dim(a[7],10000)&&part_number(a[8],&x)&&x>=0&&x<=40&&part_number(a[9],&y)&&part_number(a[7],&z)&&y>=z?1:-1;
    case MD_STOCK:return n>=5&&sx_dim(a[3],10000)&&md_faces(n,a,4,8)?1:-1;
    }return -1;
}
#endif
