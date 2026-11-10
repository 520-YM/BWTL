#ifndef BWTL_DELIVERY_PLAN_H
#define BWTL_DELIVERY_PLAN_H

enum DeliveryCommand {
    DV_NONE,DV_REF,DV_REORDER,DV_REDEFINE,DV_IMPORT,DV_HEAL,
    DV_RECOGNIZE,DV_ATTR,DV_EXPORT,DV_REPORT,DV_PACKAGE
};

static int dv_kind(const wchar_t *verb){
    static const wchar_t *const names[]={
        L"DV_REF",L"DV_REORDER",L"DV_REDEFINE",L"DV_IMPORT",L"DV_HEAL",
        L"DV_RECOGNIZE",L"DV_ATTR",L"DV_EXPORT",L"DV_REPORT",L"DV_PACKAGE"
    };
    for(int i=0;i<_countof(names);i++)
        if(!wcscmp(verb,names[i]))return i+1;
    return DV_NONE;
}

static int dv_safe_leaf(const wchar_t *name){
    if(!part_name(name))return 0;
    static const wchar_t *const reserved[]={L"CON",L"PRN",L"AUX",L"NUL"};
    for(int i=0;i<_countof(reserved);i++)if(!_wcsicmp(name,reserved[i]))return 0;
    if(wcslen(name)==4&&(!_wcsnicmp(name,L"COM",3)||!_wcsnicmp(name,L"LPT",3))&&
       name[3]>=L'0'&&name[3]<=L'9')return 0;
    return 1;
}

/* A filename in the active Creo work directory. No relative path components. */
static int dv_exchange_file(const wchar_t *file){
    const wchar_t *dot=wcsrchr(file,L'.');
    size_t length=wcslen(file);
    if(!dot||dot==file||length>160||file[0]==L'.'||file[0]==L' '||dot[-1]==L' ')return 0;
    for(const wchar_t *p=file;*p;p++)
        if(*p<32||wcschr(L"\\/:*?\"<>|",*p))return 0;
    if(!_wcsicmp(dot,L".step")||!_wcsicmp(dot,L".stp"))return 1;
    if(!_wcsicmp(dot,L".iges")||!_wcsicmp(dot,L".igs"))return 2;
    return 0;
}

static int dv_distinct_models(wchar_t args[][PART_TOKEN_MAX],int first,int last){
    for(int i=first;i<last;i++){
        if(!dv_safe_leaf(args[i])||!_wcsicmp(args[1],args[i]))return 0;
        for(int j=first;j<i;j++)if(!_wcsicmp(args[j],args[i]))return 0;
    }
    return 1;
}

static int delivery_plan_valid(int count,wchar_t args[][PART_TOKEN_MAX]){
    int kind=dv_kind(args[0]),additional;
    if(!kind)return 0;
    switch(kind){
    case DV_REF:
        return count==5&&fd_name(args[2])&&
            (!wcscmp(args[3],L"MISSING")||qa_surface_token(args[3]))&&qa_plane_token(args[4])?1:-1;
    case DV_REORDER:
        return count==5&&fd_name(args[2])&&fd_name(args[4])&&_wcsicmp(args[2],args[4])&&
            (!wcscmp(args[3],L"BEFORE")||!wcscmp(args[3],L"AFTER"))?1:-1;
    case DV_REDEFINE:
        if(count!=6||!fd_name(args[2]))return -1;
        if(!wcscmp(args[3],L"OFFSET"))
            return qa_number(args[4],-1e6,1e6)&&qa_number(args[5],-1e6,1e6)&&
                fabs(wcstod(args[5],NULL))>=0.001?1:-1;
        return !wcscmp(args[3],L"DEPTH")&&qa_number(args[4],0.01,10000)&&
            qa_number(args[5],0.01,10000)?1:-1;
    case DV_IMPORT:
        return count==3&&dv_safe_leaf(args[1])&&dv_exchange_file(args[2])?1:-1;
    case DV_HEAL:
        return count==4&&dv_safe_leaf(args[2])&&_wcsicmp(args[1],args[2])&&dv_exchange_file(args[3])?1:-1;
    case DV_RECOGNIZE:
        return count==3&&(qa_ref(args[2],L"ID:")||fd_name(args[2]))?1:-1;
    case DV_ATTR:
        if(count<5||!part_integer(args[2],&additional)||additional>5||count<5+additional||
           (count-3-additional)%2||count-3-additional>12||!dv_distinct_models(args,3,3+additional))return -1;
        for(int i=3+additional;i<count;i+=2){
            if(!fd_name(args[i])||!args[i+1][0]||wcslen(args[i+1])>=80)return -1;
            for(int j=3+additional;j<i;j+=2)if(!_wcsicmp(args[i],args[j]))return -1;
        }
        return 1;
    case DV_EXPORT:
        return count>=4&&count<=9&&dv_safe_leaf(args[2])&&
            (!wcscmp(args[3],L"STEP")||!wcscmp(args[3],L"IGES")||!wcscmp(args[3],L"BOTH"))&&
            dv_distinct_models(args,4,count)?1:-1;
    case DV_REPORT:case DV_PACKAGE:
        return count==3&&dv_safe_leaf(args[2])?1:-1;
    default:
        return -1;
    }
}
#endif
