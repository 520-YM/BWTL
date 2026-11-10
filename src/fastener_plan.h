#ifndef BWTL_FASTENER_PLAN_H
#define BWTL_FASTENER_PLAN_H
enum FastenerKind {
    FX_NONE,FX_KEY,FX_SLOT,FX_CORNERS,FX_PCD,FX_HINGE,FX_PRESS,FX_RIVET,FX_GROUND,FX_TIE,FX_TAB
};

static int fx_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={L"FIX_KEY",L"FIX_SLOT",L"FIX_CORNERS",L"FIX_PCD",L"FIX_HINGE",
        L"FIX_PRESS",L"FIX_RIVET",L"FIX_GROUND",L"FIX_TIE",L"FIX_TAB"};
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return i+1;
    return FX_NONE;
}
static int fx_request(const wchar_t *text){
    static const wchar_t *const terms[]={L"FIX_",L"钥匙孔挂槽",L"长槽调节孔",L"四角安装孔",L"分度孔组",
        L"铰链安装孔",L"压铆螺母底孔",L"拉铆螺母底孔",L"接地端子安装",L"扎带固定槽",L"滑入式卡舌"};
    for(int i=0;i<_countof(terms);i++)if(wcsstr(text,terms[i]))return 1;
    return 0;
}

static int fx_number(const wchar_t *value){return qa_number(value,0.1,10000);}
static int fx_nonnegative(const wchar_t *value){return qa_number(value,0,10000);}
static int fx_angle(const wchar_t *value){return qa_number(value,-360,360);}

static int fastener_plan_valid(int count,wchar_t a[][PART_TOKEN_MAX]){
    int kind=fx_kind(a[0]),face;
    if(!kind)return 0;
    if(count<11||!fd_name(a[1])||!fd_name(a[2])||wcslen(a[2])>23||
       wcsncmp(a[3],L"SURFACE:",8)||!part_integer(a[3]+8,&face)||face<=0||
       wcsncmp(a[4],L"CSYS:",5)||!fd_name(a[4]+5)||wcsncmp(a[5],L"PLANE:",6)||!fd_name(a[5]+6)||
       !qa_number(a[6],-10000,10000)||!qa_number(a[7],-10000,10000)||!qa_number(a[8],0.1,1000))return -1;
    switch(kind){
    case FX_KEY:
        return count==13&&fx_number(a[9])&&fx_number(a[10])&&fx_number(a[11])&&fx_angle(a[12])&&
            wcstod(a[9],NULL)>wcstod(a[10],NULL)+0.1&&wcstod(a[11],NULL)>=wcstod(a[9],NULL)/2+0.1?1:-1;
    case FX_SLOT:
        return count==13&&fx_number(a[9])&&fx_nonnegative(a[10])&&fx_number(a[11])&&fx_angle(a[12])&&
            wcstod(a[9],NULL)+2*wcstod(a[10],NULL)+wcstod(a[11],NULL)<=10000?1:-1;
    case FX_CORNERS:
        if(!wcscmp(a[9],L"CENTER"))return count==14&&fx_number(a[10])&&fx_number(a[11])&&fx_number(a[12])&&fx_number(a[13])?1:-1;
        if(wcscmp(a[9],L"EDGE")||count!=16)return -1;
        for(int i=10;i<16;i++)if(!fx_number(a[i]))return -1;
        return wcstod(a[10],NULL)>2*wcstod(a[12],NULL)&&wcstod(a[11],NULL)>2*wcstod(a[13],NULL)?1:-1;
    case FX_PCD:{
        int holes,skip,mask=0;
        if(count<14||!fx_number(a[9])||!part_integer(a[10],&holes)||holes<3||holes>12||
           !fx_angle(a[11])||!fx_number(a[12])||!part_integer(a[13],&skip)||skip>holes-2||count!=14+skip)return -1;
        for(int i=14;i<count;i++){
            int index;
            if(!part_integer(a[i],&index)||index<1||index>holes||(mask&(1<<(index-1))))return -1;
            mask|=1<<(index-1);
        }
        return 1;
    }
    case FX_HINGE:{
        int rows,edge;
        return count==15&&!wcsncmp(a[9],L"EDGE:",5)&&part_integer(a[9]+5,&edge)&&edge>0&&
            fx_nonnegative(a[10])&&fx_number(a[11])&&part_integer(a[12],&rows)&&rows>=2&&rows<=6&&
            fx_number(a[13])&&fx_number(a[14])?1:-1;
    }
    case FX_PRESS:
        return count==13&&fx_number(a[9])&&fx_number(a[10])&&fx_number(a[11])&&
            wcstod(a[11],NULL)>=wcstod(a[10],NULL)&&fx_number(a[12])?1:-1;
    case FX_RIVET:
        return count==15&&(!wcscmp(a[9],L"ROUND")||!wcscmp(a[9],L"HEX"))&&
            fx_number(a[10])&&fx_number(a[11])&&fx_number(a[12])&&wcstod(a[12],NULL)>=wcstod(a[11],NULL)&&
            fx_number(a[13])&&fx_angle(a[14])&&(wcscmp(a[9],L"ROUND")||wcstod(a[14],NULL)==0)?1:-1;
    case FX_GROUND:
        return count==11&&fx_number(a[9])&&fx_number(a[10])&&wcstod(a[10],NULL)>=wcstod(a[9],NULL)+0.2?1:-1;
    case FX_TIE:
        return count==14&&fx_number(a[9])&&fx_number(a[10])&&wcstod(a[9],NULL)>=wcstod(a[10],NULL)+0.1&&
            fx_number(a[11])&&fx_angle(a[12])&&fx_number(a[13])&&wcstod(a[11],NULL)>=wcstod(a[10],NULL)+wcstod(a[13],NULL)?1:-1;
    case FX_TAB:
        return count==13&&fx_number(a[9])&&fx_number(a[10])&&wcstod(a[9],NULL)>=2*wcstod(a[10],NULL)+0.1&&
            fx_number(a[11])&&fx_number(a[12])&&wcstod(a[12],NULL)>=wcstod(a[10],NULL)+0.1?1:-1;
    default:return -1;
    }
}
#endif
