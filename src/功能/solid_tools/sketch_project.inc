
        stage="resolve projected geometry";e=fd_ref_resolve(model,cmd->arg[3],PRO_TYPE_UNUSED,&ref);if(e)goto done;
        if(ref.item.type!=PRO_EDGE&&ref.item.type!=PRO_CURVE){e=PRO_TK_INVALID_TYPE;goto done;}
        {ProFeature source;int before,after;if((e=ProFeatureInit((ProSolid)model,ref.owner_feature,&source))||(e=ProFeatureNumberGet(&source,&before))||(e=ProFeatureNumberGet(&s.feature,&after)))goto done;if(before>=after){e=PRO_TK_BAD_CONTEXT;goto done;}}
        stage="create associated projected geometry";e=ProSectionEntityUseEdge(s.section,ref.selection,&created);if(e)goto done;
    
