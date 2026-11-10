static ProError as_repair(ProSection section,const AsAudit *before,double tolerance,double scale,AsEdit *edit){
    AsLine lines[AS_AUDIT_LIMIT];
    int ids[AS_AUDIT_LIMIT],mate[AS_AUDIT_LIMIT*2],count=0,i,j;
    double min_size=0.001*scale;
    ProError e;
    if(!before->gaps||before->branches||before->duplicates||before->crossings||before->unsupported)return PRO_TK_BAD_INPUTS;
    for(i=0;i<before->gaps;i++){
        mate[i]=-1;
        for(j=0;j<before->gaps;j++){
            if(i==j||before->ends[i].id==before->ends[j].id||
               !as_near(before->ends[i].point,before->ends[j].point,tolerance))continue;
            if(mate[i]>=0)return PRO_TK_E_AMBIGUOUS;
            mate[i]=j;
        }
        if(mate[i]<0)return PRO_TK_E_NOT_FOUND;
    }
    for(i=0;i<before->gaps;i++){
        const AsEnd *end=&before->ends[i],*other=&before->ends[mate[i]];
        AsPoint midpoint=as_mul(as_plus(end->point,other->point),0.5);
        for(j=0;j<count;j++)if(ids[j]==end->id)break;
        if(j==count){
            if((e=as_line_get(section,end->id,&lines[count])))return e;
            ids[count++]=end->id;
        }
        if(end->end)lines[j].b=midpoint;else lines[j].a=midpoint;
    }
    for(i=0;i<count;i++){
        if(as_dist(lines[i].a,lines[i].b)<min_size)return PRO_TK_BAD_INPUTS;
        if((e=as_replace_line(section,ids[i],lines[i],edit)))return e;
    }
    return PRO_TK_NO_ERROR;
}
