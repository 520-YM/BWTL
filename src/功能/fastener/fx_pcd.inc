static void fx_pcd(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double radius=as_arg(cmd,9)/2,start=as_arg(cmd,11)*AS_PI/180;
    int count=(int)as_arg(cmd,10),skip_mask=0;
    for(int i=14;i<cmd->count;i++)skip_mask|=1<<((int)as_arg(cmd,i)-1);
    for(int i=0;i<count;i++){
        if(skip_mask&(1<<i))continue;
        double angle=start+i*2*AS_PI/count;
        pn_circle(&plan->panel,x+radius*cos(angle),y+radius*sin(angle),as_arg(cmd,12));
    }
    plan->margin=0.05;
}
