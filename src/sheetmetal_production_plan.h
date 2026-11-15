
#ifndef BWTL_SHEETMETAL_PRODUCTION_PLAN_H
#define BWTL_SHEETMETAL_PRODUCTION_PLAN_H

enum {
    SMX_NONE,SMX_TEMPLATE=341,SMX_MATERIAL,SMX_SUBSTITUTE,SMX_METADATA,SMX_THICK_PREVIEW,
    SMX_THICK_APPLY,SMX_FRAMES,SMX_GRAIN,SMX_RULE,SMX_REVISION,
    SMX_GAUGE,SMX_ATTACH,SMX_PAIR,SMX_CONFIDENCE,SMX_RIP,SMX_REBUILD,SMX_BODIES,
    SMX_SPLIT,SMX_DXF_REBUILD,SMX_GEOMETRY_DIFF,
    SMX_OUTER,SMX_INNER,SMX_TO_PLANE,SMX_PROFILE,SMX_BEND_OFFSET,SMX_DIMENSION,
    SMX_DIRECTIONS,SMX_RADIUS,SMX_ALLOWANCE_SOURCE,SMX_COUPON,
    SMX_MITER=371,SMX_MITER_OFFSET,SMX_RELIEF,SMX_RELIEF_COMPARE,SMX_WELD_SEAM,SMX_JOGGLE,
    SMX_TAB_PAIR,SMX_FLANGE_CLEARANCE,SMX_CLOSED_HEM,SMX_PIN_BARREL,
    SMX_GRID=381,SMX_MOUNT,SMX_NORMAL,SMX_CROSS_BEND,SMX_BEND_CLEARANCE,SMX_WEB_CLEARANCE,
    SMX_CLIP_GRID,SMX_INTERFACE,SMX_LASER,SMX_EDIT_CIRCLES
};

static int smx_number(const wchar_t *token,double low,double high);
#include "sheetmetal_workshop_plan.h"

static int smx_kind(const wchar_t *verb){
    int workshop=smy_kind(verb);
    if(workshop)return workshop;
#ifdef BWTL_ENABLE_SMX_CANDIDATE
    static const wchar_t *const names[]={
        L"SMX_TEMPLATE",L"SMX_MATERIAL",L"SMX_SUBSTITUTE",L"SMX_METADATA",L"SMX_THICK_PREVIEW",
        L"SMX_THICK_APPLY",L"SMX_FRAMES",L"SMX_GRAIN",L"SMX_RULE",L"SMX_REVISION",
        L"SMX_GAUGE",L"SMX_ATTACH",L"SMX_PAIR",L"SMX_CONFIDENCE",L"SMX_RIP",L"SMX_REBUILD",
        L"SMX_BODIES",L"SMX_SPLIT",L"SMX_DXF_REBUILD",L"SMX_GEOMETRY_DIFF",
        L"SMX_OUTER",L"SMX_INNER",L"SMX_TO_PLANE",L"SMX_PROFILE",L"SMX_BEND_OFFSET",
        L"SMX_DIMENSION",L"SMX_DIRECTIONS",L"SMX_RADIUS",L"SMX_ALLOWANCE_SOURCE",L"SMX_COUPON"
    };
    for(int i=0;i<_countof(names);i++)if(!wcscmp(verb,names[i]))return SMX_TEMPLATE+i;
    if(!wcscmp(verb,L"SMX_MITER"))return SMX_MITER;
    if(!wcscmp(verb,L"SMX_MITER_OFFSET"))return SMX_MITER_OFFSET;
    if(!wcscmp(verb,L"SMX_WELD_SEAM"))return SMX_WELD_SEAM;
    if(!wcscmp(verb,L"SMX_JOGGLE"))return SMX_JOGGLE;
    if(!wcscmp(verb,L"SMX_CORNER_RELIEF"))return SMX_RELIEF;
    if(!wcscmp(verb,L"SMX_RELIEF_COMPARE"))return SMX_RELIEF_COMPARE;
    if(!wcscmp(verb,L"SMX_FLANGE_CLEARANCE"))return SMX_FLANGE_CLEARANCE;
    if(!wcscmp(verb,L"SMX_CLOSED_HEM"))return SMX_CLOSED_HEM;
    if(!wcscmp(verb,L"SMX_PIN_BARREL"))return SMX_PIN_BARREL;
    if(!wcscmp(verb,L"SMX_TAB_PAIR"))return SMX_TAB_PAIR;
    static const wchar_t *const openings[]={
        L"SMX_GRID",L"SMX_MOUNT",L"SMX_NORMAL",L"SMX_CROSS_BEND",L"SMX_BEND_CLEARANCE",
        L"SMX_WEB_CLEARANCE",L"SMX_CLIP_GRID",L"SMX_INTERFACE",L"SMX_LASER",L"SMX_EDIT_CIRCLES"
    };
    for(int i=0;i<_countof(openings);i++)if(!wcscmp(verb,openings[i]))return SMX_GRID+i;
#else
    (void)verb;
#endif
    return SMX_NONE;
}

static int smx_independent(int kind){return kind==SMX_TEMPLATE;}

static int smx_read_only(int kind,const PartCommand *cmd){
    if(kind>=391)return smy_read_only(kind);
    switch(kind){
    case SMX_MATERIAL:case SMX_SUBSTITUTE:case SMX_THICK_PREVIEW:case SMX_RULE:
    case SMX_GAUGE:case SMX_ATTACH:case SMX_PAIR:case SMX_CONFIDENCE:case SMX_BODIES:
    case SMX_GEOMETRY_DIFF:case SMX_DIMENSION:case SMX_DIRECTIONS:case SMX_RADIUS:
    case SMX_ALLOWANCE_SOURCE:case SMX_COUPON:case SMX_NORMAL:case SMX_CROSS_BEND:
    case SMX_BEND_CLEARANCE:case SMX_WEB_CLEARANCE:case SMX_LASER:
    case SMX_RELIEF_COMPARE:case SMX_FLANGE_CLEARANCE:case SMX_RIP:return 1;
    case SMX_REVISION:return cmd->count==3;
    }
    return 0;
}

static int smx_number(const wchar_t *token,double low,double high){
    double value;
    return part_number(token,&value)&&value>=low&&value<=high;
}

static int smx_word(const wchar_t *token){
    size_t size=wcslen(token);
    if(!size||size>=32)return 0;
    for(const wchar_t *p=token;*p;p++)
        if(!iswalnum(*p)&&!wcschr(L"_-.",*p))return 0;
    return wcscmp(token,L".")&&wcscmp(token,L"..");
}

static int smx_csys(const wchar_t *token){return !wcsncmp(token,L"CSYS:",5)&&fd_name(token+5);}
static int smx_body_token(const wchar_t *token,int *id){
    if(wcsncmp(token,L"BODY:",5))return 0;
    const wchar_t *number=token+5;
    if(*number==L'-')number++;
    if(!part_integer(number,id)||!*id)return 0;
    if(token[5]==L'-')*id=-*id;
    return 1;
}
static int smx_axis(const wchar_t *token){return !wcscmp(token,L"X")||!wcscmp(token,L"Y")||!wcscmp(token,L"Z");}

static int smx_standard_plan(int n,wchar_t a[][PART_TOKEN_MAX],int kind){
    switch(kind){
    case SMX_TEMPLATE:return n==3&&fd_name(a[1])&&smx_word(a[2])?1:-1;
    case SMX_MATERIAL:return n==4&&smx_word(a[2])&&smx_number(a[3],0.01,100)?1:-1;
    case SMX_SUBSTITUTE:return n==5&&smx_word(a[2])&&smx_word(a[3])&&_wcsicmp(a[2],a[3])&&smx_number(a[4],0.01,100)?1:-1;
    case SMX_METADATA:
        if(n!=9||!smx_number(a[7],0.01,100))return -1;
        for(int i=2;i<9;i++)if(i!=7&&!smx_word(a[i]))return -1;
        return 1;
    case SMX_THICK_PREVIEW:
        return n==5&&smx_number(a[2],0.01,100)&&smx_number(a[3],0.01,100)&&sf_id(a[4],L"SURFACE:")?1:-1;
    case SMX_THICK_APPLY:
        return n==8&&smx_number(a[2],0.01,100)&&smx_number(a[3],0.01,100)&&smx_axis(a[4])&&
            smx_number(a[5],0.01,1e5)&&smx_number(a[6],0.0001,10)&&sf_id(a[7],L"SURFACE:")?1:-1;
    case SMX_FRAMES:return n==4&&smx_csys(a[2])&&smx_csys(a[3])?1:-1;
    case SMX_GRAIN:
        return n==6&&sf_id(a[2],L"SURFACE:")&&sf_id(a[3],L"EDGE:")&&
            (!wcscmp(a[4],L"COATED")||!wcscmp(a[4],L"BARE"))&&
            (!wcscmp(a[5],L"FILM")||!wcscmp(a[5],L"NO_FILM"))?1:-1;
    case SMX_RULE:
        return n==7&&smx_word(a[2])&&smx_number(a[3],0.01,100)&&smx_number(a[4],0.01,1000)&&
            smx_number(a[5],1,180)&&smx_word(a[6])?1:-1;
    case SMX_REVISION:return (n==3&&smx_word(a[2]))||
        (n==5&&smx_word(a[2])&&!wcscmp(a[3],L"APPLY")&&smx_word(a[4]))?1:-1;
    default:return 0;
    }
}

static int smx_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX],int kind){
    if(kind>=391)return n>=3&&fd_name(a[1])?smy_workshop_input(n,a,kind):-1;
    if(kind<=SMX_REVISION)return smx_standard_plan(n,a,kind);
    switch(kind){
    case SMX_GAUGE:case SMX_ATTACH:case SMX_PAIR:case SMX_CONFIDENCE:case SMX_BODIES:
    case SMX_ALLOWANCE_SOURCE:return n==2?1:-1;
    case SMX_RIP:
        if(n<5||n>8||!sf_id(a[2],L"SURFACE:")||!smx_number(a[3],0.001,25))return -1;
        for(int i=4;i<n;i++)if(!sf_id(a[i],L"EDGE:"))return -1;
        return 1;
    case SMX_REBUILD:return n==5&&fd_name(a[2])&&_wcsicmp(a[1],a[2])&&sf_id(a[3],L"SURFACE:")&&smx_number(a[4],0.01,100)?1:-1;
    case SMX_SPLIT:{int id;return n==4&&smx_body_token(a[2],&id)&&fd_name(a[3])&&_wcsicmp(a[1],a[3])?1:-1;}
    case SMX_DXF_REBUILD:return n==8&&fd_name(a[2])&&smx_word(a[3])&&ad_plane(a[4])&&ad_plane(a[5])&&
        smx_number(a[6],0.01,100)&&sx_side(a[7])?1:-1;
    case SMX_GEOMETRY_DIFF:return n==4&&fd_name(a[2])&&_wcsicmp(a[1],a[2])&&smx_number(a[3],0.0001,50)?1:-1;
    case SMX_OUTER:case SMX_INNER:case SMX_TO_PLANE:
        return n==10&&fd_name(a[2])&&sf_id(a[3],L"EDGE:")&&
            (kind==SMX_OUTER?smx_axis(a[4]):kind==SMX_INNER?sf_id(a[4],L"SURFACE:"):ad_plane(a[4]))&&
            (kind==SMX_TO_PLANE?!wcscmp(a[5],L"0"):smx_number(a[5],0.01,10000))&&
            sh_angle(a[6])&&smx_number(a[7],0.01,1000)&&sx_side(a[8])&&smx_number(a[9],0.0001,5)?1:-1;
    case SMX_PROFILE:
        if(n<12||n>24||(n-4)%4||!fd_name(a[2])||!sf_id(a[3],L"EDGE:"))return -1;
        for(int i=4;i<n;i+=4)
            if(!smx_number(a[i],0.01,10000)||!sh_angle(a[i+1])||!smx_number(a[i+2],0.01,1000)||!sx_side(a[i+3]))return -1;
        return 1;
    case SMX_BEND_OFFSET:return (n==10||(n==11&&sf_id(a[10],L"SURFACE:")))&&fd_name(a[2])&&sf_id(a[3],L"EDGE:")&&smx_number(a[4],0.01,10000)&&
        ad_plane(a[5])&&sh_angle(a[6])&&smx_number(a[7],0.01,1000)&&sx_side(a[8])&&
        (!wcscmp(a[9],L"FIX1")||!wcscmp(a[9],L"FIX2"))?1:-1;
    case SMX_DIMENSION:return n==3&&sf_id(a[2],L"SURFACE:")?1:-1;
    case SMX_DIRECTIONS:
        return n==7&&sf_id(a[2],L"EDGE:")&&smx_number(a[3],0.01,10000)&&sh_angle(a[4])&&
            smx_number(a[5],0.01,1000)&&sf_id(a[6],L"SURFACE:")?1:-1;
    case SMX_RADIUS:return n==6&&smx_word(a[2])&&!wcscmp(a[3],L"GRAIN")&&smx_number(a[4],0,90)&&
        smx_number(a[5],0,1000)?1:-1;
    case SMX_COUPON:return n==7&&smx_number(a[2],0.01,100)&&smx_number(a[3],0.01,1000)&&
        smx_number(a[4],1,175)&&smx_number(a[5],0.01,10000)&&smx_number(a[6],0.01,20000)?1:-1;
    case SMX_MITER:case SMX_MITER_OFFSET:case SMX_WELD_SEAM:{
        int tail=kind==SMX_MITER?7:9;
        if(n!=(kind==SMX_WELD_SEAM?15:tail+2)||!fd_name(a[2])||!sf_id(a[3],L"EDGE:")||!sf_id(a[4],L"EDGE:")||
           !wcscmp(a[3],a[4]))return -1;
        for(int i=5;i<=tail;i++)if(!smx_number(a[i],0.05,1000))return -1;
        if(!smx_number(a[tail+1],0.001,1000))return -1;
        return kind!=SMX_WELD_SEAM||(smx_number(a[11],0,100)&&smx_number(a[12],0,100)&&
            smx_number(a[13],0,100)&&smx_number(a[14],0.0001,5))?1:-1;
    }
    case SMX_JOGGLE:
        return n==11&&fd_name(a[2])&&sf_id(a[3],L"EDGE:")&&smx_number(a[4],0.01,1000)&&
            smx_number(a[5],0.01,1000)&&smx_number(a[6],0.01,1000)&&sx_side(a[7])&&
            sf_id(a[8],L"SURFACE:")&&smx_number(a[9],0.01,1000)&&smx_number(a[10],0.0001,1000)?1:-1;
    case SMX_RELIEF:case SMX_RELIEF_COMPARE:
        return n==(kind==SMX_RELIEF?9:8)&&fd_name(a[2])&&sf_id(a[3],L"SURFACE:")&&sf_id(a[4],L"SURFACE:")&&sf_id(a[5],L"SURFACE:")&&
            (kind==SMX_RELIEF_COMPARE||!wcscmp(a[6],L"RECT")||!wcscmp(a[6],L"ROUND")||!wcscmp(a[6],L"V"))&&
            smx_number(a[n-2],0.01,1000)&&smx_number(a[n-1],0.01,1000)?1:-1;
    case SMX_TAB_PAIR:
        return n==18&&fd_name(a[2])&&fd_name(a[3])&&fd_name(a[4])&&_wcsicmp(a[1],a[3])&&_wcsicmp(a[2],a[4])&&
            sf_id(a[5],L"EDGE:")&&sf_id(a[6],L"SURFACE:")&&smx_csys(a[7])&&ad_plane(a[8])&&
            smx_number(a[9],0,10000)&&smx_number(a[10],0.01,10000)&&smx_number(a[11],0.01,10000)&&
            smx_number(a[12],0.01,1000)&&sx_side(a[13])&&smx_number(a[14],-1e5,1e5)&&smx_number(a[15],-1e5,1e5)&&
            smx_number(a[16],0.001,20)&&smx_number(a[17],0,10000)?1:-1;
    case SMX_CLOSED_HEM:case SMX_PIN_BARREL:
        return n==(kind==SMX_CLOSED_HEM?10:12)&&fd_name(a[2])&&sf_id(a[3],L"EDGE:")&&
            smx_number(a[4],0.01,10000)&&smx_number(a[5],0.01,1000)&&sx_side(a[6])&&
            smx_number(a[7],0.001,kind==SMX_CLOSED_HEM?0.2:1000)&&smx_number(a[8],0.01,10000)&&
            smx_number(a[9],0.0001,kind==SMX_CLOSED_HEM?0.05:1000)&&
            (kind==SMX_CLOSED_HEM||(smx_number(a[10],0.0001,1000)&&smx_number(a[11],0.0001,1)))?1:-1;
    case SMX_FLANGE_CLEARANCE:return n==5&&sf_id(a[2],L"SURFACE:")&&sf_id(a[3],L"SURFACE:")&&smx_number(a[4],0,1000)?1:-1;
    case SMX_GRID:case SMX_CLIP_GRID:{
        int nx,ny;
        return n==17&&fd_name(a[2])&&sf_id(a[3],L"SURFACE:")&&smx_csys(a[4])&&ad_plane(a[5])&&
            smx_number(a[6],-1e5,1e5)&&smx_number(a[7],-1e5,1e5)&&
            part_integer(a[8],&nx)&&part_integer(a[9],&ny)&&nx>=1&&nx<=48&&ny>=1&&ny<=48&&nx*ny<=48&&
            smx_number(a[10],nx==1?0:0.01,10000)&&smx_number(a[11],ny==1?0:0.01,10000)&&
            (!wcscmp(a[12],L"ROUND")||!wcscmp(a[12],L"SLOT")||!wcscmp(a[12],L"D"))&&
            smx_number(a[13],0.01,10000)&&smx_number(a[14],0,10000)&&
            smx_number(a[15],0,10000)&&smx_number(a[16],0,10000)?1:-1;
    }
    case SMX_MOUNT:
        return n==10&&fd_name(a[2])&&sf_id(a[3],L"SURFACE:")&&smx_csys(a[4])&&ad_plane(a[5])&&
            smx_word(a[6])&&smx_number(a[7],-1e5,1e5)&&smx_number(a[8],-1e5,1e5)&&smx_number(a[9],0,10000)?1:-1;
    case SMX_INTERFACE:
        return n==13&&fd_name(a[2])&&sf_id(a[3],L"SURFACE:")&&smx_csys(a[4])&&ad_plane(a[5])&&
            (!wcscmp(a[6],L"ROUNDRECT")||!wcscmp(a[6],L"D")||!wcscmp(a[6],L"KEY")||!wcscmp(a[6],L"NOTCH"))&&
            smx_number(a[7],-1e5,1e5)&&smx_number(a[8],-1e5,1e5)&&smx_number(a[9],0.01,10000)&&
            smx_number(a[10],0,10000)&&smx_number(a[11],0,10000)&&smx_number(a[12],0,10000)?1:-1;
    case SMX_NORMAL:case SMX_BEND_CLEARANCE:case SMX_WEB_CLEARANCE:
        return (n==5||(kind==SMX_WEB_CLEARANCE&&n==6&&sf_id(a[5],L"SURFACE:")))&&
            sf_id(a[2],L"SURFACE:")&&sf_id(a[3],L"SURFACE:")&&smx_number(a[4],0,kind==SMX_NORMAL?90:10000)?1:-1;
    case SMX_CROSS_BEND:return n==3&&sf_id(a[2],L"SURFACE:")?1:-1;
    case SMX_LASER:return n==6&&sf_id(a[2],L"SURFACE:")&&smx_number(a[3],0.0001,10000)&&
        smx_number(a[4],0.0001,10000)&&smx_number(a[5],0.01,180)?1:-1;
    case SMX_EDIT_CIRCLES:{
        int count;
        return n==7&&fd_name(a[2])&&fd_name(a[3])&&smx_number(a[4],0.01,10000)&&smx_number(a[5],0.01,10000)&&
            wcstod(a[4],NULL)!=wcstod(a[5],NULL)&&part_integer(a[6],&count)&&count>=1&&count<=48?1:-1;
    }
    }
    return -1;
}
#endif
