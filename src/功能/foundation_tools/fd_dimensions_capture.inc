static ProError fd_dimensions_capture(ProSection sec,FdDimValue *values,int *count){
    int *ids=NULL,n=0,i;ProError e=ProSecdimIdsGet(sec,&ids,&n);*count=0;
    if(e==PRO_TK_E_NOT_FOUND)return 0;if(e)return e;if(n>256){ProArrayFree((ProArray *)&ids);return PRO_TK_BAD_CONTEXT;}
    for(i=0;i<n;i++){
        values[i].id=ids[i];if((e=ProSecdimTypeGet(sec,ids[i],&values[i].type))||(e=ProSecdimValueGet(sec,ids[i],&values[i].value)))break;(*count)++;
    }if(ids)ProArrayFree((ProArray *)&ids);return e;
}
