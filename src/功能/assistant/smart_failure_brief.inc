static const wchar_t *smart_failed_action(const PartCommand *cmd){
    static const struct {const wchar_t *verb,*name;} names[]={
        {L"SMT_SKETCH",L"画钣金草绘"},{L"SKETCH_CREATE",L"画草绘"},{L"SKETCH_ADD",L"补画草绘"},
        {L"SMT_FIRST_WALL",L"创建第一块钣金壁"},{L"SMT_CUT",L"在钣金上切口"},
        {L"SMT_FLANGE",L"创建钣金凸缘"},{L"SMT_BEND",L"创建折弯"},{L"CASE_TRAY",L"制作钣金托盘"},
        {L"EDGE_ROUND",L"倒圆角"},{L"EDGE_CHAMFER",L"倒角"},{L"HOLE_CREATE",L"开孔"},
        {L"ASM_W_ALIGN",L"定位装配组件"},{L"ASM_W_ALIGN_BATCH",L"批量定位装配组件"}
    };
    if(cmd){
        if(cmd->verb==PART_IMITATE||cmd->verb==PART_IMITATE_LOCAL||cmd->verb==PART_IMITATE_SIZE)
            return L"按原特征重画";
        if(cmd->verb==PART_EXTRUDE)return L"拉伸";
        if(cmd->verb==PART_SAVE)return L"保存模型";
        if(cmd->verb==PART_STEP)return L"导出 STEP";
        for(size_t i=0;i<_countof(names);i++){
            if(!wcscmp(cmd->arg[0],names[i].verb))return names[i].name;
        }
    }
    return L"当前操作";
}

/* Use evidence from this native receipt. An error number alone cannot identify the cause. */
static void smart_describe_failure(const PartCommand *cmd,const wchar_t *receipt,wchar_t *out,size_t capacity){
    const wchar_t *cause=L"Creo 回执没有说明具体原因";
    const wchar_t *effect=L"";
    if(wcsstr(receipt,L"与请求时不一致"))
        cause=L"当前模型、模型版本或工作目录与提交时对不上，插件在动模型前停下了";
    else if(wcsstr(receipt,L"regenerate and verify native feature")||
            wcsstr(receipt,L"verify sketch feature and parent datums"))
        cause=L"在重新生成或检查特征时没通过，Creo 没说明具体是哪项检查出了问题";
    else if(wcsstr(receipt,L"regenerate sheetmetal feature")||wcsstr(receipt,L"verify healthy sheetmetal feature")||
            wcsstr(receipt,L"verify native sheetmetal type, parents, thickness and dimensions"))
        cause=L"在重新生成或检查钣金特征时没通过，Creo 没说明具体是哪项检查出了问题";
    else if(wcsstr(receipt,L"solve referenced sketch")||wcsstr(receipt,L"fd_section_solve("))
        cause=L"Creo 没能解出草绘的形状和约束";
    else if(wcsstr(receipt,L"create sketch geometry and dimensions")||wcsstr(receipt,L"test rejected sketch dimension input")||
            wcsstr(receipt,L"fd_shape_append(")||wcsstr(receipt,L"as_add_line("))
        cause=L"Creo 在画轮廓或加尺寸时出了错，回执没说具体是哪处";
    else if(wcsstr(receipt,L"set sketch plane and orientation")||wcsstr(receipt,L"fd_sketch_setup("))
        cause=L"Creo 没能用指定的面和方向建立草绘参照";
    else if(wcsstr(receipt,L"先恢复源件参照")||wcsstr(receipt,L"先恢复源件的参照"))
        cause=L"原件还有缺失或失效的参照，这一步无法继续重建";

    if(wcsstr(receipt,L"未新增或已核对移除本次特征")||wcsstr(receipt,L"未创建或已核对移除本次新增特征"))
        effect=L"，这一步没有留下新特征";
    else if(wcsstr(receipt,L"已恢复原草绘并核对原尺寸"))
        effect=L"，原草绘和尺寸已经恢复";
    else if(smart_unsafe_receipt(receipt))
        effect=L"，这次改动是否撤回还没有确认";
    swprintf_s(out,capacity,L"简单说：%ls没成功，%ls%ls。",smart_failed_action(cmd),cause,effect);
}
