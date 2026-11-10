#ifndef BWTL_FABRICATION_PLAN_H
#define BWTL_FABRICATION_PLAN_H
enum FabricationKind {FAB_NONE,FAB_BLANK,FAB_BENDS,FAB_HOLES,FAB_PAINT,FAB_DXF,FAB_DXF_CHECK,FAB_TEMPLATE,FAB_INSPECT,FAB_SORT,FAB_READBACK};

static int fab_kind(const wchar_t *verb){
    static const wchar_t *const names[]={L"FAB_BLANK",L"FAB_BENDS",L"FAB_HOLES",L"FAB_PAINT",L"FAB_DXF",
        L"FAB_DXF_CHECK",L"FAB_TEMPLATE",L"FAB_INSPECT",L"FAB_SORT",L"FAB_READBACK"};
    for(int i=0;i<_countof(names);i++)if(!wcscmp(verb,names[i]))return i+1;
    return FAB_NONE;
}

static int fab_request(const wchar_t *s){
    static const wchar_t *const words[]={L"FAB_",L"板料下料尺寸",L"折弯工序表",L"孔加工清单",L"喷涂面积清单",
        L"批量生成平板下料",L"批量下料DXF",L"下料DXF出图前检查",L"二维孔位模板",L"关键尺寸检验表",
        L"按料号与版本整理",L"导出后回读检查"};
    for(int i=0;i<_countof(words);i++)if(wcsstr(s,words[i]))return 1;
    return 0;
}

static int fab_dxf_leaf(const wchar_t *file){
    const wchar_t *dot=wcsrchr(file,L'.');
    wchar_t name[PART_TOKEN_MAX];
    if(!dot||dot==file||_wcsicmp(dot,L".dxf")||(size_t)(dot-file)>=_countof(name))return 0;
    wcsncpy_s(name,_countof(name),file,(size_t)(dot-file));
    return dv_safe_leaf(name);
}

static int fab_frame(const wchar_t *value){return !wcscmp(value,L"MODEL")||mp_named_ref(value,L"CSYS:");}

static int fabrication_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int kind=fab_kind(a[0]),count;
    if(!kind)return 0;
    if(n<3||!fd_name(a[1]))return -1;
    if(kind==FAB_DXF_CHECK)return n==3&&qa_surface_token(a[2])?1:-1;
    if(kind==FAB_READBACK){
        if(n==5&&!wcscmp(a[2],L"STEP"))
            return dv_exchange_file(a[3])==1&&qa_number(a[4],0.0001,5)?1:-1;
        return n==6&&!wcscmp(a[2],L"DXF")&&fab_dxf_leaf(a[3])&&qa_surface_token(a[4])&&qa_number(a[5],0.0001,5)?1:-1;
    }
    if(!dv_safe_leaf(a[2]))return -1;
    switch(kind){
    case FAB_BLANK:return n==4&&qa_surface_token(a[3])?1:-1;
    case FAB_BENDS:return n==3?1:-1;
    case FAB_HOLES:return n==4&&fab_frame(a[3])?1:-1;
    case FAB_PAINT:
        if(n==4&&!wcscmp(a[3],L"ALL"))return 1;
        if(n<5||n>20||wcscmp(a[3],L"FACES"))return -1;
        for(int i=4;i<n;i++)if(!qa_surface_token(a[i]))return -1;
        return 1;
    case FAB_DXF:
        if(n<4||n>14||n%2||!qa_surface_token(a[3]))return -1;
        for(int i=4;i<n;i+=2){
            if(!dv_safe_leaf(a[i])||!qa_surface_token(a[i+1])||!_wcsicmp(a[i],a[1]))return -1;
            for(int j=4;j<i;j+=2)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    case FAB_TEMPLATE:return n==5&&qa_surface_token(a[3])&&mp_named_ref(a[4],L"CSYS:")?1:-1;
    case FAB_INSPECT:
        if(n<8||!part_integer(a[3],&count)||count<1||count>7||n!=4+4*count)return -1;
        for(int i=4;i<n;i+=4){
            if(!fd_name(a[i])||!qa_number(a[i+1],-1e6,1e6)||!qa_number(a[i+2],0,1e4)||!qa_number(a[i+3],0,1e4))return -1;
            for(int j=4;j<i;j+=4)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    case FAB_SORT:return n<=8&&dv_distinct_models(a,3,n)?1:-1;
    default:return -1;
    }
}
#endif
