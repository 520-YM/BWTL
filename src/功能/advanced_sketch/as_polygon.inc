static ProError as_polygon(ProSection section,const PartCommand *cmd,int first,double scale,int *ids,int *count){
    AsPoint center=as_point(as_arg(cmd,first+1)*scale,as_arg(cmd,first+2)*scale);
    double radius=as_arg(cmd,first+4)*scale,angle=as_arg(cmd,first+5)*AS_PI/180,step;
    int i,n;
    ProError e;
    part_integer(cmd->arg[first],&n);*count=n;
    step=2*AS_PI/n;
    if(!wcscmp(cmd->arg[first+3],L"SIDE")){radius/=cos(step/2);angle-=step/2;}
    for(i=0;i<n;i++){
        AsPoint a=as_plus(center,as_mul(as_ray(angle+step*i),radius));
        AsPoint b=as_plus(center,as_mul(as_ray(angle+step*((i+1)%n)),radius));
        if((e=as_add_line(section,a,b,&ids[i])))return e;
    }
    return PRO_TK_NO_ERROR;
}
