static ProError in_ratio(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    RevisionHole hole;
    double remaining;
    ProError error=in_blind(inspect,cmd->arg[2],&hole,&remaining);
    if(error)return error;
    double ratio=hole.depth/hole.diameter,limit=as_arg(cmd,3);
    part_text_add(out,L"平底盲孔深径比 %.8g，%ls限值 %.8g。圆柱直径 %.8g mm，平底深度 %.8g mm。",
        ratio,qa_at_least(limit,ratio)?L"符合":L"超过",limit,hole.diameter*inspect->mm,hole.depth*inspect->mm);
    return PRO_TK_NO_ERROR;
}
