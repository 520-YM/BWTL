static ProError in_bottom(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    RevisionHole hole;
    double bottom;
    ProError error=in_blind(inspect,cmd->arg[2],&hole,&bottom);
    if(error)return error;
    double limit=as_arg(cmd,3);
    part_text_add(out,L"盲孔剩余底厚%ls。九处轴向测点最薄 %.8g mm，要求至少 %.8g mm，实测平底深度 %.8g mm。",
        qa_at_least(bottom,limit)?L"符合":L"低于要求",bottom,limit,hole.depth*inspect->mm);
    return PRO_TK_NO_ERROR;
}
