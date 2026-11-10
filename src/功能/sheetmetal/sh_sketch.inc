static ProError sh_sketch(FdTxn *tx,const PartCommand *cmd){
    FdSketch s={0};ProError e;int ids[AS_LIMIT]={0},count=0;
#define SS(call) do{tx->stage=#call;e=(call);trace(#call,e);if(e)goto done;}while(0)
    SS(fd_sketch_setup(tx,cmd->arg[3],cmd->arg[4]));SS(fd_create(tx,1));s.feature=tx->feature;
    SS(ProFeatureElemtreeExtract(&s.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&s.tree));SS(fd_section_from_tree(s.tree,&s.section));
    if(!wcscmp(cmd->arg[5],L"RECT")){
        ProBoolean im;SS(ProSectionIntentManagerModeGet(s.section,&im));if(im)SS(ProSectionIntentManagerModeSet(s.section,PRO_B_FALSE));
        double x=as_arg(cmd,6)*tx->scale,y=as_arg(cmd,7)*tx->scale,w=as_arg(cmd,8)*tx->scale,h=as_arg(cmd,9)*tx->scale;
        AsPoint p[]={ {x,y},{x+w,y},{x+w,y+h},{x,y+h} };
        for(int i=0;i<4;i++)SS(as_add_line(s.section,p[i],p[(i+1)%4],&ids[i]));count=4;
    }else SS(fd_shape_append(s.section,cmd,5,tx->scale,ids,&count));
    SS(fd_section_solve(s.section,tx->scale));
    tx->options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    SS(ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,tx->options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx->errors));SS(fd_verify(tx));
done:fd_sketch_free(&s);return e;
#undef SS
}
