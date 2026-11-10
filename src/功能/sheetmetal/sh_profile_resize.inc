static ProError sh_profile_resize(FdTxn *tx,double length,double radius){
    FdSketch sketch={0};ProError e;int *ids=NULL,n=0,height_id=-1;double largest=0;ProBoolean intent;
#define PS(call) do{if((e=(call)))goto done;}while(0)
    sketch.feature=tx->feature;PS(ProFeatureElemtreeExtract(&sketch.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree));PS(fd_section_from_tree(sketch.tree,&sketch.section));
    PS(ProSectionIntentManagerModeGet(sketch.section,&intent));if(intent)PS(ProSectionIntentManagerModeSet(sketch.section,PRO_B_FALSE));
    PS(ProSecdimIdsGet(sketch.section,&ids,&n));
    for(int i=0;i<n;i++){
        ProSecdimType type;double value;PS(ProSecdimTypeGet(sketch.section,ids[i],&type));PS(ProSecdimValueGet(sketch.section,ids[i],&value));
        if(type==PRO_TK_DIM_LINE&&value>largest){height_id=ids[i];largest=value;}
        if(radius>0&&(type==PRO_TK_DIM_RAD||type==PRO_TK_DIM_DIA))PS(ProSecdimValueSet(sketch.section,ids[i],radius*tx->scale*(type==PRO_TK_DIM_DIA?2:1)));
    }
    if(height_id<0){e=PRO_TK_BAD_INPUTS;goto done;}PS(ProSecdimValueSet(sketch.section,height_id,length*tx->scale));
    {ProWSecerror errors=NULL;PS(ProSecerrorAlloc(&errors));e=ProSectionRegenerate(sketch.section,&errors);ProSecerrorFree(&errors);if(e)goto done;}
    tx->options[0]=PRO_FEAT_CR_NO_OPTS;
    PS(ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,tx->options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx->errors));
    {ProElement verify_tree=NULL;ProSection verify_section;double value;
     e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&verify_tree);
     if(!e)e=fd_section_from_tree(verify_tree,&verify_section);if(!e)e=ProSecdimValueGet(verify_section,height_id,&value);
     if(!e&&!part_close(value,length*tx->scale))e=PRO_TK_GENERAL_ERROR;
     if(verify_tree)ProFeatureElemtreeFree(&tx->feature,verify_tree);if(e)goto done;}
done:if(ids)ProArrayFree((ProArray *)&ids);fd_sketch_free(&sketch);return e;
#undef PS
}
