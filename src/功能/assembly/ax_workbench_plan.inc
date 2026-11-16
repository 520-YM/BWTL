enum AxWorkKind {
    AW_NONE = 200,
#define AX_WORK(number, suffix, category, title, inputs) AW_##suffix,
#include "ax_workbench_catalog.inc"
#undef AX_WORK
    AW_END
};

#define AW_WIDE_I(x) L##x
#define AW_WIDE(x) AW_WIDE_I(x)
static int aw_kind(const wchar_t *verb){
    static const wchar_t *const verbs[]={
#define AX_WORK(number, suffix, category, title, inputs) L"ASM_W_" AW_WIDE(#suffix),
#include "ax_workbench_catalog.inc"
#undef AX_WORK
    };
    for(int i=0;i<_countof(verbs);i++)if(!wcscmp(verb,verbs[i]))return AW_NONE+1+i;
    return 0;
}

static int aw_diagnostic(int kind){return kind==AW_FIND||kind==AW_OCCURRENCE||kind==AW_CONFLICT||kind==AW_FREEDOM||kind==AW_PAIRS||kind==AW_METHOD_RECALL;}

static int aw_request(const wchar_t *text){
    static const wchar_t *const titles[]={
#define AX_WORK(number, suffix, category, title, inputs) L##title,
#include "ax_workbench_catalog.inc"
#undef AX_WORK
    };
    if(wcsstr(text,L"ASM_W_")||wcsstr(text,L"定位到")||wcsstr(text,L"找一下")&&wcsstr(text,L"当前装配"))return 1;
    for(int i=0;i<_countof(titles);i++)if(wcsstr(text,titles[i]))return 1;
    return 0;
}

#undef AW_WIDE
#undef AW_WIDE_I

static int aw_mutates(int kind){return kind==AW_ALIGN||kind==AW_ALIGN_BATCH||kind==AW_BEAM_LAYOUT||
    kind==AW_TRAY_LAYOUT||kind==AW_ROUTE_WRITE||kind==AW_CLIP_WRITE||kind==AW_GLASS_BLOCKS;}

static int aw_path(const wchar_t *value){
    int ids[AX_PATH_DEPTH];
    return ax_path_parse(value,ids)!=0;
}
static int aw_face_inputs(wchar_t args[][PART_TOKEN_MAX],int offset){
    return aw_path(args[offset])&&ax_q_surface(args[offset+1]);
}
static int aw_mm(wchar_t args[][PART_TOKEN_MAX],int from,int until,double min,double max){
    for(int i=from;i<until;i++)if(!qa_number(args[i],min,max))return 0;
    return 1;
}
static int aw_two_faces(wchar_t args[][PART_TOKEN_MAX],int offset){
    return aw_face_inputs(args,offset)&&aw_face_inputs(args,offset+2)&&
        (wcscmp(args[offset],args[offset+2])||wcscmp(args[offset+1],args[offset+3]));
}

static int aw_root_or_path(const wchar_t *token){return !wcscmp(token,L"ROOT")||aw_path(token);}
static int aw_frame_inputs(wchar_t args[][PART_TOKEN_MAX],int offset){
    return aw_root_or_path(args[offset])&&ax_csys_token(args[offset+1]);
}

static int aw_connection_token(const wchar_t *token,wchar_t id[PART_TOKEN_MAX],wchar_t csys[PART_TOKEN_MAX]){
    const wchar_t *separator=wcschr(token,L'@');
    if(!separator||separator==token||separator-token>=PART_TOKEN_MAX||wcslen(separator+1)>=PART_TOKEN_MAX)return 0;
    wcsncpy_s(id,PART_TOKEN_MAX,token,separator-token);
    wcscpy_s(csys,PART_TOKEN_MAX,separator+1);
    return aw_root_or_path(id)&&ax_csys_token(csys);
}

static int aw_service_valid(int kind,int count,wchar_t a[][PART_TOKEN_MAX]){
    static const wchar_t *const roles[][3]={
        {L"STRAIGHT",L"ANGLED",L"SWING"},{L"CLOSE",NULL,NULL},{L"WIRE",L"TOOL",NULL},
        {L"PLUG",L"RELEASE",L"CABLE"},{L"SLIDE",L"RELEASE",NULL},{L"GRIP",L"REMOVE",NULL},
        {L"DRAW",L"GRIP",NULL},{L"ENTRY",L"TURN",L"SEAT"},{L"WELD",L"GRIND",NULL},
        {L"INSTALL",L"REMOVE",NULL},{L"OPEN",L"CLOSED",NULL},{L"SUPPORT",L"CLAMP",L"HAND"},
        {L"BLOCK",NULL,NULL},{L"SWIVEL",L"BRAKE",NULL}
    };
    static const int required[]={7,1,3,7,1,3,3,5,3,3,3,7,1,3};
    const int offset=kind==AW_TERMINAL_ACCESS?6:3;
    const int kinds[]={AW_TOOL_ACCESS,AW_DOOR_FINAL,AW_TERMINAL_ACCESS,AW_PLUG_PATH,AW_RAIL_SERVICE,
                       AW_SMALL_SERVICE,AW_FILTER_SERVICE,AW_INSTALL_PATH,AW_WELD_TOOL,
                       AW_OUTDOOR_ACCESS,AW_CLOSE_BLOCKS,AW_TEMP_SUPPORT,AW_REMOVE_MIN,AW_CASTER_BRAKE};
    int index=0;
    while(index<_countof(kinds)&&kinds[index]!=kind)index++;
    if(index==_countof(kinds))return -1;
    if(count<offset+8||(count-offset)%8||!qa_number(a[2],0,1000))return -1;
    if(offset==6&&!aw_mm(a,3,6,0,1000))return -1;
    int mask=0,last=-1;
    for(int i=offset;i<count;i+=8){
        int role=0;
        while(role<3&&roles[index][role]&&wcscmp(a[i],roles[index][role]))role++;
        if(role==3||!roles[index][role]||!aw_path(a[i+1])||!aw_path(a[i+2])||!wcscmp(a[i+1],a[i+2])||
           wcsncmp(a[i+3],L"AXIS:",5)||!fd_name(a[i+3]+5))return -1;
        const int rotation=!wcscmp(a[i+4],L"ROTATE");
        if(!rotation&&wcscmp(a[i+4],L"SLIDE"))return -1;
        if((kind==AW_TOOL_ACCESS&&rotation!=(role==2))||(kind==AW_DOOR_FINAL&&!rotation)||
           (kind==AW_INSTALL_PATH&&rotation!=(role==1)))return -1;
        const double maximum=rotation?360:10000;
        if(!aw_mm(a,i+5,i+7,-maximum,maximum)||!ax_int_range(a[i+7],2,25))return -1;
        const double low=wcstod(a[i+5],NULL),high=wcstod(a[i+6],NULL);
        if(fabs(high-low)<0.000001||(kind==AW_DOOR_FINAL&&fabs(high-low)>45))return -1;
        if(kind==AW_INSTALL_PATH&&(last>=role||fabs(low)>1e-7||wcscmp(a[i+1],a[offset+1])))return -1;
        if(kind==AW_CASTER_BRAKE&&role==0&&(!rotation||fabs(high-low)<359.999))return -1;
        if(kind==AW_REMOVE_MIN&&i>offset){
            if(wcscmp(a[i+1],a[offset+1])||wcscmp(a[i+3],a[offset+3])||wcscmp(a[i+4],a[offset+4])||
               fabs(low-wcstod(a[offset+5],NULL))>1e-7||fabs(high-wcstod(a[offset+6],NULL))>1e-7)return -1;
            for(int j=offset;j<i;j+=8)if(!wcscmp(a[i+2],a[j+2]))return -1;
        }
        if(kind==AW_CLOSE_BLOCKS&&i>offset&&
           (wcscmp(a[i+1],a[offset+1])||wcscmp(a[i+3],a[offset+3])||wcscmp(a[i+4],a[offset+4])||
            fabs(low-wcstod(a[offset+5],NULL))>1e-7||fabs(high-wcstod(a[offset+6],NULL))>1e-7))return -1;
        last=role;
        mask|=1<<role;
    }
    return (mask&required[index])==required[index]?1:-1;
}


static int aw_csv_leaf(const wchar_t *token){
    const wchar_t *dot=wcsrchr(token,L'.');
    wchar_t name[PART_TOKEN_MAX];
    if(!dot||_wcsicmp(dot,L".csv")||dot<=token||dot-token>=_countof(name))return 0;
    wcsncpy_s(name,_countof(name),token,dot-token);
    return dv_safe_leaf(name);
}

static int aw_valid(int kind,int n,wchar_t a[][PART_TOKEN_MAX]){
    switch(kind){
    case AW_FIND:
        return (n==3&&a[2][0])||(n==6&&a[2][0]&&ax_signed_axis(a[3])&&ax_int_range(a[4],1,2048)&&
            qa_number(a[5],0.001,1000))?1:-1;
    case AW_PAIRS:
        return n==6&&aw_path(a[2])&&aw_path(a[3])&&wcscmp(a[2],a[3])&&
            qa_number(a[4],0.001,100)&&qa_number(a[5],0.001,15)?1:-1;
    case AW_OCCURRENCE:case AW_CONFLICT:case AW_FREEDOM:
        return n==3&&aw_path(a[2])?1:-1;
    case AW_ORIENTATION:
        return n==7&&aw_path(a[2])&&ax_csys_token(a[3])&&ax_signed_axis(a[4])&&ax_signed_axis(a[5])&&aw_mm(a,6,7,0,180)?1:-1;
    case AW_SCREW_LENGTH:
        if(n<10||n%2||!aw_two_faces(a,2)||!aw_mm(a,6,8,0,10000))return -1;
        for(int i=8;i<n;i+=2)if(!dv_safe_leaf(a[i])||!qa_number(a[i+1],0.001,10000))return -1;
        return 1;
    case AW_BLIND_BOTTOM:
        return n==11&&aw_two_faces(a,2)&&qa_number(a[6],0.001,10000)&&aw_mm(a,7,11,0,10000)?1:-1;
    case AW_COUNTERSINK:
        return n==10&&aw_two_faces(a,2)&&aw_mm(a,6,8,1,179)&&aw_mm(a,8,9,0,100)&&aw_mm(a,9,10,0,90)?1:-1;
    case AW_PRESS_FIT:
        return (n==12||n==14)&&aw_two_faces(a,2)&&ax_q_surface(a[6])&&aw_mm(a,7,n,0,1000)?1:-1;
    case AW_RIVET_GRIP:
        return n==10&&aw_two_faces(a,2)&&aw_mm(a,6,10,0,10000)?1:-1;
    case AW_THERMAL_GAP:
        if(n!=15||!aw_path(a[2])||!aw_path(a[3])||!wcscmp(a[2],a[3])||!ax_csys_token(a[4])||
           !aw_mm(a,9,11,0,500)||!aw_mm(a,11,12,-150,150)||!aw_mm(a,12,15,0,1000))return -1;
        for(int i=5;i<9;i++)if(!ax_q_surface(a[i]))return -1;
        return 1;
    case AW_DOOR_GAP:
        if(n<9||(n-5)%4||!aw_mm(a,2,5,0,10000))return -1;
        for(int i=5;i<n;i+=4)if(!aw_two_faces(a,i))return -1;
        return 1;
    case AW_FANS:
        if(n<7||(n-4)%3||!ax_signed_axis(a[2])||!aw_mm(a,3,4,0,90))return -1;
        for(int i=4;i<n;i+=3)if(!aw_path(a[i])||!ax_csys_token(a[i+1])||!ax_signed_axis(a[i+2]))return -1;
        return 1;
    case AW_DRAIN:
        return n==8&&aw_two_faces(a,2)&&ax_csys_token(a[6])&&aw_mm(a,7,8,0,100)?1:-1;
    case AW_REVISIONS:
        if(n<4||n%2)return -1;
        for(int i=2;i<n;i+=2){
            if(!a[i][0]||!a[i+1][0])return -1;
            for(int j=2;j<i;j+=2)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    case AW_ASBUILT:
        return n==10&&aw_two_faces(a,2)&&aw_mm(a,6,10,0,10000)?1:-1;
    case AW_ALIGN:case AW_ALIGN_BATCH:{
        int ids[AX_PATH_DEPTH];
        if(n<9||(n-4)%5||(kind==AW_ALIGN&&n!=9)||!aw_mm(a,2,3,0.001,10000)||!aw_mm(a,3,4,0,180))return -1;
        for(int i=4;i<n;i+=5){
            if(ax_path_parse(a[i],ids)!=1)return -1;
            for(int j=1;j<=4;j++)if(!ax_csys_token(a[i+j]))return -1;
            for(int j=4;j<i;j+=5)if(!wcscmp(a[j],a[i]))return -1;
            if(!_wcsicmp(a[i+1],a[i+3])&&!_wcsicmp(a[i+2],a[i+4]))return -1;
        }
        return 1;
    }
    case AW_CHECKPOINT:return n==3&&(!wcscmp(a[2],L"CAPTURE")||!wcscmp(a[2],L"COMPARE"))?1:-1;
    case AW_RESULT:return n==2?1:-1;
    case AW_SQUARE:
        if(n!=12||!aw_mm(a,2,3,0,1000)||!aw_mm(a,3,4,0,30))return -1;
        for(int i=4;i<n;i+=2)if(!aw_frame_inputs(a,i))return -1;
        return 1;
    case AW_TWIST:
        return n==11&&aw_frame_inputs(a,2)&&aw_frame_inputs(a,4)&&aw_mm(a,6,9,-1e6,1e6)&&
            aw_mm(a,9,10,0,1000)&&aw_mm(a,10,11,0,180)?1:-1;
    case AW_LEVEL:
        if(n<9||(n-3)%6||!ax_csys_token(a[2]))return -1;
        for(int i=3;i<n;i+=6)if(!aw_face_inputs(a,i)||!aw_mm(a,i+2,i+3,-1e6,1e6)||!aw_mm(a,i+3,i+6,0,10000))return -1;
        return 1;
    case AW_ANCHORS:
        if(n<12||(n-4)%4||(n-4)/4>AX_BATCH_LIMIT||!ax_csys_token(a[2])||!aw_mm(a,3,4,0,1000))return -1;
        for(int i=4;i<n;i+=4)if(!aw_frame_inputs(a,i)||!aw_mm(a,i+2,i+4,-1e6,1e6))return -1;
        return 1;
    case AW_WIRE_LENGTH:case AW_WIRE_BEND:{
        const int offset=kind==AW_WIRE_LENGTH?6:3;
        if(n<offset+2||(n-offset)%2||!aw_mm(a,2,offset,0,10000))return -1;
        for(int i=offset;i<n;i+=2){
            if(!aw_root_or_path(a[i])||wcsncmp(a[i+1],L"CURVE:",6)||!fd_name(a[i+1]+6))return -1;
            for(int j=offset;j<i;j+=2)if(!_wcsicmp(a[j],a[i])&&!_wcsicmp(a[j+1],a[i+1]))return -1;
        }
        return 1;
    }
    case AW_CHANNEL_FILL:
        if(n<12||n%2||!aw_path(a[2])||!ax_csys_token(a[3])||!aw_mm(a,8,9,0,10000)||!aw_mm(a,9,10,0,100))return -1;
        for(int i=4;i<8;i++)if(!ax_q_surface(a[i]))return -1;
        for(int i=10;i<n;i+=2)if(!qa_number(a[i],0.01,1000)||!ax_int_range(a[i+1],1,10000))return -1;
        return 1;
    case AW_DISPLAY_VIEW:case AW_BYPASS:case AW_AIRFLOW:
        if(!aw_path(a[2])||!ax_csys_token(a[3]))return -1;
        for(int i=4;i<8;i++)if(!ax_q_surface(a[i]))return -1;
        if(kind==AW_DISPLAY_VIEW)return n==12&&aw_face_inputs(a,8)&&aw_mm(a,10,12,-75,75)?1:-1;
        if(kind==AW_BYPASS)return n==11&&aw_face_inputs(a,8)&&aw_mm(a,10,11,0,100)?1:-1;
        if(n<10||n-10>AX_BATCH_LIMIT||!aw_mm(a,8,9,-1e6,1e6)||!aw_mm(a,9,10,0,1e10))return -1;
        for(int i=10;i<n;i++){
            if(!aw_path(a[i])||!wcscmp(a[2],a[i]))return -1;
            for(int j=10;j<i;j++)if(!wcscmp(a[j],a[i]))return -1;
        }
        return 1;
    case AW_VOLTAGE_ZONE:
        return (n==5||n==7)&&aw_path(a[2])&&aw_path(a[3])&&wcscmp(a[2],a[3])&&
            aw_mm(a,4,5,0,10000)&&(n==5||aw_face_inputs(a,5))?1:-1;
    case AW_RAIN_COVER:
        return n==11&&ax_csys_token(a[2])&&aw_two_faces(a,3)&&aw_mm(a,7,11,0,10000)?1:-1;
    case AW_SEAL_LOOP:
        if(n<13||n-10>AX_BATCH_LIMIT||!aw_two_faces(a,2)||!aw_mm(a,6,7,0.001,1000)||
           !aw_mm(a,7,9,0,100)||!aw_mm(a,9,10,0,100))return -1;
        for(int i=10;i<n;i++){
            if(!sf_id(a[i],L"EDGE:"))return -1;
            for(int j=10;j<i;j++)if(!wcscmp(a[i],a[j]))return -1;
        }
        return 1;
    case AW_SLOT_RULE:case AW_TEMPORARY:return n==2?1:-1;
    case AW_REQUIRED:case AW_TERMINALS:{
        const int stride=kind==AW_TERMINALS?2:3;
        if(n<3+stride||(n-3)%stride||!aw_root_or_path(a[2]))return -1;
        for(int i=3;i<n;i+=stride){
            if(!a[i][0]||!ax_int_range(a[i+stride-1],0,10000))return -1;
            if(kind==AW_REQUIRED&&(!a[i+1][0]||!_wcsicmp(a[i],a[i+1])||!wcscmp(a[i+2],L"0")))return -1;
        }
        return 1;
    }
    case AW_TORQUE:
        if(n<7||(n-2)%5)return -1;
        for(int i=2;i<n;i+=5)if(!aw_path(a[i])||!aw_mm(a,i+1,i+3,0,1e5)||!a[i+3][0]||!a[i+4][0])return -1;
        return 1;
    case AW_PRODUCTION_BOM:
        if(n<4||n%2)return -1;
        for(int i=2;i<n;i+=2){
            if(!a[i][0]||!ax_int_range(a[i+1],0,100000))return -1;
            for(int j=2;j<i;j+=2)if(!_wcsicmp(a[i],a[j]))return -1;
        }
        return 1;
    case AW_ISSUE_LIST:return (n==3||n==4)&&a[2][0]&&(n==3||dv_safe_leaf(a[3]))?1:-1;
    case AW_SHARED_MODELS:
        if(n<4||!aw_path(a[2]))return -1;
        for(int i=3;i<n;i++)if(!fd_name(a[i])||!_wcsicmp(a[1],a[i]))return -1;
        return 1;
    case AW_RAIL_CAPACITY:
        if(n<11||n-11>AX_BATCH_LIMIT||!ax_csys_token(a[2])||!aw_mm(a,3,9,-1e6,1e6)||!aw_mm(a,9,11,0.01,1e6))return -1;
        for(int i=11;i<n;i++)if(!aw_path(a[i]))return -1;
        return 1;
    case AW_FASTENER_SIDE:
        return n==10&&aw_face_inputs(a,2)&&ax_q_surface(a[4])&&aw_face_inputs(a,5)&&ax_q_surface(a[7])&&
            wcscmp(a[2],a[5])&&aw_mm(a,8,10,0,100)?1:-1;
    case AW_MIXED_SCREWS:return n==4&&aw_root_or_path(a[2])&&aw_mm(a,3,4,0.001,10000)?1:-1;
    case AW_ELECTRIC_LAYOUT:
        if(n<11||(n-8)%3||!ax_csys_token(a[2])||!aw_mm(a,3,7,-1e6,1e6)||!aw_mm(a,7,8,0,1000))return -1;
        for(int i=8;i<n;i+=3)if(!aw_path(a[i])||!ax_csys_token(a[i+1])||!ax_signed_axis(a[i+2])||a[i+2][1]==L'Z')return -1;
        return 1;
    case AW_INSTALL_ORDER:{
        if(n<4||!ax_int_range(a[2],1,AX_BATCH_LIMIT))return -1;
        int count,ids[AX_PATH_DEPTH];
        part_integer(a[2],&count);
        if(n<3+count||(n-3-count)%2)return -1;
        for(int i=3;i<n;i++){
            if(ax_path_parse(a[i],ids)!=1)return -1;
            if(i<3+count)for(int j=3;j<i;j++)if(!wcscmp(a[i],a[j]))return -1;
        }
        return 1;
    }
    case AW_TRANSPORT:
        if(n<12||(n-8)%4||!aw_root_or_path(a[2])||!ax_csys_token(a[3])||!aw_mm(a,4,8,0,10000))return -1;
        for(int i=8;i<n;i+=4)if((wcscmp(a[i],L"STRAIGHT_X")&&wcscmp(a[i],L"STRAIGHT_Y")&&wcscmp(a[i],L"TURN"))||
            !aw_mm(a,i+1,i+4,0.001,1e6))return -1;
        return 1;
    case AW_REWORK:
        return (n==5||n==6)&&fd_name(a[2])&&_wcsicmp(a[1],a[2])&&aw_mm(a,3,4,0,10000)&&
            aw_mm(a,4,5,0,180)&&(n==5||dv_safe_leaf(a[5]))?1:-1;
    case AW_METHOD_RECORD:case AW_METHOD_RECALL:
        return n==4&&aw_path(a[2])&&fd_name(a[3])?1:-1;
    case AW_TOOL_ACCESS:case AW_DOOR_FINAL:case AW_TERMINAL_ACCESS:case AW_PLUG_PATH:
    case AW_RAIL_SERVICE:case AW_SMALL_SERVICE:case AW_FILTER_SERVICE:case AW_INSTALL_PATH:
        return aw_service_valid(kind,n,a);
    case AW_DOOR_SHIM:
        if(n<9||(n-5)%4||!aw_mm(a,2,5,0,1000))return -1;
        for(int i=5;i<n;i+=4)if(!aw_two_faces(a,i))return -1;
        return 1;
    case AW_DOOR_WIRE:
        return n==12&&aw_frame_inputs(a,2)&&aw_path(a[4])&&ax_csys_token(a[5])&&
            !wcsncmp(a[6],L"AXIS:",5)&&fd_name(a[6]+5)&&aw_mm(a,7,9,-360,360)&&
            aw_mm(a,9,11,0,10000)&&ax_int_range(a[11],2,81)?1:-1;
    case AW_EDGE_WIRE:
        return n==8&&aw_path(a[4])&&sf_id(a[5],L"EDGE:")&&aw_root_or_path(a[2])&&!wcsncmp(a[3],L"CURVE:",6)&&
            fd_name(a[3]+6)&&aw_mm(a,6,8,0,1000)?1:-1;
    case AW_WIRE_CROSS:
        if(n!=11||!aw_mm(a,6,10,0,10000)||!aw_mm(a,10,11,0,90))return -1;
        for(int i=2;i<6;i+=2)if(!aw_root_or_path(a[i])||wcsncmp(a[i+1],L"CURVE:",6)||!fd_name(a[i+1]+6))return -1;
        return wcscmp(a[2],a[4])||_wcsicmp(a[3],a[5])?1:-1;
    case AW_CONNECTIONS:{
        wchar_t id[PART_TOKEN_MAX],csys[PART_TOKEN_MAX];
        if(n<4||n%2)return -1;
        for(int i=2;i<n;i++)if(!aw_connection_token(a[i],id,csys))return -1;
        return 1;
    }
    case AW_WELD_CONFLICT:
        if(n<5||(n-2)%3)return -1;
        for(int i=2;i<n;i+=3)if(!aw_path(a[i])||!aw_path(a[i+1])||!wcscmp(a[i],a[i+1])||
            !qa_number(a[i+2],0,1e8))return -1;
        return 1;
    case AW_WELD_TOOL:return aw_service_valid(kind,n,a);
    case AW_HIGHLIGHT:
        return (n==3&&(!wcscmp(a[2],L"CLEAR")||aw_path(a[2])))||
            (n==4&&aw_face_inputs(a,2))?1:-1;
    case AW_INTERFACE:
        if(n<8||(n-8)%5||!aw_path(a[2])||!ax_csys_token(a[3])||!ax_q_surface(a[4])||
           !qa_number(a[5],-1e6,1e6)||!qa_number(a[6],0,1000)||!qa_number(a[7],0,90))return -1;
        for(int i=8;i<n;i+=5){
            if(!ax_q_surface(a[i])||!aw_mm(a,i+1,i+3,-1e6,1e6)||!aw_mm(a,i+3,i+4,0,1000)||!aw_mm(a,i+4,i+5,0,90))return -1;
        }
        return 1;
    case AW_CHANGE_PREVIEW:
        return n==5&&aw_root_or_path(a[2])&&fd_name(a[3])&&qa_number(a[4],0.001,1e6)?1:-1;
    case AW_BEAM_LAYOUT:case AW_TRAY_LAYOUT:{
        const int beams=kind==AW_BEAM_LAYOUT;
        int ids[AX_PATH_DEPTH];
        if(n<9||n-8>AX_BATCH_LIMIT||!ax_csys_token(a[2])||!aw_mm(a,3,5,-1e6,1e6)||!qa_number(a[5],0,1e6)||
           (beams?!qa_number(a[6],0.001,1e6):!ax_int_range(a[6],2,AX_BATCH_LIMIT))||
           (wcscmp(a[7],L"PREVIEW")&&wcscmp(a[7],L"APPLY")))return -1;
        for(int i=8;i<n;i++){
            if(ax_path_parse(a[i],ids)!=1)return -1;
            for(int j=8;j<i;j++)if(!wcscmp(a[j],a[i]))return -1;
        }
        return 1;
    }
    case AW_INTERFACE_DRAW:
        if(n<7||(n-4)%3||!dv_safe_leaf(a[2])||!ax_csys_token(a[3]))return -1;
        for(int i=4;i<n;i+=3)if(!fd_name(a[i])||!aw_frame_inputs(a,i+1))return -1;
        return 1;
    case AW_ORDER_AUDIT:return n==3&&aw_csv_leaf(a[2])?1:-1;
    case AW_DROP_FASTENER:case AW_LABEL_VIEW:case AW_WATER_PATH:case AW_RELEASE_DROP:
        if(n<7||!aw_frame_inputs(a,2)||!ax_signed_axis(a[4])||!qa_number(a[5],0.001,1e6))return -1;
        for(int i=6;i<n;i++)if(!aw_path(a[i])||!wcscmp(a[i],a[2]))return -1;
        return 1;
    case AW_GLASS_CORNER:
        return n==11&&aw_path(a[2])&&ax_csys_token(a[3])&&ax_q_surface(a[4])&&sf_id(a[5],L"EDGE:")&&
            aw_face_inputs(a,6)&&sf_id(a[8],L"EDGE:")&&aw_mm(a,9,11,0,1000)?1:-1;
    case AW_GLUE_SPACE:
        if(n<13||(n-5)%8||!aw_mm(a,2,5,0.001,1000))return -1;
        for(int i=5;i<n;i+=4)if(!aw_two_faces(a,i))return -1;
        return 1;
    case AW_GLASS_PRESSURE:
        if(n<5||!aw_path(a[2])||!qa_number(a[3],0,1000))return -1;
        for(int i=4;i<n;i++)if(!aw_path(a[i])||!wcscmp(a[i],a[2]))return -1;
        return 1;
    case AW_HEAT_ZONE:
        if(n<7||!aw_face_inputs(a,2)||!qa_number(a[4],0.001,1e6)||!qa_number(a[5],0,10000))return -1;
        for(int i=6;i<n;i++)if(!aw_path(a[i])||!wcscmp(a[i],a[2]))return -1;
        return 1;
    case AW_AIR_PAIR:
        return n==10&&aw_frame_inputs(a,2)&&ax_signed_axis(a[4])&&aw_frame_inputs(a,5)&&ax_signed_axis(a[7])&&
            qa_number(a[8],0.001,1e6)&&qa_number(a[9],0.01,90)?1:-1;
    case AW_ROUTE_WRITE:
        if(n<16||n>20||!fd_name(a[2])||wcslen(a[2])>22||!ax_csys_token(a[3])||!qa_number(a[4],0.001,1000)||
           !qa_number(a[5],0,1000)||!aw_frame_inputs(a,6)||!aw_frame_inputs(a,8)||!aw_mm(a,10,16,-1e6,1e6))return -1;
        for(int i=16;i<n;i++)if(!aw_path(a[i]))return -1;
        return 1;
    case AW_CLIP_WRITE:
        return n==12&&dv_safe_leaf(a[2])&&ax_csys_token(a[3])&&aw_root_or_path(a[4])&&
            !wcsncmp(a[5],L"CURVE:",6)&&fd_name(a[5]+6)&&ax_csys_token(a[6])&&qa_number(a[7],0,1e6)&&
            qa_number(a[8],0.001,1e6)&&qa_number(a[9],0,1000)&&ax_int_range(a[10],1,AX_BATCH_LIMIT)&&
            (!wcscmp(a[11],L"NONE")||aw_path(a[11]))?1:-1;
    case AW_GLASS_BLOCKS:
        return n==10&&aw_path(a[2])&&ax_csys_token(a[3])&&dv_safe_leaf(a[4])&&ax_csys_token(a[5])&&
            qa_number(a[6],5,45)&&aw_mm(a,7,9,0,1000)&&(!wcscmp(a[9],L"BOTTOM")||!wcscmp(a[9],L"SIDES"))?1:-1;
    case AW_DOOR_HAND:{
        if(n<23||(n-7)%4||!fd_name(a[2])||!_wcsicmp(a[1],a[2])||!ax_csys_token(a[3])||
           (wcscmp(a[4],L"LEFT")&&wcscmp(a[4],L"RIGHT"))||!qa_number(a[5],-1e6,1e6)||!qa_number(a[6],0,10000))return -1;
        const wchar_t *const roles[]={L"HINGE",L"LATCH",L"LIMIT",L"WIRE"};
        int mask=0;
        for(int i=7;i<n;i+=4){
            int role=0;
            while(role<_countof(roles)&&wcscmp(roles[role],a[i]))role++;
            if(role==_countof(roles)||!aw_frame_inputs(a,i+1)||(wcscmp(a[i+3],L"L")&&wcscmp(a[i+3],L"R")))return -1;
            mask|=1<<role;
        }
        return mask==15?1:-1;
    }
    case AW_GROUND_BOND:
        return n==9&&aw_face_inputs(a,2)&&ax_q_surface(a[4])&&aw_face_inputs(a,5)&&ax_q_surface(a[7])&&
            wcscmp(a[2],a[5])&&qa_number(a[8],0,1000)?1:-1;
    case AW_CONNECTOR_OPENING:
        if(n!=12||!aw_path(a[2])||!ax_csys_token(a[3])||!aw_path(a[8])||ax_axis_index(a[9])<0||
           !qa_number(a[10],-89,89)||!qa_number(a[11],0,1000))return -1;
        for(int i=4;i<8;i++)if(!ax_q_surface(a[i]))return -1;
        return 1;
    case AW_POSTURE:
        return n>=12&&n<=6+2*AX_BATCH_LIMIT&&(n-6)%2==0&&ax_csys_token(a[2])&&ax_axis_index(a[3])>=0&&
            qa_number(a[4],-180,180)&&qa_number(a[5],0,10000)&&aw_mm(a,6,n,-1e6,1e6)?1:-1;
    case AW_REASSEMBLE:
        return n==8&&aw_path(a[2])&&ax_csys_token(a[3])&&aw_frame_inputs(a,4)&&qa_number(a[6],0,10000)&&
            qa_number(a[7],0,180)?1:-1;
    case AW_WORKPOINTS:
        if(n<3)return -1;
        for(int i=2;i<n;i++)if(!aw_path(a[i]))return -1;
        return 1;
    case AW_REMOVAL_DRAW:{
        if(n<8||(n-4)%4||!dv_safe_leaf(a[2])||!ax_csys_token(a[3]))return -1;
        wchar_t id[PART_TOKEN_MAX],csys[PART_TOKEN_MAX];
        for(int i=4;i<n;i+=4)if(!aw_path(a[i])||!ax_signed_axis(a[i+1])||
            (wcscmp(a[i+2],L"NONE")&&!aw_path(a[i+2]))||
            (wcscmp(a[i+3],L"NONE")&&!aw_connection_token(a[i+3],id,csys)))return -1;
        return 1;
    }
    case AW_ROOM_CLEARANCE:
        return n==16&&aw_root_or_path(a[2])&&ax_csys_token(a[3])&&aw_mm(a,4,10,-1e6,1e6)&&aw_mm(a,10,16,0,1e6)&&
            wcstod(a[4],NULL)<wcstod(a[5],NULL)&&wcstod(a[6],NULL)<wcstod(a[7],NULL)&&
            wcstod(a[8],NULL)<wcstod(a[9],NULL)?1:-1;
    case AW_HANDLING:
        if(n<13||!ax_csys_token(a[2])||!aw_frame_inputs(a,3)||!aw_frame_inputs(a,5)||!aw_frame_inputs(a,7)||
           !aw_mm(a,9,13,0,1e6)||wcstod(a[10],NULL)>wcstod(a[11],NULL))return -1;
        for(int i=13;i<n;i++)if(!aw_path(a[i]))return -1;
        return 1;
    case AW_ORDER_IMPACT:
        if(n<4||!aw_path(a[2]))return -1;
        for(int i=3;i<n;i++)if(!fd_name(a[i])||!_wcsicmp(a[1],a[i]))return -1;
        return 1;
    case AW_PRE_RELEASE:
        return n==7&&(!wcscmp(a[2],L"OUTDOOR")||!wcscmp(a[2],L"INDOOR")||!wcscmp(a[2],L"CONTROL"))&&
            ax_csys_token(a[3])&&aw_mm(a,4,7,0.001,1e6)?1:-1;
    case AW_OUTDOOR_ACCESS:case AW_CLOSE_BLOCKS:case AW_TEMP_SUPPORT:case AW_REMOVE_MIN:case AW_CASTER_BRAKE:
        return aw_service_valid(kind,n,a);
    default:return 0;
    }
}
