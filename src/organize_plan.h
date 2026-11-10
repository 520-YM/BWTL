#ifndef BWTL_ORGANIZE_PLAN_H
#define BWTL_ORGANIZE_PLAN_H
enum OrganizeKind {ORG_NONE,ORG_HOLES,ORG_SPECS,ORG_SLOTS,ORG_PLANES,ORG_RENAME,ORG_UNUSED,ORG_STATUS,ORG_OVERLAP,ORG_PARAMS,ORG_IMPACT};

static int org_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"ORG_HOLES",L"ORG_SPECS",L"ORG_SLOTS",L"ORG_PLANES",L"ORG_RENAME",
        L"ORG_UNUSED",L"ORG_STATUS",L"ORG_OVERLAP",L"ORG_PARAMS",L"ORG_IMPACT"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return ORG_NONE;
}

static int org_rename_request(const wchar_t *s){
    return wcsstr(s,L"批量重命名特征")||wcsstr(s,L"批量改特征名")||wcsstr(s,L"ORG_RENAME");
}

static int org_request(const wchar_t *s){
    static const wchar_t *const names[]={L"ORG_",L"孔位坐标表",L"归类孔规格",L"孔规格归类",L"导入件槽口",L"提取安装平面",
        L"批量重命名特征",L"批量改特征名",L"未被引用的基准",L"未引用基准",L"失败与抑制特征",
        L"查找失败特征",L"列出失败特征",L"查找抑制特征",L"列出抑制特征",
        L"重叠孔位",L"重复孔位",L"版本参数对照",L"改模影响预检"};
    for(int i=0;i<_countof(names);i++)if(wcsstr(s,names[i]))return 1;
    return 0;
}

static int org_ref(const wchar_t *s){return fd_name(s)||qa_ref(s,L"ID:");}

static int organize_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int kind=org_kind(a[0]),count;
    if(!kind)return 0;
    if(n<2||!fd_name(a[1]))return -1;
    switch(kind){
    case ORG_HOLES:return n==4&&(!wcscmp(a[2],L"MODEL")||mp_named_ref(a[2],L"CSYS:"))&&vt_integer(a[3],100)?1:-1;
    case ORG_SPECS:return n==4&&qa_number(a[2],0.0001,10)&&qa_number(a[3],0.0001,100)?1:-1;
    case ORG_SLOTS:return n==6&&org_ref(a[2])&&qa_surface_token(a[3])&&qa_surface_token(a[4])&&qa_number(a[5],1.1,100)?1:-1;
    case ORG_PLANES:
        if(n!=5||!qa_number(a[3],0,1e10)||!vt_integer(a[4],24))return -1;
        return !wcscmp(a[2],L"ALL")||((a[2][0]==L'+'||a[2][0]==L'-')&&qa_axis(a[2]+1))?1:-1;
    case ORG_RENAME:
        if(n<6||!part_integer(a[2],&count)||count<1||count>8||n!=3+3*count)return -1;
        for(int i=3;i<n;i+=3){
            if(!qa_ref(a[i],L"ID:")||!fd_name(a[i+1])||!fd_name(a[i+2])||!_wcsicmp(a[i+1],a[i+2]))return -1;
        }
        return 1;
    case ORG_UNUSED:case ORG_STATUS:return n==2?1:-1;
    case ORG_OVERLAP:return n==4&&qa_number(a[2],0.0001,10)&&vt_integer(a[3],64)?1:-1;
    case ORG_PARAMS:return n==3&&fd_name(a[2])&&_wcsicmp(a[1],a[2])?1:-1;
    case ORG_IMPACT:return n==5&&org_ref(a[2])&&(!wcscmp(a[3],L"EDIT")||!wcscmp(a[3],L"SUPPRESS")||!wcscmp(a[3],L"DELETE"))&&vt_integer(a[4],5)?1:-1;
    default:return -1;
    }
}
#endif
