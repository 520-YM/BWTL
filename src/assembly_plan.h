#ifndef BWTL_ASSEMBLY_PLAN_H
#define BWTL_ASSEMBLY_PLAN_H

#define AX_PATH_DEPTH 16
enum AssemblyCommand {
    AX_NONE,AX_TREE,AX_INSERT,AX_CSYS,AX_MOVE,AX_STATE,
    AX_CLEARANCE,AX_INTERFERENCE,AX_MASS,AX_BOM,AX_STEP,
    AX_REFS,AX_PLACEMENT,AX_FIX,AX_MATE,AX_PARALLEL,AX_OFFSET,AX_COAX,AX_THREE,AX_FASTENER,AX_EDIT_OFFSET,
    AX_INSERT_ASM,AX_NEW_ASM,AX_COPY_COMPONENT,AX_REPLACE_COMPONENT,AX_DELETE_COMPONENT,
    AX_ROTATE_COMPONENT,AX_FLIP_COMPONENT,AX_UNFIX_COMPONENT,AX_EDIT_REF,AX_REPAIR_REF,
    AX_ARRAY_LINEAR,AX_ARRAY_GRID,AX_ARRAY_CIRCULAR,AX_HOLE_ARRAY,AX_MIRROR_COMPONENT,
    AX_FASTENER_STACK,AX_PRESS_HARDWARE,AX_FASTENER_FIT,AX_REPLACE_BATCH,AX_FASTENER_AUDIT,
    AX_SCREEN_MOUNT,AX_GLASS_MOUNT,AX_POWER_MOUNT,AX_PCB_MOUNT,AX_FAN_MOUNT,
    AX_FILTER_MOUNT,AX_RAIL_MOUNT,AX_LATCH_MOUNT,AX_HINGE_MOUNT,AX_SEAL_CHECK,
    AX_JOINT_PIN,AX_JOINT_SLIDER,AX_JOINT_CYLINDER,AX_ROTATION_LIMIT,AX_TRAVEL_LIMIT,AX_MOTION_PRESET,
    AX_MOTION_SWEEP,AX_DOOR_SWEEP,AX_TRAY_SWEEP,AX_TOOL_SWEEP,
    AX_LAYOUT_DATUM,AX_LAYOUT_ENVELOPE,AX_LAYOUT_SKELETON,AX_LAYOUT_LINK,AX_LAYOUT_INTERFACE,
    AX_LAYOUT_COPY,AX_LAYOUT_DIM,AX_LAYOUT_REFS,AX_LAYOUT_AUDIT,AX_LAYOUT_COMPARE,
    AX_FIELD_CUT,AX_FIELD_HOLE,AX_FIELD_GRID,AX_FIELD_TRANSFER,AX_FIELD_LAYERS,AX_FIELD_WINDOW,AX_FIELD_ROUTE,
    AX_FIELD_SHIM,AX_FIELD_MARK,AX_FIELD_AUDIT,
    AX_Q_REGEN,AX_Q_GAPS,AX_Q_INTERF,AX_Q_HOLES,AX_Q_FLUSH,AX_Q_COPLANE,AX_Q_STACK,AX_Q_SUPPORT,AX_Q_SPACE,AX_Q_REPORT,
    AX_V_REP,AX_V_ISOLATE,AX_V_VIEW,AX_V_SECTION,AX_V_EXPLODE,AX_V_SEQUENCE,AX_V_CONFIG,AX_V_APPLY,AX_V_COMPARE,AX_V_ORDER,
    AX_B_TREE,AX_B_BUY,AX_B_DIFF,AX_B_CHECK,AX_B_FAST,AX_B_WIRE,AX_B_SPARE,AX_B_ROUTE,AX_B_NATIVE,AX_B_PACK
};

static int ax_kind(const wchar_t *verb){
    static const wchar_t *const names[]={
        L"ASM_TREE",L"ASM_INSERT",L"ASM_CSYS",L"ASM_MOVE",L"ASM_STATE",
        L"ASM_CLEARANCE",L"ASM_INTERFERENCE",L"ASM_MASS",L"ASM_BOM",L"ASM_STEP",
        L"ASM_REFS",L"ASM_PLACEMENT",L"ASM_FIX",L"ASM_MATE",L"ASM_PARALLEL",L"ASM_OFFSET",
        L"ASM_COAX",L"ASM_THREE",L"ASM_FASTENER",L"ASM_EDIT_OFFSET",
        L"ASM_INSERT_ASM",L"ASM_NEW_ASM",L"ASM_COPY_COMPONENT",L"ASM_REPLACE_COMPONENT",L"ASM_DELETE_COMPONENT",
        L"ASM_ROTATE_COMPONENT",L"ASM_FLIP_COMPONENT",L"ASM_UNFIX_COMPONENT",L"ASM_EDIT_REF",L"ASM_REPAIR_REF",
        L"ASM_ARRAY_LINEAR",L"ASM_ARRAY_GRID",L"ASM_ARRAY_CIRCULAR",L"ASM_HOLE_ARRAY",L"ASM_MIRROR_COMPONENT",
        L"ASM_FASTENER_STACK",L"ASM_PRESS_HARDWARE",L"ASM_FASTENER_FIT",L"ASM_REPLACE_BATCH",L"ASM_FASTENER_AUDIT",
        L"ASM_SCREEN_MOUNT",L"ASM_GLASS_MOUNT",L"ASM_POWER_MOUNT",L"ASM_PCB_MOUNT",L"ASM_FAN_MOUNT",
        L"ASM_FILTER_MOUNT",L"ASM_RAIL_MOUNT",L"ASM_LATCH_MOUNT",L"ASM_HINGE_MOUNT",L"ASM_SEAL_CHECK",
        L"ASM_JOINT_PIN",L"ASM_JOINT_SLIDER",L"ASM_JOINT_CYLINDER",L"ASM_ROTATION_LIMIT",L"ASM_TRAVEL_LIMIT",
        L"ASM_MOTION_PRESET",L"ASM_MOTION_SWEEP",L"ASM_DOOR_SWEEP",L"ASM_TRAY_SWEEP",L"ASM_TOOL_SWEEP",
        L"ASM_LAYOUT_DATUM",L"ASM_LAYOUT_ENVELOPE",L"ASM_LAYOUT_SKELETON",L"ASM_LAYOUT_LINK",
        L"ASM_LAYOUT_INTERFACE",L"ASM_LAYOUT_COPY",L"ASM_LAYOUT_DIM",L"ASM_LAYOUT_REFS",L"ASM_LAYOUT_AUDIT",L"ASM_LAYOUT_COMPARE",L"ASM_FIELD_CUT",L"ASM_FIELD_HOLE",L"ASM_FIELD_GRID",L"ASM_FIELD_TRANSFER",
        L"ASM_FIELD_LAYERS",L"ASM_FIELD_WINDOW",L"ASM_FIELD_ROUTE",L"ASM_FIELD_SHIM",L"ASM_FIELD_MARK",L"ASM_FIELD_AUDIT",
        L"ASM_Q_REGEN",L"ASM_Q_GAPS",L"ASM_Q_INTERF",L"ASM_Q_HOLES",L"ASM_Q_FLUSH",L"ASM_Q_COPLANE",
        L"ASM_Q_STACK",L"ASM_Q_SUPPORT",L"ASM_Q_SPACE",L"ASM_Q_REPORT",
        L"ASM_V_REP",L"ASM_V_ISOLATE",L"ASM_V_VIEW",L"ASM_V_SECTION",L"ASM_V_EXPLODE",
        L"ASM_V_SEQUENCE",L"ASM_V_CONFIG",L"ASM_V_APPLY",L"ASM_V_COMPARE",L"ASM_V_ORDER",
        L"ASM_B_TREE",L"ASM_B_BUY",L"ASM_B_DIFF",L"ASM_B_CHECK",L"ASM_B_FAST",L"ASM_B_WIRE",
        L"ASM_B_SPARE",L"ASM_B_ROUTE",L"ASM_B_NATIVE",L"ASM_B_PACK"
    };
    for(int i=0;i<_countof(names);i++)
        if(!wcscmp(verb,names[i]))return i+1;
    return AX_NONE;
}

static int ax_request(const wchar_t *text){
    static const wchar_t *const terms[]={L"ASM_",L"组件树",L"放入装配",L"装入组件",L"装配坐标系",
        L"组件位置",L"抑制组件",L"恢复组件",L"装配间隙",L"组件间隙",L"干涉检查",L"装配质量",
        L"装配BOM",L"装配 BOM",L"装配STEP",L"装配 STEP",L"定位参照",L"定位状态",L"固定组件",L"平面贴合",L"平面平行",
        L"平面偏距",L"轴孔同轴",L"三面定位",L"紧固件定位",L"定位偏距",L"安装面贴",L"子装配",L"复制组件",L"替换组件",L"删除组件",L"旋转组件",
        L"翻转组件",L"解除固定",L"定位参照",L"组件阵列",L"孔阵列",L"镜像布置",L"紧固件",L"压铆件",L"屏幕与窗口",L"玻璃与面框",L"电源与安装板",L"PCB",L"风扇与风口",
        L"滤网",L"导轨",L"锁扣",L"铰链",L"密封条",L"销轴连接",L"滑块连接",L"圆柱连接",L"角度限位",L"行程限位",
        L"机构位置",L"运动干涉",L"开启空间",L"抽出空间",L"工具操作空间",L"安装基准",L"空间包络",L"骨架布局",
        L"骨架驱动",L"模块安装接口",L"公共安装参照",L"总装控制尺寸",L"跨零件外部参照",L"循环引用",L"断链",L"装配布局",L"装配切除",L"装配通孔",L"装配孔组",L"传递孔位",L"分层孔型",L"避让窗口",L"过线孔",L"间隙生成垫片",L"装配定位标记",L"装配加工影响范围",L"装配重生成诊断",L"区域间隙",L"分区域间隙",L"干涉白名单",L"白名单筛选干涉",L"紧固孔对位",L"面板齐平",L"安装面共面",L"装配公差",L"重心与支撑",L"安装空间",L"装配检查报告",L"装配质量检查",L"装配质量报告",L"检查结果汇总",L"简化表示",L"隔离显示",L"装配视图",L"装配视角",L"装配剖切",L"爆炸视图",L"装配顺序",L"产品配置",L"配置表",L"切换组件型号",L"比较不同配置",L"订单专用装配",L"订单装配",L"分层BOM",L"分层 BOM",L"采购BOM",L"采购 BOM",L"采购分类",L"BOM资料",L"BOM 资料",L"版BOM",L"版 BOM",L"紧固件用量",L"附件安装表",L"线束安装表",L"维修备件",L"装配工序卡",L"原生装配",L"装配交付包"};
    for(int i=0;i<_countof(terms);i++)if(wcsstr(text,terms[i]))return 1;
    return 0;
}

/* Component IDs are occurrence paths, not model names or filesystem paths. */
static int ax_path_parse(const wchar_t *token,int ids[AX_PATH_DEPTH]){
    if(wcsncmp(token,L"ID:",3))return 0;
    const wchar_t *p=token+3;
    int count=0;
    for(;;){
        int id=0,digits=0;
        while(*p>=L'0'&&*p<=L'9'){
            int digit=*p-L'0';
            if(id>(2147483647-digit)/10)return 0;
            id=id*10+digit;
            p++;
            digits++;
        }
        if(!digits||!id||count==AX_PATH_DEPTH)return 0;
        ids[count++]=id;
        if(!*p)return count;
        if(*p++!=L'/')return 0;
    }
}

static int ax_csys_token(const wchar_t *token){return !wcsncmp(token,L"CSYS:",5)&&fd_name(token+5);}

static int ax_delete_preview(const PartCommand *cmd){
    return !wcscmp(cmd->arg[0],L"ASM_DELETE_COMPONENT")&&cmd->count==5&&!wcscmp(cmd->arg[4],L"CHECK");
}

static int ax_read_diagnostic(int kind){return kind==AX_TREE||kind==AX_REFS||kind==AX_PLACEMENT||kind==AX_Q_REGEN||kind==AX_Q_REPORT;}
static int ax_motion_inspection_request(const wchar_t *text){
    const wchar_t *terms[]={L"运动干涉",L"开启空间",L"抽出空间",L"工具操作空间",
        L"第287项",L"第288项",L"第289项",L"第290项",
        L"ASM_MOTION_SWEEP",L"ASM_DOOR_SWEEP",L"ASM_TRAY_SWEEP",L"ASM_TOOL_SWEEP"};
    for(int i=0;i<_countof(terms);i++)if(wcsstr(text,terms[i]))return 1;
    return 0;
}
static int ax_mutates(int kind){return (kind>=AX_INSERT&&kind<=AX_STATE)||(kind>=AX_FIX&&kind<AX_SEAL_CHECK&&kind!=AX_FASTENER_FIT&&kind!=AX_FASTENER_AUDIT)||(kind>=AX_JOINT_PIN&&kind<=AX_MOTION_PRESET)||(kind>=AX_LAYOUT_DATUM&&kind<=AX_LAYOUT_DIM)||(kind>=AX_FIELD_CUT&&kind<=AX_FIELD_MARK)||(kind>=AX_V_REP&&kind<=AX_V_ORDER&&kind!=AX_V_COMPARE);}

static int ax_ref_token(const wchar_t *token){
    int id;
    return (!wcsncmp(token,L"PLANE:",6)&&fd_name(token+6))||
           (!wcsncmp(token,L"AXIS:",5)&&fd_name(token+5))||
           (!wcsncmp(token,L"SURFACE:",8)&&part_integer(token+8,&id)&&id>0);
}

static int ax_offset_number(const wchar_t *token){
    double value;
    return qa_number(token,-1e6,1e6)&&part_number(token,&value)&&fabs(value)>=0.001;
}

static int ax_axis_index(const wchar_t *token){
    return wcslen(token)==1&&wcschr(L"XYZ",token[0])?token[0]-L'X':-1;
}
static int ax_int_range(const wchar_t *token,int min,int max){
    int value;
    return part_integer(token,&value)&&value>=min&&value<=max;
}
#define AX_BATCH_LIMIT 24
static int ax_signed_axis(const wchar_t *token){
    return wcslen(token)==2&&(token[0]==L'+'||token[0]==L'-')&&ax_axis_index(token+1)>=0;
}

static int ax_field_plane_token(const wchar_t *value){
    return !wcsncmp(value,L"PLANE:",6)&&fd_name(value+6);
}
static int ax_field_direction_token(const wchar_t *value,int symmetric){
    return !wcscmp(value,L"POS")||!wcscmp(value,L"NEG")||(symmetric&&!wcscmp(value,L"BOTH"));
}
static int ax_field_operation_valid(int kind,int count,wchar_t args[][PART_TOKEN_MAX]){
    int path[AX_PATH_DEPTH],targets=0;
    if(count<7||!fd_name(args[2])||wcslen(args[2])>21)return -1;
    if(kind==AX_FIELD_CUT){
        if(!fd_name(args[3])||!ax_field_direction_token(args[4],1)||!qa_number(args[5],0,10000))return -1;
        targets=6;
    }else if(kind==AX_FIELD_LAYERS){
        if(count<14||count>26||(count-8)%6||!ax_field_plane_token(args[3])||!ax_field_direction_token(args[4],0))return -1;
        for(int i=5;i<8;i++)if(!qa_number(args[i],-10000,10000))return -1;
        for(int i=8;i<count;i+=6){
            double dia,aux,depth;
            if(!ax_path_parse(args[i],path)||!ax_field_plane_token(args[i+1])||
               !qa_number(args[i+3],0.1,1000)||!qa_number(args[i+4],0,1000)||!qa_number(args[i+5],0,10000))return -1;
            part_number(args[i+3],&dia);part_number(args[i+4],&aux);part_number(args[i+5],&depth);
            if(!wcscmp(args[i+2],L"CBORE")){
                if(aux<=dia||depth<0.05)return -1;
            }else if(!wcscmp(args[i+2],L"THROUGH")){
                if(aux||depth)return -1;
            }else if(wcscmp(args[i+2],L"CORE")||aux||(depth>0&&depth<0.05))return -1;
        }
        return 1;
    }else{
        if(!ax_field_plane_token(args[3])||!ax_field_plane_token(args[4])||
           !ax_field_direction_token(args[5],1))return -1;
        if(kind==AX_FIELD_HOLE||kind==AX_FIELD_GRID){
            if(count<(kind==AX_FIELD_GRID?15:11)||!qa_number(args[9],0.1,1000))return -1;
            for(int i=6;i<=8;i++)if(!qa_number(args[i],-10000,10000))return -1;
            targets=10;
            if(kind==AX_FIELD_GRID){
                if(!ax_int_range(args[10],1,16)||!ax_int_range(args[11],1,16)||
                   !qa_number(args[12],0,10000)||!qa_number(args[13],0,10000))return -1;
                targets=14;
            }
        }else if(kind==AX_FIELD_TRANSFER){
            if(count<10||!ax_path_parse(args[6],path)||wcsncmp(args[7],L"SURFACE:",8)||
                !ax_ref_token(args[7])||!qa_number(args[8],0,1000))return -1;
            targets=9;
        }else if(kind==AX_FIELD_WINDOW){
            if(count<9||!ax_path_parse(args[6],path)||!qa_number(args[7],0,1000))return -1;
            targets=8;
        }else{
            if(count<9||wcsncmp(args[6],L"CURVE:",6)||!fd_name(args[6]+6)||
                !qa_number(args[7],0.1,1000))return -1;
            targets=8;
        }
    }
    if(count<=targets||count-targets>12)return -1;
    for(int i=targets;i<count;i++)if(!ax_path_parse(args[i],path))return -1;
    return 1;
}

static int ax_q_zone(const wchar_t *token){
    static const wchar_t *const names[]={L"SCREEN",L"GLASS",L"PCB",L"ENCLOSURE"};
    for(int i=0;i<_countof(names);i++)if(!wcscmp(names[i],token))return i;
    return -1;
}

static int ax_q_surface(const wchar_t *token){return sf_id(token,L"SURFACE:");}

static int ax_q_valid(int kind,int count,wchar_t args[][PART_TOKEN_MAX]){
    int path[AX_PATH_DEPTH];
    switch(kind){
    case AX_Q_REGEN:return count==2?1:-1;
    case AX_Q_REPORT:return count==2||(count==3&&dv_safe_leaf(args[2]))?1:-1;
    case AX_Q_GAPS:
        if(count<6||(count-2)%4)return -1;
        for(int i=2;i<count;i+=4)
            if(ax_q_zone(args[i])<0||!ax_path_parse(args[i+1],path)||!ax_path_parse(args[i+2],path)||
               !wcscmp(args[i+1],args[i+2])||!qa_number(args[i+3],0,10000))return -1;
        return 1;
    case AX_Q_INTERF:
        if((count-2)%3)return -1;
        for(int i=2;i<count;i+=3)
            if(!ax_path_parse(args[i],path)||!ax_path_parse(args[i+1],path)||!wcscmp(args[i],args[i+1])||
               !qa_number(args[i+2],0,1e12))return -1;
        return 1;
    case AX_Q_HOLES:case AX_Q_FLUSH:
        return count==8&&ax_path_parse(args[2],path)&&ax_q_surface(args[3])&&ax_path_parse(args[4],path)&&
            ax_q_surface(args[5])&&(wcscmp(args[2],args[4])||wcscmp(args[3],args[5]))&&
            qa_number(args[6],0,10000)&&qa_number(args[7],0,90)?1:-1;
    case AX_Q_COPLANE:
        if(count<10||(count-4)%2||!qa_number(args[2],0,10000)||!qa_number(args[3],0,90))return -1;
        for(int i=4;i<count;i+=2){
            if(!ax_path_parse(args[i],path)||!ax_q_surface(args[i+1]))return -1;
            for(int j=4;j<i;j+=2)if(!wcscmp(args[i],args[j])&&!wcscmp(args[i+1],args[j+1]))return -1;
        }
        return 1;
    case AX_Q_STACK:
        if(count<9||(count-4)%5||!qa_number(args[2],-1e6,1e6)||!qa_number(args[3],-1e6,1e6))return -1;
        for(int i=4;i<count;i+=5){
            if((wcscmp(args[i],L"ADD")&&wcscmp(args[i],L"SUB"))||!fd_name(args[i+1])||
               !qa_number(args[i+2],0,1e6)||!qa_number(args[i+3],0,1e5)||!qa_number(args[i+4],0,1e5))return -1;
        }
        return 1;
    case AX_Q_SUPPORT:
        if(count<10||count>28||(count-4)%2||!ax_csys_token(args[2])||!qa_number(args[3],0,1e5))return -1;
        for(int i=4;i<count;i++)if(!qa_number(args[i],-1e6,1e6))return -1;
        return 1;
    case AX_Q_SPACE:
        if(count<9||!ax_csys_token(args[2]))return -1;
        for(int i=3;i<9;i++)if(!qa_number(args[i],-1e6,1e6))return -1;
        for(int i=9;i<count;i++)if(!ax_path_parse(args[i],path))return -1;
        return 1;
    default:return -1;
    }
}

#define AX_CONFIG_LIMIT 5

static int ax_v_ids(int count,wchar_t args[][PART_TOKEN_MAX],int start,int stride){
    int seen[PART_ARG_MAX],ids[AX_PATH_DEPTH],used=0;
    for(int i=start;i<count;i+=stride){
        if(ax_path_parse(args[i],ids)!=1)return -1;
        for(int j=0;j<used;j++)if(seen[j]==ids[0])return -1;
        seen[used++]=ids[0];
    }
    return used?1:-1;
}

static int ax_v_valid(int kind,int count,wchar_t args[][PART_TOKEN_MAX]){
    int ids[AX_PATH_DEPTH];
    switch(kind){
    case AX_V_REP:case AX_V_ISOLATE:case AX_V_SEQUENCE:
        if(kind==AX_V_ISOLATE&&count==3&&!wcscmp(args[2],L"MASTER"))return 1;
        if(kind==AX_V_ISOLATE&&count==4&&!wcscmp(args[2],L"APPLY"))return fd_name(args[3])?1:-1;
        if(count<4||(kind==AX_V_SEQUENCE?!dv_safe_leaf(args[2]):!fd_name(args[2]))||
            (kind==AX_V_ISOLATE&&!wcscmp(args[2],L"APPLY")))return -1;
        return ax_v_ids(count,args,3,1);
    case AX_V_VIEW:
        return count==4&&fd_name(args[2])&&(!wcscmp(args[3],L"SAVE")||!wcscmp(args[3],L"APPLY"))?1:-1;
    case AX_V_SECTION:
        if(count==4)return fd_name(args[2])&&!wcscmp(args[3],L"APPLY")?1:-1;
        return count==5&&fd_name(args[2])&&!wcsncmp(args[3],L"PLANE:",6)&&fd_name(args[3]+6)&&
            qa_number(args[4],-1e5,1e5)?1:-1;
    case AX_V_EXPLODE:
        if(count==3)return !wcscmp(args[2],L"RESET")?1:-1;
        if(count==4)return fd_name(args[2])&&!wcscmp(args[3],L"APPLY")?1:-1;
        if(count<7||(count-3)%4||!fd_name(args[2])||!wcscmp(args[2],L"RESET")||ax_v_ids(count,args,3,4)!=1)return -1;
        for(int i=3;i<count;i+=4){
            double offset[3];
            for(int j=0;j<3;j++)
                if(!qa_number(args[i+j+1],-1e5,1e5)||!part_number(args[i+j+1],&offset[j]))return -1;
            if(!offset[0]&&!offset[1]&&!offset[2])return -1;
        }
        return 1;
    case AX_V_CONFIG:
        if(count<8||(count-3)%5||(count-3)/5>AX_CONFIG_LIMIT||!dv_safe_leaf(args[2]))return -1;
        for(int i=3;i<count;i+=5)
            if(!fd_name(args[i])||ax_path_parse(args[i+1],ids)!=1||!fd_name(args[i+2])||!fd_name(args[i+3])||
               !_wcsicmp(args[i+2],args[i+3])||!ax_csys_token(args[i+4]))return -1;
        return 1;
    case AX_V_APPLY:
        return count==4&&dv_safe_leaf(args[2])&&fd_name(args[3])?1:-1;
    case AX_V_ORDER:
        return count==6&&dv_safe_leaf(args[2])&&fd_name(args[3])&&fd_name(args[4])&&dv_safe_leaf(args[5])?1:-1;
    default:return -1;
    }
}

static int ax_pack_input_leaf(const wchar_t *leaf,const wchar_t *extension){
    const wchar_t *dot=wcsrchr(leaf,L'.');
    size_t length=wcslen(leaf);
    if(!dot||dot==leaf||length>120||leaf[0]==L'.'||leaf[0]==L' '||dot[-1]==L' '||_wcsicmp(dot,extension))return 0;
    const wchar_t *first_dot=wcschr(leaf,L'.');
    size_t base_length=(size_t)(first_dot-leaf);
    if((base_length==3&&(!_wcsnicmp(leaf,L"CON",3)||!_wcsnicmp(leaf,L"PRN",3)||!_wcsnicmp(leaf,L"AUX",3)||!_wcsnicmp(leaf,L"NUL",3)))||
       (base_length==4&&(!_wcsnicmp(leaf,L"COM",3)||!_wcsnicmp(leaf,L"LPT",3))))return 0;
    for(const wchar_t *p=leaf;*p;p++)
        if(*p<32||wcschr(L"\\/:*?\"<>|",*p))return 0;
    return 1;
}

static int ax_b_plan_valid(int kind,int count,wchar_t args[][PART_TOKEN_MAX]){
    if(kind==AX_B_CHECK&&count==2)return 1;
    if(count<3||!dv_safe_leaf(args[2]))return -1;
    if(kind==AX_B_DIFF)return count==4&&dv_safe_leaf(args[3])&&_wcsicmp(args[1],args[3])?1:-1;
    if(kind==AX_B_PACK)return count==5&&ax_pack_input_leaf(args[3],L".pdf")&&ax_pack_input_leaf(args[4],L".txt")?1:-1;
    if(kind!=AX_B_ROUTE)return count==3?1:-1;
    if(count<4)return -1;
    for(int i=3;i<count;i++){
        int ids[AX_PATH_DEPTH],depth=ax_path_parse(args[i],ids);
        if(!depth)return -1;
        for(int j=3;j<i;j++){
            int prior[AX_PATH_DEPTH],old_depth=ax_path_parse(args[j],prior);
            if(depth==old_depth&&!memcmp(ids,prior,(size_t)depth*sizeof(*ids)))return -1;
        }
    }
    return 1;
}

static int assembly_plan_valid(int count,wchar_t args[][PART_TOKEN_MAX]){
    int kind=ax_kind(args[0]),path[AX_PATH_DEPTH];
    switch(kind){
    case AX_Q_REGEN:case AX_Q_GAPS:case AX_Q_INTERF:case AX_Q_HOLES:case AX_Q_FLUSH:
    case AX_Q_COPLANE:case AX_Q_STACK:case AX_Q_SUPPORT:case AX_Q_SPACE:case AX_Q_REPORT:
        return ax_q_valid(kind,count,args);
    case AX_V_REP:case AX_V_ISOLATE:case AX_V_VIEW:case AX_V_SECTION:case AX_V_EXPLODE:
    case AX_V_SEQUENCE:case AX_V_CONFIG:case AX_V_APPLY:case AX_V_ORDER:
        return ax_v_valid(kind,count,args);
    case AX_B_TREE:case AX_B_BUY:case AX_B_DIFF:case AX_B_CHECK:case AX_B_FAST:case AX_B_WIRE:
    case AX_B_SPARE:case AX_B_ROUTE:case AX_B_NATIVE:case AX_B_PACK:
        return ax_b_plan_valid(kind,count,args);
    case AX_NONE:return 0;
    case AX_TREE:case AX_MASS:return count==2?1:-1;
    case AX_INSERT:
        if(count!=6||!dv_safe_leaf(args[2])||!_wcsicmp(args[1],args[2]))return -1;
        for(int i=3;i<6;i++)if(!qa_number(args[i],-1e6,1e6))return -1;
        return 1;
    case AX_CSYS:
        return count==5&&ax_path_parse(args[2],path)==1&&ax_csys_token(args[3])&&ax_csys_token(args[4])?1:-1;
    case AX_MOVE:
        if(count!=9||ax_path_parse(args[2],path)!=1)return -1;
        for(int i=3;i<9;i++)if(!qa_number(args[i],-1e6,1e6))return -1;
        return 1;
    case AX_STATE:
        return count==4&&ax_path_parse(args[2],path)==1&&
            (!wcscmp(args[3],L"SUPPRESS")||!wcscmp(args[3],L"RESUME"))?1:-1;
    case AX_CLEARANCE:
        return count==5&&ax_path_parse(args[2],path)&&ax_path_parse(args[3],path)&&
            wcscmp(args[2],args[3])&&qa_number(args[4],0,1e6)?1:-1;
    case AX_INTERFERENCE:
        if(count==3&&!wcscmp(args[2],L"ALL"))return 1;
        return count==4&&ax_path_parse(args[2],path)&&ax_path_parse(args[3],path)&&wcscmp(args[2],args[3])?1:-1;
    case AX_BOM:case AX_STEP:return count==3&&dv_safe_leaf(args[2])?1:-1;
    case AX_REFS:return count==3&&(!wcscmp(args[2],L"ROOT")||ax_path_parse(args[2],path))?1:-1;
    case AX_PLACEMENT:return count==3&&ax_path_parse(args[2],path)?1:-1;
    case AX_FIX:return count==3&&ax_path_parse(args[2],path)==1?1:-1;
    case AX_EDIT_OFFSET:
        if(count==6&&ax_path_parse(args[2],path)==1){
            int index;
            if(part_integer(args[3],&index)&&index>=1&&index<=12&&ax_offset_number(args[4])&&ax_offset_number(args[5]))return 1;
        }
        return -1;
    case AX_MATE:case AX_PARALLEL:case AX_OFFSET:case AX_COAX:case AX_THREE:case AX_FASTENER:{
        int pairs=kind==AX_THREE?3:kind==AX_FASTENER?2:1;
        if(count!=4+2*pairs+(kind==AX_OFFSET)||ax_path_parse(args[2],path)!=1||
           (wcscmp(args[3],L"ROOT")&&!ax_path_parse(args[3],path)))return -1;
        for(int i=0;i<pairs*2;i++)if(!ax_ref_token(args[4+i]))return -1;
        return kind==AX_OFFSET&&!ax_offset_number(args[6])?-1:1;
    }
    case AX_INSERT_ASM:case AX_NEW_ASM:
        if(count!=6||!dv_safe_leaf(args[2])||!_wcsicmp(args[1],args[2]))return -1;
        for(int i=3;i<6;i++)if(!qa_number(args[i],-1e6,1e6))return -1;
        return 1;
    case AX_COPY_COMPONENT:
        return count==5&&ax_path_parse(args[2],path)==1&&
            (!wcscmp(args[3],L"ROOT")||ax_path_parse(args[3],path))&&
            (!wcscmp(args[4],L"ROOT")||ax_path_parse(args[4],path))&&wcscmp(args[3],args[4])?1:-1;
    case AX_REPLACE_COMPONENT:
        return count==5&&ax_path_parse(args[2],path)==1&&dv_safe_leaf(args[3])&&dv_safe_leaf(args[4])&&
            _wcsicmp(args[3],args[4])?1:-1;
    case AX_DELETE_COMPONENT:
        return count==5&&ax_path_parse(args[2],path)==1&&dv_safe_leaf(args[3])&&
            (!wcscmp(args[4],L"CHECK")||!wcscmp(args[4],L"DELETE"))?1:-1;
    case AX_ROTATE_COMPONENT:
        if(count!=8||ax_path_parse(args[2],path)!=1||wcslen(args[3])!=1||!wcschr(L"XYZ",args[3][0])||
           !qa_number(args[4],-360,360))return -1;
        for(int i=5;i<8;i++)if(!qa_number(args[i],-1e6,1e6))return -1;
        return 1;
    case AX_FLIP_COMPONENT:
        return count==4&&ax_path_parse(args[2],path)==1&&wcslen(args[3])==1&&wcschr(L"XYZ",args[3][0])?1:-1;
    case AX_UNFIX_COMPONENT:return count==3&&ax_path_parse(args[2],path)==1?1:-1;
    case AX_EDIT_REF:case AX_REPAIR_REF:{
        int index;
        if(count!=7||ax_path_parse(args[2],path)!=1||!part_integer(args[3],&index)||index<1||index>12)return -1;
        if(wcscmp(args[4],L"BASE")&&wcscmp(args[4],L"PART"))return -1;
        if(!wcscmp(args[4],L"PART")?wcscmp(args[5],L"SELF")!=0:
           wcscmp(args[5],L"ROOT")&&!ax_path_parse(args[5],path))return -1;
        return ax_ref_token(args[6])||(kind==AX_REPAIR_REF&&!wcscmp(args[6],L"AUTO"))?1:-1;
    }
    case AX_ARRAY_LINEAR:
        return count==6&&ax_path_parse(args[2],path)==1&&ax_axis_index(args[3])>=0&&
            ax_int_range(args[4],2,AX_BATCH_LIMIT)&&ax_offset_number(args[5])?1:-1;
    case AX_ARRAY_GRID:{
        int rows,cols;
        return count==8&&ax_path_parse(args[2],path)==1&&
            (!wcscmp(args[3],L"XY")||!wcscmp(args[3],L"XZ")||!wcscmp(args[3],L"YZ"))&&
            ax_int_range(args[4],1,AX_BATCH_LIMIT)&&ax_int_range(args[5],1,AX_BATCH_LIMIT)&&
            part_integer(args[4],&rows)&&part_integer(args[5],&cols)&&rows*cols>=2&&rows*cols<=AX_BATCH_LIMIT&&
            ax_offset_number(args[6])&&ax_offset_number(args[7])?1:-1;
    }
    case AX_ARRAY_CIRCULAR:
        return count==9&&ax_path_parse(args[2],path)==1&&ax_axis_index(args[3])>=0&&
            qa_number(args[4],-1e6,1e6)&&qa_number(args[5],-1e6,1e6)&&qa_number(args[6],-1e6,1e6)&&
            ax_int_range(args[7],2,AX_BATCH_LIMIT)&&qa_number(args[8],-359.999,359.999)&&ax_offset_number(args[8])?1:-1;
    case AX_MIRROR_COMPONENT:
        return count==6&&ax_path_parse(args[2],path)==1&&ax_ref_token(args[3])&&
            (!wcscmp(args[4],L"SAME")||dv_safe_leaf(args[4]))&&ax_axis_index(args[5])>=0?1:-1;
    case AX_HOLE_ARRAY:case AX_PRESS_HARDWARE:case AX_FASTENER_AUDIT:{
        int first=kind==AX_FASTENER_AUDIT?6:kind==AX_PRESS_HARDWARE?8:7;
        if(count<=first||count-first>12||!ax_path_parse(args[2],path)||!ax_ref_token(args[3])||
            !dv_safe_leaf(args[4])||!ax_ref_token(args[5]))return -1;
        if(kind!=AX_FASTENER_AUDIT&&!ax_ref_token(args[6]))return -1;
        if(kind==AX_PRESS_HARDWARE&&!qa_number(args[7],0,2))return -1;
        for(int i=first;i<count;i++)if(wcsncmp(args[i],L"SURFACE:",8)||!ax_ref_token(args[i]))return -1;
        return 1;
    }
    case AX_FASTENER_STACK:
        if(count!=16||!ax_path_parse(args[2],path))return -1;
        for(int i=3;i<16;i++){
            int is_name=i==6||i==9||i==13;
            if(is_name?!dv_safe_leaf(args[i]):!ax_ref_token(args[i]))return -1;
        }
        return 1;
    case AX_FASTENER_FIT:
        return count==13&&ax_path_parse(args[2],path)&&ax_path_parse(args[6],path)&&
            ax_ref_token(args[3])&&ax_ref_token(args[4])&&ax_ref_token(args[5])&&ax_ref_token(args[7])&&ax_ref_token(args[8])&&
            qa_number(args[9],0,10000)&&qa_number(args[10],0.001,10000)&&qa_number(args[11],0.001,10000)&&
            qa_number(args[12],0.001,10000)?1:-1;
    case AX_REPLACE_BATCH:
        if(count<9||count-8>12||!dv_safe_leaf(args[2])||!dv_safe_leaf(args[3])||!_wcsicmp(args[2],args[3]))return -1;
        for(int i=4;i<8;i++)if(!ax_ref_token(args[i]))return -1;
        for(int i=8;i<count;i++)if(ax_path_parse(args[i],path)!=1)return -1;
        return 1;
    case AX_SCREEN_MOUNT:case AX_GLASS_MOUNT:case AX_FILTER_MOUNT:case AX_LATCH_MOUNT:{
        int required=kind==AX_FILTER_MOUNT?16:kind==AX_LATCH_MOUNT?14:13;
        if(count!=required||!ax_path_parse(args[2],path)||!ax_csys_token(args[3])||
           !dv_safe_leaf(args[8])||!ax_csys_token(args[9])||!ax_ref_token(args[10])||
           !qa_number(args[11],-1000,1000)||!qa_number(args[12],0,1000))return -1;
        for(int i=4;i<8;i++)if(!ax_ref_token(args[i]))return -1;
        if(kind==AX_FILTER_MOUNT)
            return ax_signed_axis(args[13])&&qa_number(args[14],0.001,10000)&&
                (!wcscmp(args[15],L"NONE")||ax_path_parse(args[15],path))?1:-1;
        if(kind==AX_GLASS_MOUNT&&!qa_number(args[11],0,1000))return -1;
        if(kind==AX_LATCH_MOUNT&&!qa_number(args[13],0.001,1000))return -1;
        return 1;
    }
    case AX_POWER_MOUNT:case AX_FAN_MOUNT:
        if(count!=(kind==AX_FAN_MOUNT?14:12)||!ax_path_parse(args[2],path)||!dv_safe_leaf(args[6])||
           !ax_signed_axis(args[11]))return -1;
        for(int i=3;i<=10;i++)if(i!=6&&!ax_ref_token(args[i]))return -1;
        return kind==AX_POWER_MOUNT||(ax_ref_token(args[12])&&ax_ref_token(args[13]))?1:-1;
    case AX_PCB_MOUNT:
        if(count!=15||!ax_path_parse(args[2],path)||!ax_path_parse(args[5],path)||!dv_safe_leaf(args[8])||
           !ax_path_parse(args[12],path)||!qa_number(args[14],0,1000))return -1;
        for(int i=3;i<=13;i++)if(i!=5&&i!=8&&i!=12&&!ax_ref_token(args[i]))return -1;
        return 1;
    case AX_RAIL_MOUNT:
        return count==9&&ax_path_parse(args[2],path)&&ax_csys_token(args[3])&&dv_safe_leaf(args[4])&&
            ax_csys_token(args[5])&&ax_int_range(args[6],1,12)&&qa_number(args[7],-10000,10000)&&
            qa_number(args[8],0.001,10000)?1:-1;
    case AX_HINGE_MOUNT:
        if(count<14||count>16||!ax_path_parse(args[2],path)||!ax_ref_token(args[3])||!ax_ref_token(args[4])||
           !ax_path_parse(args[5],path)||!ax_ref_token(args[6])||!ax_ref_token(args[7])||
           !dv_safe_leaf(args[8])||!ax_csys_token(args[9])||!ax_ref_token(args[10])||!ax_ref_token(args[11]))return -1;
        if(!ax_ref_token(args[12]))return -1;
        for(int i=13;i<count;i++)if(!ax_csys_token(args[i]))return -1;
        return 1;
    case AX_SEAL_CHECK:
        if(count<10||count>16||!ax_path_parse(args[2],path)||!ax_ref_token(args[3])||
           !ax_path_parse(args[4],path)||!ax_ref_token(args[5])||!qa_number(args[6],0.001,1000)||
           !qa_number(args[7],0,100)||!qa_number(args[8],0,100))return -1;
        for(int i=9;i<count;i++)if(wcsncmp(args[i],L"EDGE:",5)||!ax_int_range(args[i]+5,1,2147483647))return -1;
        return 1;
    case AX_JOINT_PIN:case AX_JOINT_SLIDER:case AX_JOINT_CYLINDER:
        if(count!=10||(ax_path_parse(args[2],path)!=1&&wcscmp(args[2],L"ROOT"))||!dv_safe_leaf(args[3]))return -1;
        for(int i=4;i<10;i++)if(!ax_ref_token(args[i]))return -1;
        return 1;
    case AX_ROTATION_LIMIT:case AX_TRAVEL_LIMIT:{
        double limit=kind==AX_ROTATION_LIMIT?179.999:10000,low,high;
        return count==5&&ax_path_parse(args[2],path)==1&&qa_number(args[3],-limit,limit)&&
            qa_number(args[4],-limit,limit)&&part_number(args[3],&low)&&part_number(args[4],&high)&&high>low?1:-1;
    }
    case AX_MOTION_PRESET:
        return count==4&&ax_path_parse(args[2],path)==1&&
            (!wcscmp(args[3],L"CLOSED")||!wcscmp(args[3],L"SERVICE")||!wcscmp(args[3],L"OPEN"))?1:-1;
    case AX_MOTION_SWEEP:
        return count==10&&ax_path_parse(args[2],path)&&ax_path_parse(args[3],path)&&
            ax_ref_token(args[4])&&(!wcscmp(args[5],L"ROTATE")||!wcscmp(args[5],L"SLIDE"))&&
            qa_number(args[6],-10000,10000)&&qa_number(args[7],-10000,10000)&&
            ax_int_range(args[8],2,25)&&qa_number(args[9],0,1000)?1:-1;
    case AX_DOOR_SWEEP:case AX_TRAY_SWEEP:case AX_TOOL_SWEEP:
        return count==8&&ax_path_parse(args[2],path)&&ax_path_parse(args[3],path)&&ax_ref_token(args[4])&&
            qa_number(args[5],-10000,10000)&&ax_int_range(args[6],2,25)&&qa_number(args[7],0,1000)?1:-1;
    case AX_LAYOUT_DATUM:
        return count==8&&fd_name(args[2])&&wcslen(args[2])<=20&&ax_csys_token(args[3])&&ax_ref_token(args[4])&&
            ax_ref_token(args[5])&&ax_ref_token(args[6])&&qa_number(args[7],-10000,10000)?1:-1;
    case AX_LAYOUT_ENVELOPE:
        if(count!=10||!dv_safe_leaf(args[2])||(wcscmp(args[3],L"SPACE")&&wcscmp(args[3],L"KEEPOUT")))return -1;
        for(int i=4;i<10;i++)if(!qa_number(args[i],i<7?-10000:0.1,10000))return -1;
        return 1;
    case AX_LAYOUT_SKELETON:
        return count==6&&dv_safe_leaf(args[2])&&qa_number(args[3],20,10000)&&qa_number(args[4],20,10000)&&
            qa_number(args[5],20,10000)?1:-1;
    case AX_LAYOUT_LINK:
        return count==6&&ax_path_parse(args[2],path)==1&&ax_path_parse(args[3],path)==1&&
            ax_csys_token(args[4])&&ax_csys_token(args[5])?1:-1;
    case AX_LAYOUT_INTERFACE:
        return count==11&&ax_path_parse(args[2],path)==1&&fd_name(args[3])&&wcslen(args[3])<=20&&
            ax_csys_token(args[4])&&ax_ref_token(args[5])&&ax_ref_token(args[6])&&ax_ref_token(args[7])&&
            qa_number(args[8],0.1,10000)&&qa_number(args[9],0.1,10000)&&qa_number(args[10],-10000,10000)?1:-1;
    case AX_LAYOUT_COPY:
        if(count<6||count>12||(wcscmp(args[2],L"ROOT")&&!ax_path_parse(args[2],path))||
           ax_path_parse(args[3],path)!=1||!fd_name(args[4])||wcslen(args[4])>20)return -1;
        for(int i=5;i<count;i++)if(!ax_ref_token(args[i])&&!ax_csys_token(args[i])&&
                        ((wcsncmp(args[i],L"CURVE:",6)||!fd_name(args[i]+6))&&
             (wcsncmp(args[i],L"POINT:",6)||!fd_name(args[i]+6))))return -1;
        return 1;
    case AX_LAYOUT_DIM:
        return count==6&&(!wcscmp(args[2],L"ROOT")||ax_path_parse(args[2],path)==1)&&fd_name(args[3])&&
            qa_number(args[4],0.001,10000)&&qa_number(args[5],0.001,10000)?1:-1;
    case AX_LAYOUT_REFS:case AX_LAYOUT_AUDIT:
        return count==3&&(!wcscmp(args[2],L"ALL")||!wcscmp(args[2],L"ROOT")||ax_path_parse(args[2],path))?1:-1;
    case AX_LAYOUT_COMPARE:case AX_V_COMPARE:
        return count==5&&dv_safe_leaf(args[2])&&(!wcscmp(args[3],L"NONE")||ax_csys_token(args[3]))&&
            qa_number(args[4],0.001,1000)?1:-1;
    case AX_FIELD_CUT:case AX_FIELD_HOLE:case AX_FIELD_GRID:case AX_FIELD_TRANSFER:
    case AX_FIELD_LAYERS:case AX_FIELD_WINDOW:case AX_FIELD_ROUTE:
        return ax_field_operation_valid(kind,count,args);
    case AX_FIELD_SHIM:
        return count==12&&dv_safe_leaf(args[2])&&ax_path_parse(args[3],path)&&
            !wcsncmp(args[4],L"SURFACE:",8)&&ax_ref_token(args[4])&&ax_path_parse(args[5],path)&&
            !wcsncmp(args[6],L"SURFACE:",8)&&ax_ref_token(args[6])&&ax_csys_token(args[7])&&
            qa_number(args[8],-10000,10000)&&qa_number(args[9],-10000,10000)&&
            qa_number(args[10],0.1,10000)&&qa_number(args[11],0.1,10000)?1:-1;
    case AX_FIELD_MARK:
        if(count!=11||ax_path_parse(args[2],path)!=1||!fd_name(args[3])||!ax_csys_token(args[4]))return -1;
        for(int i=5;i<11;i++)if(!qa_number(args[i],-10000,10000))return -1;
        return 1;
    case AX_FIELD_AUDIT:
        return count==3&&fd_name(args[2])?1:-1;

    default:return -1;
    }
}
#endif
