static ProError fd_dimension_create(ProSection sec,ProSecdimType kind,int n,int *ids,ProSectionPointType *senses,double value,double scale,int *dimension){
    int *all=NULL,count=0,i;Pro2dPnt place={10*scale,10*scale};ProError e;int found=-1;
    e=ProSecdimIdsGet(sec,&all,&count);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;
    for(i=0;i<count;i++){
        ProSecdimType type;int refs=0,*entities=NULL;ProSectionPointType *points=NULL;
        if(!ProSecdimTypeGet(sec,all[i],&type)&&type==kind&&!ProSecdimReferencesGet(sec,all[i],&entities,&points,&refs)&&refs==n&&fd_same_refs(n,ids,senses,entities,points))found=all[i];
        if(entities)ProArrayFree((ProArray *)&entities);if(points)ProArrayFree((ProArray *)&points);if(found>=0)break;
    }if(all)ProArrayFree((ProArray *)&all);
    if(found<0){if((e=ProSecdimCreate(sec,ids,senses,n,kind,place,&found)))return e;}
    if((e=ProSecdimValueSet(sec,found,value)))return e;
    e=ProSecdimStrengthen(sec,found);if(e==PRO_TK_E_FOUND)e=0;if(!e)*dimension=found;return e;
}
