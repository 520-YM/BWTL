static ProError as_trim(ProSection section,const PartCommand *cmd,double scale,AsEdit *edit){
    int id=as_id(cmd,5),start=!wcscmp(cmd->arg[4],L"START"),trim=!wcscmp(cmd->arg[3],L"TRIM");
    AsLine target,reference;
    AsPoint crossing;
    double t,u,tol=1e-5*scale,epsilon;
    ProError e;
    if((e=as_line_get(section,id,&target))||(e=as_line_get(section,as_id(cmd,6),&reference)))return e;
    if(!as_line_intersect(target,reference,&t,&u)||u< -1e-7||u>1+1e-7)return PRO_TK_BAD_INPUTS;
    epsilon=tol/as_dist(target.a,target.b);
    if(trim?(t<=epsilon||t>=1-epsilon):(start?t>=-epsilon:t<=1+epsilon))return PRO_TK_BAD_INPUTS;
    crossing=as_plus(target.a,as_mul(as_direction(target),t));
    if(start)target.a=crossing;else target.b=crossing;
    return as_replace_line(section,id,target,edit);
}
