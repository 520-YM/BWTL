#ifndef BWTL_ADVANCED_SKETCH_PLAN_H
#define BWTL_ADVANCED_SKETCH_PLAN_H

#define AS_LIMIT 16
enum { AS_ELLIPSE=1,AS_SLOT,AS_POLYGON,AS_SPLINE };
enum { AS_OFFSET=1,AS_MIRROR,AS_TRIM,AS_CORNER,AS_CHAIN,AS_CHECK,AS_REPAIR };

static int as_shape_kind(const wchar_t *name){
    if(!wcscmp(name,L"ELLIPSE"))return AS_ELLIPSE;
    if(!wcscmp(name,L"SLOT"))return AS_SLOT;
    if(!wcscmp(name,L"POLYGON"))return AS_POLYGON;
    if(!wcscmp(name,L"SPLINE"))return AS_SPLINE;
    return 0;
}

static int as_value(const wchar_t *token,double *value){
    return part_number(token,value)&&fabs(*value)<=1000000;
}

static int as_shape_valid(int count,wchar_t args[][PART_TOKEN_MAX],int start,int kind){
    double value[6];
    int i,edges;
    const int first=start+1,quantity=count-first;
    if(kind==AS_SPLINE){
        if(quantity<7||quantity>17||!(quantity%2))return 0;
        if(wcscmp(args[first],L"THROUGH")&&wcscmp(args[first],L"CONTROL"))return 0;
        for(i=first+1;i<count;i++)if(!as_value(args[i],value))return 0;
        return 1;
    }
    if(quantity!=(kind==AS_POLYGON?6:5))return 0;
    for(i=0;i<quantity;i++){
        if(kind==AS_POLYGON&&i==3){if(wcscmp(args[first+i],L"VERTEX")&&wcscmp(args[first+i],L"SIDE"))return 0;}
        else if(!as_value(args[first+i],&value[i]))return 0;
    }
    if(kind==AS_POLYGON)return part_integer(args[first],&edges)&&edges>=3&&edges<=AS_LIMIT&&value[4]>=0.01&&fabs(value[5])<=360;
    if(fabs(value[4])>360||value[2]<0.01)return 0;
    if(value[3]<0.01)return 0;
    return kind==AS_SLOT?value[2]-value[3]>=0.01:value[2]>=value[3];
}

static int as_edit_kind(const wchar_t *name){
    const wchar_t *verbs[]={L"SKETCH_OFFSET",L"SKETCH_MIRROR",L"SKETCH_TRIM_EXTEND",L"SKETCH_CORNER",L"SKETCH_PROJECT_CHAIN",L"SKETCH_CHECK",L"SKETCH_REPAIR"};
    int i;
    for(i=0;i<_countof(verbs);i++)if(!wcscmp(name,verbs[i]))return i+1;
    return 0;
}

static int as_ids(int first,int n,wchar_t a[][PART_TOKEN_MAX]){
    int i,j,ids[AS_LIMIT];
    if(n<=first||n-first>AS_LIMIT)return 0;
    for(i=first;i<n;i++){
        if(!part_integer(a[i],&ids[i-first]))return 0;
        for(j=0;j<i-first;j++)if(ids[j]==ids[i-first])return 0;
    }
    return 1;
}

static int as_edit_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    double x;
    int kind=as_edit_kind(a[0]);
    if(!kind)return 0;
    if(n<3||!fd_name(a[2]))return -1;
    switch(kind){
    case AS_OFFSET:
        return n>=5&&as_value(a[3],&x)&&fabs(x)>=0.01&&as_ids(4,n,a)?1:-1;
    case AS_MIRROR:return as_ids(3,n,a)&&n>=5?1:-1;
    case AS_TRIM:
        return n==7&&(!wcscmp(a[3],L"TRIM")||!wcscmp(a[3],L"EXTEND"))&&
            (!wcscmp(a[4],L"START")||!wcscmp(a[4],L"END"))&&as_ids(5,n,a)?1:-1;
    case AS_CORNER:
        return n==7&&(!wcscmp(a[3],L"ROUND")||!wcscmp(a[3],L"CHAMFER"))&&
            as_value(a[4],&x)&&x>=0.01&&as_ids(5,n,a)?1:-1;
    case AS_CHAIN:{
        int edges[AS_LIMIT];
        if(n==4&&!wcsncmp(a[3],L"CURVE:",6))return fd_name(a[3]+6)?1:-1;
        if(n<5||n-3>AS_LIMIT)return -1;
        for(int i=3;i<n;i++){
            if(wcsncmp(a[i],L"EDGE:",5)||!part_integer(a[i]+5,&edges[i-3])||edges[i-3]<=0)return -1;
            for(int j=3;j<i;j++)if(edges[j-3]==edges[i-3])return -1;
        }
        return 1;
    }
    case AS_CHECK:return n==3?1:-1;
    case AS_REPAIR:return n==4&&as_value(a[3],&x)&&x>=0.001&&x<=0.25?1:-1;
    }
    return -1;
}
#endif
