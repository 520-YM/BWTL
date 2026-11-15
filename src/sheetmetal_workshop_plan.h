#ifndef BWTL_SHEETMETAL_WORKSHOP_PLAN_H
#define BWTL_SHEETMETAL_WORKSHOP_PLAN_H

enum {
    SMY_FORM_RECESS=393,SMY_FORM_BOSS,SMY_FORM_LIBRARY=398,SMY_FORM_CLEARANCE,SMY_FORM_FLAT,
    SMY_PRESS_CHECK=401,SMY_PRESS_STUD,SMY_PRESS_ACCESS,SMY_RIVET_GRIP,SMY_RIVET_ALIGN,SMY_DRIVER_ACCESS,SMY_SPOT_WELDS,SMY_PLUG_SLOT,SMY_WELD_SCHEDULE,SMY_JOIN_CARD,
    SMY_CASE_SPACE=411,SMY_CASE_FLUSH=413,SMY_CASE_SEAL,SMY_CASE_DISPLAY,SMY_VENT_INTERFACE,SMY_CABLE_ENTRY=418,SMY_MOUNT_INSPECTION,
    SMY_DFM_FLANGE=421,SMY_DFM_SHORT,SMY_FORM_INTERACTION,SMY_GAUGE_FACE=425,SMY_TOOL_WITHDRAWAL,SMY_FINISH_PROTECTION,SMY_COATING_POINT,SMY_MIRROR_PAIR,
    SMY_FLAT_COMPARE=431,SMY_FLAT_TOPOLOGY,SMY_FLAT_HOLE_MAP=434,SMY_FLAT_BEND_LAYERS,SMY_FLAT_DATUM=437,SMY_FLAT_CLEAN,SMY_FLAT_MATCH,SMY_FLAT_LOCK,
    SMY_BEND_NUMBER=441,SMY_BEND_ORDER,SMY_BEND_CANDIDATES,
    SMY_BEND_TOOL=445,SMY_BEND_FORCE=447,SMY_BEND_TRIAL,SMY_BEND_SEGMENTS,SMY_BEND_CARD,
    SMY_STOCK_MEASURE=451,SMY_STOCK_GROUP,SMY_STOCK_FIT,SMY_STOCK_GRID,SMY_STOCK_PACK,
    SMY_STOCK_HAND,SMY_STOCK_AUDIT,SMY_STOCK_COMMON,SMY_STOCK_REMNANT,SMY_STOCK_TOTAL,
    SMY_DRAW_TEMPLATE=461,SMY_FIRST_CARD=466,SMY_GAUGE_ACCESS,SMY_VERSION_COMPARE,SMY_DELIVERY_CHECK,SMY_DELIVERY_BUNDLE
};

static int smy_kind(const wchar_t *verb){
#define SMY_GROUP(title)
#define SMY_FEATURE(kind,title,parameters,note) if(!wcscmp(verb,L"" #kind))return kind;
#include "sheetmetal_workshop_catalog.inc"
#undef SMY_FEATURE
#undef SMY_GROUP
    return 0;
}

static int dv_safe_leaf(const wchar_t *name);
static int mp_named_ref(const wchar_t *value,const wchar_t *prefix);
static int smy_vent_kind(const wchar_t *value){
    return !wcscmp(value,L"FAN")||!wcscmp(value,L"FILTER")||!wcscmp(value,L"HOOD");
}
static int smy_mount_inspection_role(const wchar_t *value){
    return !wcscmp(value,L"WALL")||!wcscmp(value,L"HANG")||!wcscmp(value,L"SUPPORT");
}

static int smy_workshop_input(int n,wchar_t a[][PART_TOKEN_MAX],int kind){
    switch(kind){
    case SMY_FORM_RECESS:case SMY_FORM_BOSS:
        return n==13&&fd_name(a[2])&&sf_id(a[3],L"SURFACE:")&&mp_named_ref(a[4],L"CSYS:")&&mp_named_ref(a[5],L"PLANE:")&&
            (!wcscmp(a[6],L"ROUND")||!wcscmp(a[6],L"RECT"))&&smx_number(a[7],-1e5,1e5)&&smx_number(a[8],-1e5,1e5)&&
            smx_number(a[9],0.1,10000)&&smx_number(a[10],0,10000)&&smx_number(a[11],0.05,1000)&&smx_number(a[12],0.01,1000)?1:-1;
    case SMY_FORM_LIBRARY:
        return n==6&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&(!wcscmp(a[4],L"IN")||!wcscmp(a[4],L"OUT"))&&smx_number(a[5],0.01,1000)?1:-1;
    case SMY_FORM_FLAT:
        return n==6&&dv_safe_leaf(a[2])&&sf_id(a[3],L"SURFACE:")&&fd_name(a[4])&&
            (!wcscmp(a[5],L"FLAT")||!wcscmp(a[5],L"CUTS"))?1:-1;
    case SMY_FORM_CLEARANCE:
        return n==12&&sf_id(a[2],L"SURFACE:")&&mp_named_ref(a[3],L"CSYS:")&&mp_named_ref(a[4],L"PLANE:")&&
            smx_number(a[5],-1e5,1e5)&&smx_number(a[6],-1e5,1e5)&&smx_number(a[7],0.01,10000)&&
            smx_number(a[8],0.01,10000)&&smx_number(a[9],0.01,10000)&&smx_number(a[10],0.01,10000)&&
            smx_number(a[11],0,1000)?1:-1;
    case SMY_PRESS_STUD:
        return n==12&&fd_name(a[2])&&wcslen(a[2])<=22&&sf_id(a[3],L"SURFACE:")&&mp_named_ref(a[4],L"CSYS:")&&
            mp_named_ref(a[5],L"PLANE:")&&dv_safe_leaf(a[6])&&dv_safe_leaf(a[7])&&
            smx_number(a[8],-1e5,1e5)&&smx_number(a[9],-1e5,1e5)&&smx_number(a[10],0.01,1000)&&smx_number(a[11],0,1000)?1:-1;
    case SMY_PRESS_ACCESS:case SMY_DRIVER_ACCESS:case SMY_GAUGE_ACCESS:
        if(n!=(kind==SMY_PRESS_ACCESS?9:8)||!sf_id(a[2],L"SURFACE:")||!sf_id(a[3],L"SURFACE:"))return -1;
        for(int i=4;i<n;i++)if(!smx_number(a[i],i==n-1?0:0.01,1000))return -1;
        return 1;
    case SMY_RIVET_ALIGN:
        if(n!=14||!fd_name(a[2])||!_wcsicmp(a[1],a[2])||!mp_named_ref(a[3],L"CSYS:")||!mp_named_ref(a[6],L"CSYS:")||
           !sf_id(a[4],L"SURFACE:")||!sf_id(a[5],L"SURFACE:")||!sf_id(a[7],L"SURFACE:")||!sf_id(a[8],L"SURFACE:"))return -1;
        for(int i=9;i<n;i++)if(!smx_number(a[i],0.001,1000))return -1;
        return 1;
    case SMY_PRESS_CHECK:case SMY_RIVET_GRIP:
        return (n==6||(kind==SMY_RIVET_GRIP&&n==7&&fd_name(a[6])&&_wcsicmp(a[1],a[6])))&&
            dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&sf_id(a[4],L"SURFACE:")&&sf_id(a[5],L"SURFACE:")?1:-1;
    case SMY_SPOT_WELDS:case SMY_PLUG_SLOT:
        if(n!=(kind==SMY_SPOT_WELDS?13:14)||!dv_safe_leaf(a[2])||
           (kind==SMY_PLUG_SLOT&&(!fd_name(a[2])||wcslen(a[2])>24))||
           !sf_id(a[3],L"SURFACE:")||!mp_named_ref(a[4],L"CSYS:")||!mp_named_ref(a[5],L"PLANE:"))return -1;
        for(int i=6;i<n;i++){
            double minimum=(i==n-1)?0:(kind==SMY_SPOT_WELDS?i<10:i==6||i==7||i==9||i==10)?-1e5:0.01;
            if(!smx_number(a[i],minimum,1e5))return -1;
        }
        return 1;
    case SMY_JOIN_CARD:return n==4&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])?1:-1;
    case SMY_WELD_SCHEDULE:
        return n==9&&dv_safe_leaf(a[2])&&sf_id(a[3],L"EDGE:")&&
            (!wcscmp(a[4],L"CONT")||!wcscmp(a[4],L"STITCH"))&&
            smx_number(a[5],0,100000)&&smx_number(a[6],0,100000)&&
            smx_number(a[7],0,100000)&&smx_number(a[8],0,100000)?1:-1;
    case SMY_CASE_SPACE:
        if(n!=13||!fd_name(a[2])||wcslen(a[2])>22||!mp_named_ref(a[3],L"PLANE:")||!mp_named_ref(a[4],L"PLANE:")||
           !smx_number(a[5],0.1,100)||!smx_number(a[6],0.1,100)||!smx_number(a[7],0.1,100)||!sx_side(a[8])||!dv_safe_leaf(a[9]))return -1;
        for(int i=10;i<13;i++)if(!smx_number(a[i],0,1000))return -1;
        return 1;
    case SMY_CASE_FLUSH:case SMY_CASE_SEAL:
        if(n!=(kind==SMY_CASE_FLUSH?11:14)||!fd_name(a[2])||!_wcsicmp(a[1],a[2])||
           !mp_named_ref(a[3],L"CSYS:")||!sf_id(a[4],L"SURFACE:")||
           !mp_named_ref(a[5],L"CSYS:")||!sf_id(a[6],L"SURFACE:")||
           !smx_number(a[7],-1e5,1e5)||!smx_number(a[8],-1e5,1e5))return -1;
        if(kind==SMY_CASE_FLUSH)return smx_number(a[9],-1000,1000)&&smx_number(a[10],0.001,1000)?1:-1;
        return smx_number(a[9],0.01,1000)&&smx_number(a[10],0,100)&&smx_number(a[11],0,100)&&
            smx_number(a[12],0.01,1e4)&&smx_number(a[13],0.01,1e4)?1:-1;
    case SMY_TOOL_WITHDRAWAL:
        if(n!=12||!mp_named_ref(a[2],L"CSYS:")||(wcscmp(a[11],L"RECT")&&wcscmp(a[11],L"ROUND")))return -1;
        for(int i=3;i<11;i++)if(!smx_number(a[i],i<7?-1e5:i==10?0:0.01,i<7?1e5:10000))return -1;
        return 1;
    case SMY_FINISH_PROTECTION:
        if(n!=11||!sf_id(a[2],L"SURFACE:")||!mp_named_ref(a[3],L"CSYS:"))return -1;
        for(int i=4;i<11;i++)if(!smx_number(a[i],i<6?-1e5:i<8?0.01:0,i<6?1e5:10000))return -1;
        return 1;
    case SMY_COATING_POINT:
        return n==9&&(!wcscmp(a[2],L"MASK")||!wcscmp(a[2],L"HANG"))&&sf_id(a[3],L"SURFACE:")&&sf_id(a[4],L"SURFACE:")&&
            smx_number(a[5],0.01,1000)&&smx_number(a[6],0,1000)&&smx_number(a[7],wcstod(a[6],NULL),1000)&&smx_number(a[8],0,1000)?1:-1;
    case SMY_CABLE_ENTRY:
        if(n!=13||!fd_name(a[2])||wcslen(a[2])>24||!sf_id(a[3],L"SURFACE:")||!mp_named_ref(a[4],L"CSYS:")||
           !mp_named_ref(a[5],L"PLANE:")||!dv_safe_leaf(a[6])||!dv_safe_leaf(a[7]))return -1;
        for(int i=8;i<n;i++)if(!smx_number(a[i],i<10?-1e5:i==10?0.01:0,1e5))return -1;
        return 1;
    case SMY_VENT_INTERFACE:
        return n==11&&fd_name(a[2])&&wcslen(a[2])<=24&&sf_id(a[3],L"SURFACE:")&&mp_named_ref(a[4],L"CSYS:")&&
            mp_named_ref(a[5],L"PLANE:")&&smy_vent_kind(a[6])&&dv_safe_leaf(a[7])&&
            smx_number(a[8],-1e5,1e5)&&smx_number(a[9],-1e5,1e5)&&smx_number(a[10],0,1000)?1:-1;
    case SMY_MOUNT_INSPECTION:
        if(n<8||n>18||!smy_mount_inspection_role(a[2])||!dv_safe_leaf(a[3])||
           !sf_id(a[4],L"SURFACE:")||!mp_named_ref(a[5],L"CSYS:"))return -1;
        for(int i=6;i<n;i++){
            if(!sf_id(a[i],L"SURFACE:")||!wcscmp(a[i],a[4]))return -1;
            int id;
            part_integer(a[i]+8,&id);
            for(int j=6;j<i;j++){
                int previous;
                part_integer(a[j]+8,&previous);
                if(previous==id)return -1;
            }
        }
        return 1;
    case SMY_CASE_DISPLAY:
        if(n!=15||!fd_name(a[2])||wcslen(a[2])>24||!sf_id(a[3],L"SURFACE:")||
           !mp_named_ref(a[4],L"CSYS:")||!mp_named_ref(a[5],L"PLANE:")||
           !smx_number(a[6],-1e5,1e5)||!smx_number(a[7],-1e5,1e5))return -1;
        for(int i=8;i<15;i++)if(!smx_number(a[i],0.1,10000))return -1;
        return 1;
    case SMY_MIRROR_PAIR:
        return n==8&&fd_name(a[2])&&_wcsicmp(a[1],a[2])&&sf_id(a[3],L"SURFACE:")&&sf_id(a[4],L"SURFACE:")&&
            (!wcscmp(a[5],L"X")||!wcscmp(a[5],L"Y"))&&smx_number(a[6],-1e5,1e5)&&smx_number(a[7],0.0001,1)?1:-1;
    case SMY_GAUGE_FACE:
        return n==7&&sf_id(a[2],L"SURFACE:")&&sf_id(a[3],L"EDGE:")&&smx_number(a[4],0.01,10000)&&
            smx_number(a[5],0.01,1000)&&smx_number(a[6],0,1000)?1:-1;
    case SMY_DFM_FLANGE:case SMY_DFM_SHORT:return n==4&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])?1:-1;
    case SMY_FORM_INTERACTION:
        return n==5&&fd_name(a[2])&&sf_id(a[3],L"SURFACE:")&&smx_number(a[4],0.001,10000)?1:-1;
    case SMY_BEND_NUMBER:return n==3&&dv_safe_leaf(a[2])?1:-1;
    case SMY_BEND_ORDER:return n==4&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])?1:-1;
    case SMY_BEND_CANDIDATES:return n==3&&dv_safe_leaf(a[2])?1:-1;
    case SMY_BEND_TOOL:
        return n==6&&sf_id(a[2],L"SURFACE:")&&dv_safe_leaf(a[3])&&dv_safe_leaf(a[4])&&dv_safe_leaf(a[5])?1:-1;
    case SMY_BEND_FORCE:
        return n==5&&sf_id(a[2],L"SURFACE:")&&dv_safe_leaf(a[3])&&dv_safe_leaf(a[4])?1:-1;
    case SMY_BEND_TRIAL:
        return n==8&&dv_safe_leaf(a[2])&&sf_id(a[3],L"SURFACE:")&&dv_safe_leaf(a[4])&&dv_safe_leaf(a[5])&&
            dv_safe_leaf(a[6])&&smx_number(a[7],0.01,180)?1:-1;
    case SMY_VERSION_COMPARE:
        return n==8&&fd_name(a[2])&&_wcsicmp(a[1],a[2])&&sf_id(a[3],L"SURFACE:")&&sf_id(a[4],L"SURFACE:")&&
            smx_number(a[5],0.0001,2)&&dv_safe_leaf(a[6])&&dv_safe_leaf(a[7])?1:-1;
    case SMY_BEND_SEGMENTS:
        if(n<7||n>24||!(n%2)||!sf_id(a[2],L"SURFACE:"))return -1;
        for(int i=3;i<n;i++)if(!smx_number(a[i],0,1e5))return -1;
        return 1;
    case SMY_BEND_CARD:return n==5&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&(!wcscmp(a[4],L"LONG")||!wcscmp(a[4],L"TOOL"))?1:-1;
    case SMY_FLAT_COMPARE:
        if(n<5||n>9||!dv_safe_leaf(a[2]))return -1;
        for(int i=3;i<n;i++){
            if(!sf_id(a[i],L"SURFACE:"))return -1;
            for(int j=3;j<i;j++)if(!wcscmp(a[i],a[j]))return -1;
        }
        return 1;
    case SMY_FLAT_HOLE_MAP:
        if(n<6||n>22||n%2||!dv_safe_leaf(a[2])||!sf_id(a[3],L"SURFACE:"))return -1;
        for(int i=4;i<n;i++)if(!sf_id(a[i],L"SURFACE:"))return -1;
        return 1;
    case SMY_FLAT_BEND_LAYERS:return n==4&&dv_safe_leaf(a[2])&&sf_id(a[3],L"SURFACE:")?1:-1;
    case SMY_FLAT_TOPOLOGY:return n==3&&dv_safe_leaf(a[2])?1:-1;
    case SMY_FLAT_DATUM:
        if(n<7||n>12||!dv_safe_leaf(a[2])||!smx_number(a[3],-1e6,1e6)||!smx_number(a[4],-1e6,1e6)||
           !smx_number(a[5],-360,360))return -1;
        for(int i=6;i<n;i++){
            if(!dv_safe_leaf(a[i]))return -1;
            for(int j=6;j<i;j++)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    case SMY_FLAT_CLEAN:return n==5&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&smx_number(a[4],0.00001,0.05)?1:-1;
    case SMY_FLAT_MATCH:return n==5&&dv_safe_leaf(a[2])&&sf_id(a[3],L"SURFACE:")&&smx_number(a[4],0.00001,2)?1:-1;
    case SMY_FLAT_LOCK:return n==4&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])?1:-1;
    case SMY_DRAW_TEMPLATE:
        return n==7&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&_wcsicmp(a[2],a[3])&&dv_safe_leaf(a[4])&&dv_safe_leaf(a[5])&&dv_safe_leaf(a[6])?1:-1;
    case SMY_FIRST_CARD:return n==4&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])?1:-1;
    case SMY_DELIVERY_CHECK:return n==5&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&sf_id(a[4],L"SURFACE:")?1:-1;
    case SMY_DELIVERY_BUNDLE:
        return n==9&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&sf_id(a[4],L"SURFACE:")&&
            dv_safe_leaf(a[5])&&dv_safe_leaf(a[6])&&dv_safe_leaf(a[7])&&dv_safe_leaf(a[8])?1:-1;
    case SMY_STOCK_MEASURE:return n==4&&sf_id(a[2],L"SURFACE:")&&smx_number(a[3],0.001,100000)?1:-1;
    case SMY_STOCK_GROUP:case SMY_STOCK_HAND:return n==3&&dv_safe_leaf(a[2])?1:-1;
    case SMY_STOCK_REMNANT:return n==5&&dv_safe_leaf(a[2])&&dv_safe_leaf(a[3])&&smx_number(a[4],0,1000)?1:-1;
    case SMY_STOCK_COMMON:return n==4&&dv_safe_leaf(a[2])&&smx_number(a[3],0,100)?1:-1;
    case SMY_STOCK_FIT:case SMY_STOCK_GRID:case SMY_STOCK_PACK:case SMY_STOCK_AUDIT:case SMY_STOCK_TOTAL:
        return n==(kind==SMY_STOCK_PACK||kind==SMY_STOCK_TOTAL?9:8)&&dv_safe_leaf(a[2])&&
            smx_number(a[3],1,100000)&&smx_number(a[4],1,100000)&&
            smx_number(a[5],0,10000)&&smx_number(a[6],0,10000)&&smx_number(a[7],0,1000)&&
            (n==8||dv_safe_leaf(a[8]))?1:-1;
    }
    return -1;
}

static int smy_read_only(int kind){return kind!=SMY_VENT_INTERFACE&&kind!=SMY_STOCK_PACK&&kind!=SMY_STOCK_TOTAL&&kind!=SMY_FORM_RECESS&&kind!=SMY_FORM_BOSS&&
    kind!=SMY_FLAT_COMPARE&&kind!=SMY_FLAT_DATUM&&kind!=SMY_FLAT_CLEAN&&kind!=SMY_FLAT_LOCK&&kind!=SMY_WELD_SCHEDULE&&
    kind!=SMY_BEND_NUMBER&&kind!=SMY_BEND_ORDER&&kind!=SMY_BEND_CARD&&kind!=SMY_FIRST_CARD&&kind!=SMY_DELIVERY_BUNDLE&&kind!=SMY_CASE_SPACE&&kind!=SMY_CASE_DISPLAY&&kind!=SMY_DRAW_TEMPLATE&&kind!=SMY_BEND_TRIAL&&kind!=SMY_FORM_FLAT&&kind!=SMY_FLAT_HOLE_MAP&&kind!=SMY_FLAT_BEND_LAYERS&&kind!=SMY_PRESS_STUD&&kind!=SMY_SPOT_WELDS&&kind!=SMY_PLUG_SLOT&&kind!=SMY_CABLE_ENTRY&&kind!=SMY_JOIN_CARD;}

#endif
