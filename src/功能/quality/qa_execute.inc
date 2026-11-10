static ProError qa_execute(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
    PartText out={report,capacity,0};
    ProError error=PRO_TK_NO_ERROR;
    int kind=qa_kind(cmd->arg[0]);
    double scale=1.0;
    if(quality_plan_valid(cmd->count,(wchar_t (*)[PART_TOKEN_MAX])cmd->arg)!=1)return PRO_TK_BAD_INPUTS;
    if(kind!=QA_DEPS&&(error=qa_mm_per_unit(model,&scale)))goto done;
    if((kind==QA_WALL||kind==QA_HOLES)&&(error=qa_single_body(model,NULL)))goto done;
    switch(kind){
    case QA_MEASURE:error=qa_measure(model,cmd,scale,&out);break;
    case QA_WALL:error=qa_wall(model,cmd,scale,&out);break;
    case QA_HOLES:error=qa_holes(model,cmd,scale,&out);break;
    case QA_DRAFT:error=qa_draft(model,cmd,&out);break;
    case QA_CONTINUITY:error=qa_continuity(model,cmd,scale,&out);break;
    case QA_MASS:error=qa_mass(model,scale,&out);break;
    case QA_SYMMETRY:error=qa_symmetry(model,cmd,scale,&out);break;
    case QA_COMPARE:error=qa_compare(model,cmd,scale,&out);break;
    case QA_DEPS:error=qa_deps(model,cmd,&out);break;
    case QA_RULES:error=qa_rules(model,cmd,scale,&out);break;
    }
done:
    if(error)part_text_add(&out,L"%ls 检查未完成（Creo %d）。请核对活动零件、参照类型或比较对象。\n",cmd->arg[0],error);
    return error;
}
