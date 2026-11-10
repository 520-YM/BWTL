static ProError org_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    static const wchar_t *const labels[]={L"",L"孔位坐标表",L"孔规格归类",L"导入件槽口识别",L"安装平面提取",
        L"批量重命名",L"未引用基准",L"失败与抑制特征",L"重复或重叠孔位",L"版本参数对照",L"改模影响预检"};
    int kind=org_kind(cmd->arg[0]);
    PartText out={report,capacity,0};
    if(organize_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    OrganizeView view={model,1};
    ProError error=PRO_TK_NO_ERROR;
    if(kind==ORG_HOLES||kind==ORG_SPECS||kind==ORG_SLOTS||kind==ORG_PLANES||kind==ORG_OVERLAP)
        error=qa_mm_per_unit(model,&view.mm);
    if(!error)switch(kind){
    case ORG_HOLES:error=org_holes(&view,cmd,&out);break;
    case ORG_SPECS:error=org_specs(&view,cmd,&out);break;
    case ORG_SLOTS:error=org_slots(&view,cmd,&out);break;
    case ORG_PLANES:error=org_planes(&view,cmd,&out);break;
    case ORG_RENAME:error=org_rename(&view,cmd,&out);break;
    case ORG_UNUSED:error=org_unused(&view,&out);break;
    case ORG_STATUS:error=org_status(&view,&out);break;
    case ORG_OVERLAP:error=org_overlap(&view,cmd,&out);break;
    case ORG_PARAMS:error=org_params(&view,cmd,&out);break;
    case ORG_IMPACT:error=org_impact(&view,cmd,&out);break;
    }
    trace("organize operation result",error);
    if(error&&!report[0])part_text_add(&out,L"%ls未完成（Creo %d）。请核对零件、参照和当前会话中的模型。",labels[kind],error);
    if(!error)part_text_add(&out,kind==ORG_RENAME?L"未保存。":L"已只读核对。");
    return error;
}
