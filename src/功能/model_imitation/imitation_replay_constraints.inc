static int replay_constraint_supported(ProConstraintType type){
    switch(type){
    case PRO_CONSTRAINT_SAME_POINT:case PRO_CONSTRAINT_HORIZONTAL_ENT:
    case PRO_CONSTRAINT_VERTICAL_ENT:
    case PRO_CONSTRAINT_TANGENT_ENTS:case PRO_CONSTRAINT_ORTHOG_ENTS:
    case PRO_CONSTRAINT_PARALLEL_ENTS:
    case PRO_CONSTRAINT_EQUAL_RADII:case PRO_CONSTRAINT_EQUAL_SEGMENTS:
    case PRO_CONSTRAINT_SYMMETRY:case PRO_CONSTRAINT_MIDDLE_POINT:
        return 1;
    default:return 0;
    }
}

static void replay_section_constraints(ProSection source,ProSection target,
                                       const ReplaySectionEntity *map,int count){
    ProIntlist ids=NULL;
    int total=0;
    if(ProSectionConstraintsIdsGet(source,&ids,&total))return;
    for(int i=0;i<total;i++){
        ProConstraintType type;
        ProConstraintStatus status;
        ProSectionPointType *senses=NULL;
        int *refs=NULL,n=0;
        ProSelection selected[8]={0};
        ProError error=ProSectionConstraintsGet(source,ids[i],&type,&status,&n,&refs,&senses);
        if(!error&&status==PRO_TK_CONSTRAINT_ENABLED&&replay_constraint_supported(type)&&
           n>0&&n<=_countof(selected)){
            for(int j=0;!error&&j<n;j++){
                int k=0;
                while(k<count&&map[k].source!=refs[j])k++;
                if(k==count)error=PRO_TK_E_NOT_FOUND;
                else error=ProSectionEntityGetSelected(target,map[k].target,senses[j],NULL,
                    PRO_VALUE_UNUSED,&selected[j]);
            }
            if(!error){
                int created;
                error=ProSectionConstraintCreate(target,selected,n,type,&created);
                if(error!=PRO_TK_E_FOUND)trace("Replay original sketch constraint",error);
            }
        }
        for(int j=0;j<_countof(selected);j++)if(selected[j])ProSelectionFree(&selected[j]);
        if(refs)ProArrayFree((ProArray *)&refs);
        if(senses)ProArrayFree((ProArray *)&senses);
    }
    if(ids)ProArrayFree((ProArray *)&ids);
}
