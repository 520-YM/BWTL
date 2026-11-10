static ProError as_corner(ProSection section,const PartCommand *cmd,double scale,AsEdit *edit){
    int ids[2]={as_id(cmd,5),as_id(cmd,6)};
    int round=!wcscmp(cmd->arg[3],L"ROUND"),i,endpoint[2]={-1,-1};
    AsLine line[2];
    AsPoint ends[2][2],unit[2],cut[2],origin,center;
    double size=as_arg(cmd,4)*scale,tol=1e-5*scale,len[2],angle,offset,a,b;
    ProError e;
    for(i=0;i<2;i++){
        if((e=as_line_get(section,ids[i],&line[i])))return e;
        ends[i][0]=line[i].a;ends[i][1]=line[i].b;len[i]=as_dist(line[i].a,line[i].b);
        if(len[i]<=tol)return PRO_TK_BAD_INPUTS;
    }
    for(i=0;i<2;i++)for(int j=0;j<2;j++)if(as_near(ends[0][i],ends[1][j],tol)){
        if(endpoint[0]>=0)return PRO_TK_E_AMBIGUOUS;
        endpoint[0]=i;endpoint[1]=j;
    }
    if(endpoint[0]<0)return PRO_TK_BAD_INPUTS;
    origin=as_mul(as_plus(ends[0][endpoint[0]],ends[1][endpoint[1]]),0.5);
    for(i=0;i<2;i++)unit[i]=as_mul(as_minus(ends[i][1-endpoint[i]],ends[i][endpoint[i]]),1/len[i]);
    angle=acos(fmax(-1,fmin(1,as_dot2(unit[0],unit[1]))));
    if(angle<1e-5||angle>AS_PI-1e-5)return PRO_TK_BAD_INPUTS;
    offset=round?size/tan(angle/2):size;
    if(offset>=fmin(len[0],len[1])-tol||offset<=tol)return PRO_TK_BAD_INPUTS;
    for(i=0;i<2;i++){
        cut[i]=as_plus(origin,as_mul(unit[i],offset));
        if(endpoint[i])line[i].b=cut[i];else line[i].a=cut[i];
        if((e=as_replace_line(section,ids[i],line[i],edit)))return e;
    }
    edit->count=1;
    if(!round)return as_add_line(section,cut[0],cut[1],edit->ids);
    center=as_plus(origin,as_mul(as_plus(unit[0],unit[1]),size/sin(angle)));
    a=atan2(cut[0].y-center.y,cut[0].x-center.x);
    b=atan2(cut[1].y-center.y,cut[1].x-center.x);
    if(as_cross2(as_minus(cut[0],center),as_minus(cut[1],center))<0){double tmp=a;a=b;b=tmp;}
    if(b<=a)b+=2*AS_PI;
    return as_add_arc(section,center,size,a,b,edit->ids);
}
