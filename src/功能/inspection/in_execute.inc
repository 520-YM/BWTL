static ProError in_execute(const PartCommand *cmd,ProMdl model,int sheet,wchar_t *report,size_t capacity){
    static const wchar_t *const labels[]={L"",L"孔口螺钉头落位",L"沉孔藏头",L"盲孔剩余底厚",L"孔与折弯距离",L"折弯工具空间",
        L"散热开孔率",L"设备安装净空",L"边缘尖角",L"深孔长径比",L"刀具直径"};
    Inspection inspect={0};
    PartText out={report,capacity,0};
    int kind=in_kind(cmd->arg[0]);
    if(inspection_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    inspect.model=model;
    ProError error=qa_single_body(model,&inspect.body);
    if(!error)error=qa_mm_per_unit(model,&inspect.mm);
    if(!error&&!sheet&&(kind==IN_BEND||kind==IN_TOOLSPACE))error=PRO_TK_BAD_CONTEXT;
    if(!error)switch(kind){
    case IN_SEAT:error=in_seat(&inspect,cmd,&out);break;
    case IN_HIDE:error=in_hide(&inspect,cmd,&out);break;
    case IN_BOTTOM:error=in_bottom(&inspect,cmd,&out);break;
    case IN_BEND:error=in_bend(&inspect,cmd,&out);break;
    case IN_TOOLSPACE:error=in_toolspace(&inspect,cmd,&out);break;
    case IN_VENT:error=in_vent(&inspect,cmd,&out);break;
    case IN_DEVICE:error=in_device(&inspect,cmd,&out);break;
    case IN_SHARP:error=in_sharp(&inspect,cmd,&out);break;
    case IN_RATIO:error=in_ratio(&inspect,cmd,&out);break;
    case IN_CUTTER:error=in_cutter(&inspect,cmd,&out);break;
    }
    trace("inspection native result",error);
    if(error)part_text_add(&out,L"%ls检查未完成（Creo %d）。请核对实体类型、实际参照和适用几何。",labels[kind],error);
    else part_text_add(&out,L"已只读检查当前零件。");
    return error;
}
