static ProError sd_body_count(ProMdl model,int *count){
    ProSolidBody *bodies=NULL;int n=0,i;ProError e;*count=0;
    e=ProSolidBodiesCollect((ProSolid)model,&bodies);if(e==PRO_TK_E_NOT_FOUND)return 0;if(e)return e;
    e=ProArraySizeGet((ProArray)bodies,&n);if(!e)for(i=0;i<n;i++){ProSolidBodyState state;if((e=ProSolidBodyStateGet(&bodies[i],&state)))break;if(state==PRO_BODY_STATE_ACTIVE)(*count)++;}
    if(bodies)ProArrayFree((ProArray *)&bodies);return e;
}
