static void execute_model_plan(void) {
    ProMdlName name = L"";
    const char *stage = "Not started";
    wchar_t message[768];
    ProError error;
    ModelPlan plan;
    trace("Execute modeling requested; pending", pending_plan.ready);
    if (ai_process) { append(L"请等待 AI 给出完整方案。"); return; }
    if (!pending_plan.ready) { append(L"当前没有待执行的建模方案。请先说明零件形状与完整尺寸，等待 AI 给出方案。"); return; }
    if(!smart_active){smart_begin(L"执行建模");smart_local_plan=1;}
    if(smart_readonly){pending_plan.ready=0;smart_reply(L"本轮只分析，未新建或修改模型。");return;}
    plan = pending_plan; pending_plan.ready = 0;
    if (!save_pending_plan()) { pending_plan = plan; smart_finish(); return; }
    if(plan.centered)error=model_create_centered_cylinder(&plan,name,&stage);
    else error = plan.empty_kind ? model_create_empty(plan.empty_kind,name,&stage) : model_create(&plan, name, &stage);
    if(error==PRO_TK_NO_ERROR && plan.centered)
        swprintf_s(message,768,L"已创建原点居中圆柱 %ls.prt，直径%g mm，高度%g mm。圆心位于模型原点，沿 +Z 拉伸。保留可编辑的关联圆草绘与拉伸特征，已核对轴线、端面和体积，完成的几何未自动保存。",name,plan.a,plan.height);
    else if(error==PRO_TK_NO_ERROR && plan.empty_kind)
        swprintf_s(message,768,L"已按当前默认%ls模板创建空%ls：%ls.%ls。模板特征数量、坐标系和基准面保留，未添加拉伸、板壁或孔，未自动保存。",plan.empty_kind==2?L"钣金":plan.empty_kind==3?L"装配":L"实体",plan.empty_kind==3?L"装配":L"零件",name,plan.empty_kind==3?L"asm":L"prt");
    else if (error == PRO_TK_NO_ERROR)
        swprintf_s(message, 768, L"原生建模成功：%ls.prt。已复制当前默认模板并创建基础拉伸，保留坐标系、基准面和参数；已重新生成并核对实体体积，未自动保存到磁盘。已有模型未修改。", name);
    else
        swprintf_s(message, 768, L"建模未完成。阶段：%hs，错误码：%d，新模型名：%ls。可能留下独立的空白或不完整零件供诊断，未修改已有模型、未自动保存。", stage, error, name[0] ? name : L"尚未创建");
    if(error==PRO_TK_NO_ERROR){
        wcsncpy_s(smart_new_model_result,1024,message,_TRUNCATE);
        wcscpy_s(expected_active_model,PRO_NAME_SIZE,name);
        error=ProMacroLoad(L"~ Command `CodexNativeVerifyActive`;");
        trace("Queue active model verification",error);
        if(error){
            smart_record_tool(message);
            smart_reply(L"模型已创建，但无法调度活动窗口核对。请查看执行明细。");
        }
    }else smart_after_tool(error,message,0);
}
