static void fx_tie(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double length=as_arg(cmd,9),width=as_arg(cmd,10),half=as_arg(cmd,11)/2,angle=as_arg(cmd,12)*AS_PI/180;
    AsPoint delta=as_mul(as_left(as_ray(angle)),half);
    fx_capsule(plan,x-delta.x,y-delta.y,length,width,angle);
    fx_capsule(plan,x+delta.x,y+delta.y,length,width,angle);
}
