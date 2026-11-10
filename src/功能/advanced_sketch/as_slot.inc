static ProError as_slot(ProSection section,const PartCommand *cmd,int first,double scale,int *ids){
    double angle=as_arg(cmd,first+4)*AS_PI/180;
    double radius=as_arg(cmd,first+3)*scale/2;
    AsPoint center=as_point(as_arg(cmd,first)*scale,as_arg(cmd,first+1)*scale);
    AsPoint along=as_mul(as_ray(angle),as_arg(cmd,first+2)*scale/2-radius);
    AsPoint across=as_mul(as_left(as_ray(angle)),radius);
    AsPoint low=as_minus(center,along),high=as_plus(center,along);
    ProError e;
    if((e=as_add_line(section,as_minus(low,across),as_minus(high,across),&ids[0]))||
       (e=as_add_arc(section,high,radius,angle-AS_PI/2,angle+AS_PI/2,&ids[1]))||
       (e=as_add_line(section,as_plus(high,across),as_plus(low,across),&ids[2])))return e;
    return as_add_arc(section,low,radius,angle+AS_PI/2,angle+3*AS_PI/2,&ids[3]);
}
