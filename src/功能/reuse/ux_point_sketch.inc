static ProError ux_point_sketch(FdTxn *tx,const PartCommand *cmd){FdSketch s={0};ProError e;
#define PS(call) do{if((e=(call)))goto done;}while(0)
 PS(fd_sketch_setup(tx,cmd->arg[3],cmd->arg[4]));PS(fd_create(tx,1));s.feature=tx->feature;PS(ProFeatureElemtreeExtract(&s.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&s.tree));PS(fd_section_from_tree(s.tree,&s.section));
 {ProBoolean im;PS(ProSectionIntentManagerModeGet(s.section,&im));if(im)PS(ProSectionIntentManagerModeSet(s.section,PRO_B_FALSE));}
 for(int i=5;i<cmd->count;i+=2){Pro2dPointdef point={0};int id;point.type=PRO_2D_POINT;point.pnt[0]=as_arg(cmd,i)*tx->scale;point.pnt[1]=as_arg(cmd,i+1)*tx->scale;PS(ProSectionEntityAdd(s.section,(Pro2dEntdef *)&point,&id));}
 PS(fd_section_solve(s.section,tx->scale));tx->options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;PS(ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,tx->options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx->errors));PS(fd_verify(tx));
done:fd_sketch_free(&s);return e;
#undef PS
}
