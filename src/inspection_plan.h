#ifndef BWTL_INSPECTION_PLAN_H
#define BWTL_INSPECTION_PLAN_H
enum InspectionKind {IN_NONE,IN_SEAT,IN_HIDE,IN_BOTTOM,IN_BEND,IN_TOOLSPACE,IN_VENT,IN_DEVICE,IN_SHARP,IN_RATIO,IN_CUTTER};

static int in_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"CHK_SEAT",L"CHK_HIDE",L"CHK_BOTTOM",L"CHK_BEND",L"CHK_TOOLSPACE",
        L"CHK_VENT",L"CHK_DEVICE",L"CHK_SHARP",L"CHK_RATIO",L"CHK_CUTTER"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return IN_NONE;
}

static int in_request(const wchar_t *s){
    static const wchar_t *const names[]={L"CHK_",L"螺钉头落位",L"沉孔藏头",L"盲孔剩余底厚",L"孔与折弯距离",
        L"折弯工具空间",L"散热开孔率",L"设备安装净空",L"边缘尖角",L"深孔长径比",L"刀具最小直径",L"最小刀具直径"};
    for(int i=0;i<_countof(names);i++)if(wcsstr(s,names[i]))return 1;
    return 0;
}

static int in_nonnegative(const wchar_t *s){return qa_number(s,0,10000);}

static int inspection_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int kind=in_kind(a[0]);
    if(!kind)return 0;
    if(n<4||!fd_name(a[1]))return -1;
    switch(kind){
    case IN_SEAT:return n==7&&qa_surface_token(a[2])&&qa_surface_token(a[3])&&
        mp_size(a[4])&&mp_size(a[5])&&in_nonnegative(a[6])?1:-1;
    case IN_HIDE:
        if(n!=8||!fd_name(a[2])||!mp_size(a[4])||!mp_size(a[5])||!in_nonnegative(a[7]))return -1;
        return (!wcscmp(a[3],L"BORE")&&qa_number(a[6],0,0))||
            (!wcscmp(a[3],L"SINK")&&qa_number(a[6],20,170))?1:-1;
    case IN_BOTTOM:case IN_RATIO:return n==4&&fd_name(a[2])&&mp_size(a[3])?1:-1;
    case IN_BEND:return n==6&&qa_surface_token(a[2])&&qa_surface_token(a[3])&&qa_edge_token(a[4])&&in_nonnegative(a[5])?1:-1;
    case IN_TOOLSPACE:case IN_DEVICE:{
        int start=kind==IN_TOOLSPACE?3:2;
        if(n!=start+8||(kind==IN_TOOLSPACE&&!qa_edge_token(a[2]))||!mp_named_ref(a[start],L"CSYS:"))return -1;
        for(int i=1;i<=3;i++)if(!mp_coord(a[start+i]))return -1;
        for(int i=4;i<=6;i++)if(!mp_size(a[start+i]))return -1;
        return in_nonnegative(a[start+7])?1:-1;
    }
    case IN_VENT:return n==5&&qa_surface_token(a[2])&&qa_surface_token(a[3])&&qa_number(a[4],0,100)?1:-1;
    case IN_SHARP:
        if(n<4||n>11||!qa_number(a[2],1,179))return -1;
        for(int i=3;i<n;i++)if(!qa_edge_token(a[i]))return -1;
        return 1;
    case IN_CUTTER:
        if(n==6&&!wcscmp(a[2],L"CYL"))
            return qa_surface_token(a[3])&&mp_size(a[4])&&in_nonnegative(a[5])?1:-1;
        return n==7&&!wcscmp(a[2],L"SLOT")&&qa_surface_token(a[3])&&qa_surface_token(a[4])&&
            mp_size(a[5])&&in_nonnegative(a[6])?1:-1;
    default:return -1;
    }
}
#endif
