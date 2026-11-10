#ifndef BWTL_REVISION_PLAN_H
#define BWTL_REVISION_PLAN_H
enum RevisionKind {RV_NONE,RV_SIZE,RV_SHIFT,RV_PITCH,RV_OPEN,RV_DIAM,RV_DEPTH,RV_RECESS,RV_ROUND,RV_THICK,RV_COPY};

static int rv_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"REV_SIZE",L"REV_SHIFT",L"REV_PITCH",L"REV_OPEN",L"REV_DIAM",
        L"REV_DEPTH",L"REV_RECESS",L"REV_ROUND",L"REV_THICK",L"REV_COPY"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return RV_NONE;
}

static int rv_request(const wchar_t *text){
    static const wchar_t *const words[]={L"REV_",L"按最终外形",L"按最终尺寸",L"改长宽高",L"孔组整体平移",
        L"孔组换孔距",L"开口换规格",L"孔批量改径",L"批量改孔径",L"盲孔批量改深",L"沉孔批量换规格",
        L"圆角半径批量统一",L"批量统一圆角",L"钣金换板厚",L"改型副本"};
    for(int i=0;i<_countof(words);i++)if(wcsstr(text,words[i]))return 1;
    return 0;
}

static int rv_list(int n,wchar_t a[][PART_TOKEN_MAX],int first,int stride){
    int count;
    if(first>=n||!part_integer(a[first],&count)||count<1||count>8||n!=first+1+stride*count)return 0;
    for(int i=first+1;i<n;i+=stride){
        if(!fd_name(a[i])||(stride==2&&!mp_size(a[i+1])))return 0;
        for(int j=first+1;j<i;j+=stride)if(!_wcsicmp(a[i],a[j]))return 0;
    }
    return 1;
}

static int rv_drivers(int n,wchar_t a[][PART_TOKEN_MAX],int start){
    int count;
    if(start>=n||!part_integer(a[start],&count)||count<1||n!=start+1+3*count)return 0;
    for(int i=start+1;i<n;i+=3)
        if(!fd_name(a[i])||!mp_size(a[i+1])||!mp_size(a[i+2]))return 0;
    return 1;
}

static int rv_outline_args(int n,wchar_t a[][PART_TOKEN_MAX],int start){
    if(n<start+10)return 0;
    for(int i=start;i<start+6;i++)if(!mp_size(a[i]))return 0;
    return rv_drivers(n,a,start+6);
}

static int revision_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int k=rv_kind(a[0]);
    if(!k)return 0;
    if(n<3||!fd_name(a[1]))return -1;
    switch(k){
    case RV_SIZE:return rv_outline_args(n,a,2)?1:-1;
    case RV_COPY:return fd_name(a[2])&&_wcsicmp(a[1],a[2])&&(n==3||rv_outline_args(n,a,3))?1:-1;
    case RV_SHIFT:return n>=6&&mp_coord(a[2])&&mp_coord(a[3])&&rv_list(n,a,4,1)?1:-1;
    case RV_PITCH:
        if(n!=8||!mp_size(a[2])||!mp_size(a[3]))return -1;
        for(int i=4;i<8;i++){
            if(!fd_name(a[i]))return -1;
            for(int j=4;j<i;j++)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    case RV_OPEN:
        if(n<8||!fd_name(a[2])||!fd_name(a[3])||_wcsicmp(a[2],a[3])==0)return -1;
        if(n==8&&wcscmp(a[4],L"CIRCLE")==0)return fd_name(a[5])&&mp_size(a[6])&&mp_size(a[7])?1:-1;
        if(n!=11||wcscmp(a[4],L"RECT"))return -1;
        return fd_name(a[5])&&fd_name(a[8])&&mp_size(a[6])&&mp_size(a[7])&&mp_size(a[9])&&mp_size(a[10])?1:-1;
    case RV_DIAM:case RV_DEPTH:return n>=6&&mp_size(a[2])&&mp_size(a[3])&&rv_list(n,a,4,1)?1:-1;
    case RV_RECESS:
        if(n<9||!mp_size(a[3])||!mp_size(a[4])||!rv_list(n,a,7,1))return -1;
        if(!wcscmp(a[2],L"BORE"))return mp_size(a[5])&&mp_size(a[6])?1:-1;
        return !wcscmp(a[2],L"SINK")&&qa_number(a[5],20,170)&&qa_number(a[6],20,170)?1:-1;
    case RV_ROUND:return n>=6&&mp_size(a[2])&&rv_list(n,a,3,2)?1:-1;
    case RV_THICK:return n==4&&mp_size(a[2])&&mp_size(a[3])?1:-1;
    default:return -1;
    }
}
#endif
