static const wchar_t *aw_service_name(int kind){
    switch(kind){
    case AW_TOOL_ACCESS:return L"紧固工具空间";
    case AW_DOOR_FINAL:return L"关门末段";
    case AW_TERMINAL_ACCESS:return L"端子接线空间";
    case AW_PLUG_PATH:return L"连接器插拔";
    case AW_RAIL_SERVICE:return L"导轨模块拆卸";
    case AW_SMALL_SERVICE:return L"小耗材取出";
    case AW_FILTER_SERVICE:return L"滤网更换";
    case AW_INSTALL_PATH:return L"部件装入轨迹";
    case AW_WELD_TOOL:return L"焊接及打磨工具";
    case AW_OUTDOOR_ACCESS:return L"户外附件安装和检修";
    case AW_CLOSE_BLOCKS:return L"先装后封";
    case AW_TEMP_SUPPORT:return L"工装和临时支撑";
    case AW_REMOVE_MIN:return L"更换路径与拆除";
    case AW_CASTER_BRAKE:return L"脚轮回转与刹车";
    default:return L"运动";
    }
}

static ProError aw_service_motion(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    ProMatrix previous;
    const wchar_t *last_item=NULL;
    double minimum=as_arg(cmd,2);
    int failed=0,unknown=0,total=0,open_pass=0,closed_fail=0;
    wchar_t blockers[2048]=L"";
    const int offset=kind==AW_TERMINAL_ACCESS?6:3;
    if(kind==AW_TERMINAL_ACCESS){
        const double exposed=as_arg(cmd,3),available=as_arg(cmd,4),allowed=as_arg(cmd,5);
        const int pass=exposed>=allowed&&available>=exposed;
        failed+=!pass;
        part_text_add(out,L"规定最低剥线 %.8g mm，计划剥线 %.8g mm，输入可用直进深度 %.8g mm。%ls。\n",
            allowed,exposed,available,pass?L"长度符合":L"剥线或直进深度不符");
    }
    for(int i=offset;i<cmd->count;i+=8){
        PartCommand move={0};
        move.verb=PART_FOUNDATION;
        move.count=10;
        wcscpy_s(move.arg[0],PART_TOKEN_MAX,L"ASM_MOTION_SWEEP");
        wcscpy_s(move.arg[1],PART_TOKEN_MAX,cmd->arg[1]);
        for(int j=0;j<7;j++)wcscpy_s(move.arg[2+j],PART_TOKEN_MAX,cmd->arg[i+1+j]);
        wcscpy_s(move.arg[9],PART_TOKEN_MAX,cmd->arg[2]);
        const int relative=kind==AW_INSTALL_PATH&&last_item&&!wcscmp(last_item,move.arg[2]);
        if(kind==AW_INSTALL_PATH&&last_item&&!relative){
            part_text_add(out,L"装入各步骤须使用同一个移动实例。\n");
            return PRO_TK_BAD_INPUTS;
        }
        wchar_t measures[8192]=L"";
        PartText log={measures,_countof(measures),0};
        AxMotionResult check={0};
        ProMatrix finish;
        total++;
        ProError error=ax_motion_measure(assembly,&move,AX_MOTION_SWEEP,mm,relative?previous:NULL,finish,&check,&log);
        if(error){
            unknown++;
            part_text_add(out,L"%ls %ls 对 %ls 的阶段未核实（Creo %d）。\n",cmd->arg[i],move.arg[2],move.arg[3],error);
            if(measures[0])part_text_add(out,L"%ls",measures);
            if(ax_recovery_uncertain||kind==AW_INSTALL_PATH)break;
            continue;
        }
        memcpy(previous,finish,sizeof(previous));
        last_item=cmd->arg[i+1];
        if(check.failed)failed++;
        if(kind==AW_CLOSE_BLOCKS){
            if(!wcscmp(cmd->arg[i],L"OPEN")&&!check.failed)open_pass++;
            if(!wcscmp(cmd->arg[i],L"CLOSED")&&check.failed)closed_fail++;
        }
        if(kind==AW_REMOVE_MIN&&check.failed){
            wcscat_s(blockers,_countof(blockers),L" ");
            wcscat_s(blockers,_countof(blockers),move.arg[3]);
        }
        part_text_add(out,L"%ls %ls 对 %ls，%d 个采样姿态，最小间隙 %.8g mm（要求 ≥ %.8g），%ls。\n",
            cmd->arg[i],move.arg[2],move.arg[3],check.samples,check.minimum,minimum,
            check.collisions?L"发生干涉":check.failed?L"净空不足":L"采样通过");
        if(check.failed)part_text_add(out,L"首次未达到要求位置 %.8g %ls。需要处理指定障碍或调整路径。\n",
            check.first_failure,!wcscmp(move.arg[5],L"ROTATE")?L"度":L"mm");
    }
    const int requested=(cmd->count-offset)/8;
    part_text_add(out,L"%ls核对 %d/%d 个指定阶段，%d 个不符合，%d 个未核实。结果覆盖给定障碍和采样点。原模型保留。\n",
        aw_service_name(kind),total,requested,failed,unknown);
    if(kind==AW_CLOSE_BLOCKS&&open_pass&&closed_fail&&!unknown)
        part_text_add(out,L"同一路径开盖通过而封盖受阻，当前这件需要先安装，然后封板。\n");
    if(kind==AW_REMOVE_MIN&&blockers[0])
        part_text_add(out,L"按同一抽出路径，需先移开这些已核实阻碍件：%ls。%ls\n",blockers,
            unknown?L"仍有未核实障碍。":L"其余所选件可保留在当前姿态。");
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
