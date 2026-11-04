static ProError fd_sketch_create(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};FdSketch s={0};ProError e;int ids[4]={0},count=0;
    const wchar_t *plane=cmd->arg[3],*orient=cmd->arg[4];int shape_start=5;
    if(!wcscmp(cmd->arg[0],L"SKETCH_ADD")){
        ProWVerstamp stamp=NULL;int equal;
        if(fd_draft.model!=model||wcscmp(fd_draft.name,cmd->arg[2])||!fd_draft.stamp)return PRO_TK_BAD_CONTEXT;
        if((e=ProMdlVerstampGet(model,&stamp)))return e;equal=ProVerstampEqual(stamp,fd_draft.stamp);ProVerstampFree(&stamp);
        if(!equal)return PRO_TK_BAD_CONTEXT;plane=fd_draft.plane;orient=fd_draft.orient;shape_start=3;
    }
#define FS_TRY(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    FS_TRY("prepare referenced sketch",fd_begin(&tx,model,cmd->arg[2],PRO_FEAT_CURVE));
    FS_TRY("set sketch plane and orientation",fd_sketch_setup(&tx,plane,orient));
    FS_TRY("create incomplete referenced sketch",fd_create(&tx,1));s.feature=tx.feature;
    FS_TRY("extract referenced sketch tree",ProFeatureElemtreeExtract(&s.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&s.tree));
    FS_TRY("get owned sketch section",fd_section_from_tree(s.tree,&s.section));
    FS_TRY("create sketch geometry and dimensions",fd_shape_append(s.section,cmd,shape_start,tx.scale,ids,&count));
    FS_TRY("solve referenced sketch",fd_section_solve(s.section,tx.scale));
    tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    FS_TRY("complete referenced sketch",ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,tx.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors));
    FS_TRY("verify sketch feature and parent datums",fd_verify(&tx));
    fd_last_sketch_model=model;wcscpy_s(fd_last_sketch,PRO_NAME_SIZE,cmd->arg[2]);
    if(fd_draft.model==model&&!wcscmp(fd_draft.name,cmd->arg[2])){if(fd_draft.stamp)ProVerstampFree(&fd_draft.stamp);memset(&fd_draft,0,sizeof(fd_draft));}
    swprintf_s(report,capacity,L"已在当前零件创建关联草绘 %ls（%ls），图元编号 %d%ls。草绘求解、尺寸与参照父子关系检查通过，未保存。",cmd->arg[2],cmd->arg[shape_start],ids[0],count==4?L" 起连续4条矩形边":L"");
done:
    fd_sketch_free(&s);fd_cleanup(&tx,e);
    if(e)swprintf_s(report,capacity,L"关联草绘未完成：%hs，错误码%d；%ls，未保存。",tx.stage,e,tx.rollback_ok?L"未创建或已核对移除本次新增特征":L"回滚未能验证，请检查模型");
    return e;
#undef FS_TRY
}
