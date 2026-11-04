static ProError fd_constraint_evidence(ProSection section,ProConstraintType kind,int n,int *ids,ProSectionPointType *senses,int *created){
    int i,*all=NULL,count=0,h[2]={0},v[2]={0};ProError e;
    e=ProSectionConstraintsIdsGet(section,&all,&count);if(e)return e;
    e=PRO_TK_E_NOT_FOUND;
    for(i=0;i<count;i++){
        ProConstraintType actual;ProConstraintStatus state;int refs=0,*entities=NULL;ProSectionPointType *points=NULL;ProError read;
        read=ProSectionConstraintsGet(section,all[i],&actual,&state,&refs,&entities,&points);
        if(!read&&actual==kind&&state==PRO_TK_CONSTRAINT_ENABLED&&refs==n&&fd_same_refs(n,ids,senses,entities,points)){e=0;*created=all[i];}
        if(!read&&kind==PRO_CONSTRAINT_SYMMETRY&&actual==kind&&state==PRO_TK_CONSTRAINT_ENABLED&&n==3&&refs==3&&entities[0]==ids[2]&&points[0]==senses[2]&&fd_same_refs(2,ids,senses,entities+1,points+1)){e=0;*created=all[i];}
        if(!read&&state==PRO_TK_CONSTRAINT_ENABLED&&refs==1&&n==2){int k;for(k=0;k<2;k++)if(entities[0]==ids[k]){if(actual==PRO_CONSTRAINT_HORIZONTAL_ENT)h[k]=all[i]+1;if(actual==PRO_CONSTRAINT_VERTICAL_ENT)v[k]=all[i]+1;}}
        if(entities)ProArrayFree((ProArray *)&entities);if(points)ProArrayFree((ProArray *)&points);
        if(!e)break;
    }
    /* Creo canonicalizes parallel-to-horizontal into another horizontal
       constraint. Verify BOTH enabled native constraints, not just geometry. */
    if(e&&n==2&&kind==PRO_CONSTRAINT_PARALLEL_ENTS&&((h[0]&&h[1])||(v[0]&&v[1]))){e=0;*created=(h[0]&&h[1]?h[1]:v[1])-1;}
    if(all)ProArrayFree((ProArray *)&all);return e;
}
