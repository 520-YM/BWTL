static void execute_model_plan(void) {
    ProMdlName name = L"";
    const char *stage = "Not started";
    wchar_t message[768];
    ProError error;
    ModelPlan plan;
    trace("Execute modeling requested; pending", pending_plan.ready);
    if (ai_process) { append(L"请等待 AI 给出完整方案。"); return; }
    if (!pending_plan.ready) { append(L"当前没有待执行的建模方案。请先说明零件形状与完整尺寸，等待 AI 给出方案。"); return; }
    plan = pending_plan; pending_plan.ready = 0;
    if (!save_pending_plan()) { pending_plan = plan; return; }
    append(L"正在通过原生接口新建独立零件…");
    error = plan.empty_kind ? model_create_empty(plan.empty_kind,name,&stage) : model_create(&plan, name, &stage);
    if(error==PRO_TK_NO_ERROR && plan.empty_kind)
        swprintf_s(message,768,L"已按当前默认%ls模板创建空%ls：%ls.%ls。模板特征数量、坐标系和基准面保留，未添加拉伸、板壁或孔，未自动保存。",plan.empty_kind==2?L"钣金":plan.empty_kind==3?L"装配":L"实体",plan.empty_kind==3?L"装配":L"零件",name,plan.empty_kind==3?L"asm":L"prt");
    else if (error == PRO_TK_NO_ERROR)
        swprintf_s(message, 768, L"原生建模成功：%ls.prt。已复制当前默认模板并创建基础拉伸，保留坐标系、基准面和参数；已重新生成并核对实体体积，未自动保存到磁盘。已有模型未修改。", name);
    else
        swprintf_s(message, 768, L"建模未完成。阶段：%hs，错误码：%d，新模型名：%ls。可能留下独立的空白或不完整零件供诊断，未修改已有模型、未自动保存。", stage, error, name[0] ? name : L"尚未创建");
    append_card(L"执行结果",message); save_turn(L"原生执行结果", message);
    if(error==PRO_TK_NO_ERROR){
        wcscpy_s(expected_active_model,PRO_NAME_SIZE,name);
        trace("Queue active model verification",ProMacroLoad(L"~ Command `CodexNativeVerifyActive`;"));
    }
}
