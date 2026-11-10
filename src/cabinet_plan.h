#ifndef BWTL_CABINET_PLAN_H
#define BWTL_CABINET_PLAN_H

enum CabinetKind {
    BWTL_CM_NONE,BWTL_CM_SEGMENTS,BWTL_CM_TRAY,BWTL_CM_U,BWTL_CM_Z,BWTL_CM_GAP,BWTL_CM_END,BWTL_CM_RETURN,BWTL_CM_EAR,BWTL_CM_COLLAR,BWTL_CM_BENDS
};

static int cm_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"CASE_SEG",L"CASE_TRAY",L"CASE_U",L"CASE_Z",L"CASE_GAP",
        L"CASE_END",L"CASE_RETURN",L"CASE_EAR",L"CASE_COLLAR",L"CASE_BENDS"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return BWTL_CM_NONE;
}

static int cm_request(const wchar_t *text){
    static const wchar_t *const terms[]={L"CASE_",L"多段翻边",L"翻边托盘",L"U形机箱",L"U 形机箱",L"钣金错台",
        L"翻边留缝",L"翻边端部",L"门板回折",L"板边挂耳",L"圆形翻孔",L"折弯改规格",L"调整折弯"};
    for(int i=0;i<_countof(terms);i++)if(wcsstr(text,terms[i]))return 1;
    return 0;
}

static int cm_positive(const wchar_t *value){return qa_number(value,0.1,10000);}
static int cm_radius(const wchar_t *value){return qa_number(value,0.1,1000);}
static int cm_end_side(const wchar_t *value){return !wcscmp(value,L"START")||!wcscmp(value,L"END");}

static int cabinet_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    int k=cm_kind(a[0]),count;
    if(!k)return 0;
    if(n<4||!fd_name(a[1]))return -1;
    if(k==BWTL_CM_BENDS){
        if(!part_integer(a[2],&count)||count<1||count>4||n!=3+5*count)return -1;
        for(int i=3;i<n;i+=5){
            if(!fd_name(a[i])||!sh_angle(a[i+1])||!cm_radius(a[i+2])||!sh_angle(a[i+3])||!cm_radius(a[i+4]))return -1;
            for(int j=3;j<i;j+=5)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    }
    if(!fd_name(a[2])||((k!=BWTL_CM_END&&k!=BWTL_CM_GAP)&&wcslen(a[2])>22))return -1;
    if(k==BWTL_CM_TRAY||k==BWTL_CM_U){
        if(n!=(k==BWTL_CM_TRAY?12:11)||!ad_plane(a[3])||!ad_plane(a[4])||
           !cm_positive(a[5])||!cm_positive(a[6])||!cm_radius(a[7])||!cm_positive(a[8])||
           !cm_radius(a[9])||!sx_side(a[n-1]))return -1;
        return k==BWTL_CM_U||cm_positive(a[10])?1:-1;
    }
    if(k==BWTL_CM_GAP)return n==7&&sf_id(a[3],L"EDGE:")&&sf_id(a[4],L"SURFACE:")&&cm_positive(a[5])&&cm_positive(a[6])?1:-1;
    if(k==BWTL_CM_END)return n==7&&sf_id(a[3],L"EDGE:")&&cm_end_side(a[4])&&cm_positive(a[5])&&cm_positive(a[6])?1:-1;
    if(!sf_id(a[3],L"EDGE:"))return -1;
    if(k==BWTL_CM_SEGMENTS){
        if(n<13||!cm_positive(a[4])||!sh_angle(a[5])||!cm_radius(a[6])||!sx_side(a[7])||
           !part_integer(a[8],&count)||count<2||count>5||n!=9+2*count)return -1;
        double last=-1;
        for(int i=9;i<n;i+=2){
            if(!qa_number(a[i],0,10000)||!cm_positive(a[i+1]))return -1;
            double from=wcstod(a[i],NULL),to=wcstod(a[i+1],NULL);
            if(from<=last||to-from<0.1)return -1;
            last=to;
        }
        return 1;
    }
    if(k==BWTL_CM_Z)return n==8&&cm_positive(a[4])&&cm_positive(a[5])&&cm_radius(a[6])&&sx_side(a[7])?1:-1;
    if(k==BWTL_CM_RETURN)return n==8&&sf_id(a[4],L"SURFACE:")&&cm_positive(a[5])&&cm_radius(a[6])&&sx_side(a[7])?1:-1;
    if(k==BWTL_CM_EAR)return n==10&&qa_number(a[4],0,10000)&&cm_positive(a[5])&&wcstod(a[5],NULL)-wcstod(a[4],NULL)>=0.1&&
        cm_positive(a[6])&&cm_radius(a[7])&&sx_side(a[8])&&qa_number(a[9],0,1000)?1:-1;
    if(k==BWTL_CM_COLLAR)return n==7&&cm_positive(a[4])&&cm_radius(a[5])&&sx_side(a[6])?1:-1;
    return -1;
}
#endif
