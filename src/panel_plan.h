#ifndef BWTL_PANEL_PLAN_H
#define BWTL_PANEL_PLAN_H

enum PanelKind {
    PN_NONE,PN_BUTTON,PN_ESTOP,PN_POWER,PN_PORT,PN_FAN,PN_DISPLAY,
    PN_SEAT,PN_HANDLE,PN_LOCK,PN_COMB
};

static int pn_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"PN_BUTTON",L"PN_ESTOP",L"PN_POWER",L"PN_PORT",L"PN_FAN",
        L"PN_DISPLAY",L"PN_SEAT",L"PN_HANDLE",L"PN_LOCK",L"PN_COMB"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return PN_NONE;
}

static int pn_request(const wchar_t *text){
    static const wchar_t *const terms[]={L"PN_",L"按钮开孔",L"按钮孔",L"急停孔",L"插座开孔",L"接口开孔",L"网口开孔",
        L"风扇安装孔",L"显示屏窗口",L"透明面板沉位",L"把手安装孔",L"锁具安装孔",L"走线梳齿"};
    for(int i=0;i<_countof(terms);i++)if(wcsstr(text,terms[i]))return 1;
    return 0;
}

static int panel_plan_valid(int count,wchar_t args[][PART_TOKEN_MAX]){
    static const int counts[]={0,12,12,13,12,13,14,14,12,13,13};
    int kind=pn_kind(args[0]);
    if(!kind)return 0;
    if(count!=counts[kind]||!fd_name(args[1])||!fd_name(args[2])||wcslen(args[2])>24||
       wcsncmp(args[3],L"SURFACE:",8)||!fd_reference_token(args[3])||
       wcsncmp(args[4],L"CSYS:",5)||!fd_name(args[4]+5)||
       wcsncmp(args[5],L"PLANE:",6)||!fd_name(args[5]+6)||
       !qa_number(args[6],-10000,10000)||!qa_number(args[7],-10000,10000)||
       !qa_number(args[8],0.1,1000))return -1;
    double value[5]={0};
    for(int i=9;i<count;i++){
        if(!qa_number(args[i],0,10000))return -1;
        value[i-9]=wcstod(args[i],NULL);
    }
    double a=value[0],b=value[1],c=value[2],d=value[3],e=value[4];
    if(a<0.1)return -1;
    switch(kind){
    case PN_BUTTON:case PN_ESTOP:
        return ((!b&&!c&&kind==PN_BUTTON)||(b>=0.1&&b<a&&c>=0.1))?1:-1;
    case PN_POWER:return b>=0.1&&c>=0.1&&d>=0.1?1:-1;
    case PN_PORT:return b>=0.1&&(!c||(c>=0.1&&2*c<fmin(a,b)))?1:-1;
    case PN_FAN:return b>=0.1&&c>=0.1&&d>=0.1?1:-1;
    case PN_DISPLAY:return b>=0.1&&e>=0.1&&c>=a+2*e&&d>=b+2*e?1:-1;
    case PN_SEAT:return b>=0.1&&c>=a+0.2&&d>=b+0.2&&e>=0.1&&e+0.1<=wcstod(args[8],NULL)?1:-1;
    case PN_HANDLE:return b>=0.1&&(c==0||c>=0.1)?1:-1;
    case PN_LOCK:return b>=0.1&&b<a/2&&((!c&&!d)||(c>=0.1&&d>=0.1))?1:-1;
    case PN_COMB:return a>=2&&a<=12&&a==(int)a&&b>=0.1&&c>b+0.05&&d>=0.1?1:-1;
    default:return -1;
    }
}
#endif
