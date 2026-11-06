#ifndef BWTL_MACHINING_PLAN_H
#define BWTL_MACHINING_PLAN_H
/* Batch 1. Strict native protocol, independent of Creo and the desktop. */
static int mc_is_command(const wchar_t *v){
    return !wcscmp(v,L"HOLE")||!wcscmp(v,L"EDGE_ROUND")||!wcscmp(v,L"EDGE_CHAMFER")||!wcscmp(v,L"MACHINING_INFO");
}
static int mc_plane_token(const wchar_t *s){
    return (!wcsncmp(s,L"SURFACE:",8)||!wcsncmp(s,L"PLANE:",6))&&fd_reference_token(s);
}
static int machining_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    double x,y,d,depth;int i,j,id;
    if(!mc_is_command(a[0]))return 0;
    if(!wcscmp(a[0],L"MACHINING_INFO"))return n==2?1:-1;
    if(n<5||!fd_name(a[2]))return -1;
    if(!wcscmp(a[0],L"HOLE")){
        /* HOLE model name face ref1 distance1 ref2 distance2 dia mode depth */
        if(n!=11||!mc_plane_token(a[3])||!mc_plane_token(a[4])||!mc_plane_token(a[6]))return -1;
        if(!wcscmp(a[3],a[4])||!wcscmp(a[3],a[6])||!wcscmp(a[4],a[6]))return -1;
        if(!part_number(a[5],&x)||!part_number(a[7],&y)||fabs(x)>1000000||fabs(y)>1000000)return -1;
        if(!part_number(a[8],&d)||d<0.01||d>10000||!part_number(a[10],&depth))return -1;
        if(!wcscmp(a[9],L"THROUGH"))return depth==0?1:-1;
        if(!wcscmp(a[9],L"BLIND"))return depth>=0.01&&depth<=10000?1:-1;
        return -1;
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
