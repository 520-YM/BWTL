#ifndef BWTL_MACHINING_PLAN_H
#define BWTL_MACHINING_PLAN_H
/* Batch 1. Strict native protocol, independent of Creo and the desktop. */
static int mc_is_command(const wchar_t *v){
    return !wcscmp(v,L"HOLE")||!wcscmp(v,L"HOLE_COUNTERBORE")||!wcscmp(v,L"HOLE_COUNTERSINK")||
        !wcscmp(v,L"HOLE_THREAD")||!wcscmp(v,L"EDGE_ROUND")||!wcscmp(v,L"EDGE_CHAMFER")||
        !wcscmp(v,L"SOLID_SHELL")||!wcscmp(v,L"MACHINING_INFO");
}
static int mc_plane_token(const wchar_t *s){
    return (!wcsncmp(s,L"SURFACE:",8)||!wcsncmp(s,L"PLANE:",6))&&fd_reference_token(s);
}
static int mc_hole_depth(const wchar_t *mode,double depth){
    return (!wcscmp(mode,L"THROUGH")&&depth==0)||(!wcscmp(mode,L"BLIND")&&depth>=0.01&&depth<=10000);
}
static int mc_iso_size(const wchar_t *name){
    const wchar_t *x=wcschr(name,L'x'),*p;
    wchar_t major[6];
    double diameter,pitch;
    size_t length;
    if(!x||name[0]!=L'M'||x<=name+1||x-name>6||wcslen(x+1)>6)return 0;
    for(p=name+1;p<x;p++)if((*p<L'0'||*p>L'9')&&*p!=L'.')return 0;
    length=(size_t)(x-name-1);
    wmemcpy(major,name+1,length);major[length]=0;
    return part_number(major,&diameter)&&diameter>0&&part_number(x+1,&pitch)&&pitch>0&&pitch<=20&&wcspbrk(x+1,L"+-eE")==NULL;
}
static int machining_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    double x,y,d,depth,outer,recess;int i,j,id;
    if(!mc_is_command(a[0]))return 0;
    if(!wcscmp(a[0],L"MACHINING_INFO"))return n==2?1:-1;
    if(n<5||!fd_name(a[2]))return -1;
    if(!wcscmp(a[0],L"SOLID_SHELL"))
        return n==5&&!wcsncmp(a[3],L"SURFACE:",8)&&part_integer(a[3]+8,&id)&&part_number(a[4],&d)&&d>=0.01&&d<=10000?1:-1;
    if(!wcsncmp(a[0],L"HOLE",4)){
        if(n<11||(wcsncmp(a[3],L"SURFACE:",8)&&wcscmp(a[0],L"HOLE"))||
           !mc_plane_token(a[3])||!mc_plane_token(a[4])||!mc_plane_token(a[6]))return -1;
        if(!wcscmp(a[3],a[4])||!wcscmp(a[3],a[6])||!wcscmp(a[4],a[6]))return -1;
        if(!part_number(a[5],&x)||!part_number(a[7],&y)||fabs(x)>1000000||fabs(y)>1000000)return -1;
        if(!wcscmp(a[0],L"HOLE_THREAD"))
            return n==12&&!wcscmp(a[8],L"ISO")&&mc_iso_size(a[9])&&part_number(a[10],&recess)&&
                part_number(a[11],&depth)&&recess>=0.01&&depth>recess&&depth<=10000?1:-1;
        if(!part_number(a[8],&d)||d<0.01||d>10000)return -1;
        if(!wcscmp(a[0],L"HOLE"))return n==11&&part_number(a[10],&depth)&&mc_hole_depth(a[9],depth)?1:-1;
        if(n!=13||!part_number(a[9],&outer)||outer<=d||outer>10000||!part_number(a[10],&recess)||
           !part_number(a[12],&depth)||!mc_hole_depth(a[11],depth))return -1;
        if(!wcscmp(a[0],L"HOLE_COUNTERSINK")){
            if(recess<30||recess>150)return -1;
            recess=(outer-d)/2/tan(recess*3.141592653589793/360);
        }
        return recess>=0.01&&recess<=10000&&(depth==0||recess<depth)?1:-1;
    }
    /* One constant size, 1..8 explicit solid edges, no tangent expansion. */
    if(n<5||n>12||!part_number(a[3],&d)||d<0.01||d>10000)return -1;
    for(i=4;i<n;i++){
        if(wcsncmp(a[i],L"EDGE:",5)||!part_integer(a[i]+5,&id))return -1;
        for(j=4;j<i;j++){int old;part_integer(a[j]+5,&old);if(old==id)return -1;}
    }
    return 1;
}
#endif
