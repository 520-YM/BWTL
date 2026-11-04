static ProError part_extrude_current(const PartCommand *cmd,const PartSnapshot *snapshot,ProMdl model,wchar_t *text,size_t capacity){
    ModelPlan plan={0};ProMdlName name;ProFeature created={0};const char *stage="validate current extrusion";
    ProError error;int restored=1,window;TemplateDatums before={0},after={0};
    if(snapshot->type!=PRO_MDL_PART || snapshot->subtype==PROMDLSTYPE_PART_SHEETMETAL){wcscpy_s(text,capacity,L"未执行：当前目标是装配或钣金模型。请在装配内激活要编辑的实体零件；不能把装配本身当实体拉伸。");return PRO_TK_BAD_CONTEXT;}
    plan.cylinder=!wcscmp(cmd->arg[2],L"CYLINDER");plan.ready=1;
    part_number(cmd->arg[3],&plan.a);part_number(cmd->arg[4],&plan.b);part_number(cmd->arg[5],&plan.height);part_number(cmd->arg[6],&plan.hole);
    if(!model_plan_valid(&plan))return PRO_TK_BAD_INPUTS;
    if((error=template_empty((ProSolid)model))){wcscpy_s(text,capacity,L"未执行：当前零件已有实体，现阶段的当前零件拉伸只支持空实体零件的第一个基础拉伸；没有另建零件代替。");return error;}
    if((error=ProSolidFeatVisit((ProSolid)model,template_feature,NULL,&before)))return error;
    error=model_build_internal(&plan,name,&stage,model,&restored,&created);
    if(!error && snapshot->in_assembly){
        error=part_regenerate(snapshot->window_model);
        if(error){
            restored=!model_remove_feature((ProSolid)model,created.id);
            if(part_regenerate(model))restored=0;
            if(part_regenerate(snapshot->window_model))restored=0;
            stage="Regenerate owning assembly";
        }
    }
    if(error){
        if(restored && (ProSolidFeatVisit((ProSolid)model,template_feature,NULL,&after)||after.total!=before.total||after.planes!=before.planes||after.csys!=before.csys||template_empty((ProSolid)model)))restored=0;
        swprintf_s(text,capacity,L"当前模型拉伸失败：%hs，错误码%d；%ls；未保存。",stage,error,restored?L"目标空零件状态已检查保留/恢复":L"回滚未能验证，请检查模型，不应继续操作");return error;
    }
    if(!ProWindowCurrentGet(&window))ProWindowRepaint(window);
    swprintf_s(text,capacity,L"已在当前零件 %ls 内创建%ls基础拉伸，特征ID=%d；体积及重生成校验通过。未新建零件，未自动保存。%ls 基础拉伸位于零件局部XY平面、沿+Z；保留原基准但不随基准面移动。",name,plan.cylinder?L"圆形":L"矩形",created.id,snapshot->in_assembly?L"已重生成所在装配；此修改影响同一零件的全部实例。":L"");
    return 0;
}
