static ProError aw_frame_world(ProMdl root,const wchar_t *id,const wchar_t *token,AxDeviceFrame *frame){
    AxComponent owner={0};
    ProError error=ax_ref_occurrence(root,id,&owner);
    if(error)return error;
    return ax_device_frame(&owner,token,owner.path.table_num!=0,frame);
}

static double aw_distance3(const double a[3],const double b[3]){
    ProVector difference;
    ad_sub(a,b,difference);
    return sqrt(ad_dot(difference,difference));
}

static ProError aw_square(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame corners[4];
    for(int i=0;i<4;i++){
        ProError error=aw_frame_world(assembly,cmd->arg[4+2*i],cmd->arg[5+2*i],&corners[i]);
        if(error)return error;
    }
    ProVector sides[4],cross,offset;
    double lengths[4],max_parallel=0,max_square=0;
    for(int i=0;i<4;i++){
        ad_sub(corners[(i+1)%4].origin,corners[i].origin,sides[i]);
        lengths[i]=sqrt(ad_dot(sides[i],sides[i]))*mm;
        if(lengths[i]<0.001||!ad_unit(sides[i]))return PRO_TK_BAD_INPUTS;
    }
    for(int i=0;i<4;i++){
        double angle=ax_q_angle(sides[i],sides[(i+1)%4]);
        max_square=fmax(max_square,90-angle);
        max_parallel=fmax(max_parallel,ax_q_angle(sides[i],sides[(i+2)%4]));
        if(ad_dot(sides[i],sides[(i+2)%4])>=0)return PRO_TK_BAD_CONTEXT;
    }
    ad_cross(sides[0],sides[1],cross);
    if(!ad_unit(cross))return PRO_TK_BAD_CONTEXT;
    ad_sub(corners[3].origin,corners[0].origin,offset);
    double coplane=fabs(ad_dot(offset,cross))*mm;
    const double d1=aw_distance3(corners[0].origin,corners[2].origin)*mm,d2=aw_distance3(corners[1].origin,corners[3].origin)*mm;
    double unequal=fmax(fabs(d1-d2),fmax(fabs(lengths[0]-lengths[2]),fabs(lengths[1]-lengths[3])));
    double mm_tol=as_arg(cmd,2),deg_tol=as_arg(cmd,3);
    int pass=ax_q_within(unequal,mm_tol)&&ax_q_within(coplane,mm_tol)&&ax_q_within(max_square,deg_tol)&&ax_q_within(max_parallel,deg_tol);
    part_text_add(out,L"对角线 %.8g / %.8g mm，相差 %.8g mm。四边 %.8g / %.8g / %.8g / %.8g mm。\n",
        d1,d2,fabs(d1-d2),lengths[0],lengths[1],lengths[2],lengths[3]);
    part_text_add(out,L"对边最大不平行 %.8g°，相邻边最大偏离直角 %.8g°，第四角偏平面 %.8g mm。%ls。\n",
        max_parallel,max_square,coplane,pass?L"方正度符合":L"方正度不符合");
    return PRO_TK_NO_ERROR;
}

static ProError aw_twist(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame base,moving;
    ProError error=aw_frame_world(assembly,cmd->arg[2],cmd->arg[3],&base);
    if(!error)error=aw_frame_world(assembly,cmd->arg[4],cmd->arg[5],&moving);
    if(error)return error;
    ProMatrix rotation;
    ax_device_align(&base,&moving,rotation);
    const ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    const double angle=ax_layout_angle(rotation,(double (*)[4])identity);
    double delta[3];
    for(int i=0;i<3;i++)delta[i]=ax_project(moving.origin,&base,i)*mm-as_arg(cmd,6+i);
    double shift=sqrt(ad_dot(delta,delta));
    int pass=ax_q_within(shift,as_arg(cmd,9))&&ax_q_within(angle,as_arg(cmd,10));
    part_text_add(out,L"偏离目标安装站位 (%.8g, %.8g, %.8g) mm，合成 %.8g mm，姿态旋转差 %.8g°。%ls。\n",
        delta[0],delta[1],delta[2],shift,angle,pass?L"符合":L"不符合");
    for(int i=0;i<3;i++){
        const double cosine=fmax(-1,fmin(1,ad_dot(base.axis[i],moving.axis[i])));
        part_text_add(out,L"%lc 轴有向夹角 %.8g°。\n",L'X'+i,acos(cosine)*180/AS_PI);
    }
    return PRO_TK_NO_ERROR;
}

static ProError aw_level(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame datum;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&datum);
    if(error)return error;
    int bad=0;
    for(int i=3;i<cmd->count;i+=6){
        AxQualityFace foot={0};
        error=ax_q_face(assembly,cmd->arg[i],cmd->arg[i+1],mm,&foot);
        if(error)return error;
        if(ax_q_angle(foot.plane.normal,datum.axis[2])>0.01)return PRO_TK_BAD_CONTEXT;
        const double ground=as_arg(cmd,i+2),max_adjust=as_arg(cmd,i+3),engaged=as_arg(cmd,i+4),minimum=as_arg(cmd,i+5);
        const double elevation=ax_project(foot.plane.point,&datum,2)*mm;
        const double adjustment=elevation-ground,remaining=engaged-adjustment;
        const int pass=ax_q_within(fabs(adjustment),max_adjust)&&qa_at_least(remaining,minimum);
        bad+=!pass;
        part_text_add(out,L"%ls 现有脚底标高 %.8g mm，现场地面 %.8g mm，应伸出 %+.8g mm，预计旋入 %.8g mm（要求 ≥ %.8g）。%ls。\n",
            cmd->arg[i],elevation,ground,adjustment,remaining,minimum,pass?L"符合":L"超出调节或旋入范围");
    }
    part_text_add(out,L"共测 %d 只脚，%d 只需复核。正号为继续外伸，负号为回旋。\n",(cmd->count-3)/6,bad);
    return PRO_TK_NO_ERROR;
}

static ProError aw_anchors(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame datum;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&datum);
    if(error)return error;
    const int n=(cmd->count-4)/4;
    double x[AX_BATCH_LIMIT],y[AX_BATCH_LIMIT],u[AX_BATCH_LIMIT],v[AX_BATCH_LIMIT],centroid[4]={0};
    for(int i=0;i<n;i++){
        int at=4+4*i;
        AxDeviceFrame anchor;
        if((error=aw_frame_world(assembly,cmd->arg[at],cmd->arg[at+1],&anchor)))return error;
        x[i]=ax_project(anchor.origin,&datum,0)*mm;
        y[i]=ax_project(anchor.origin,&datum,1)*mm;
        u[i]=as_arg(cmd,at+2);v[i]=as_arg(cmd,at+3);
        centroid[0]+=x[i]/n;centroid[1]+=y[i]/n;centroid[2]+=u[i]/n;centroid[3]+=v[i]/n;
        for(int j=0;j<i;j++)
            if(hypot(x[i]-x[j],y[i]-y[j])<0.001||hypot(u[i]-u[j],v[i]-v[j])<0.001)return PRO_TK_BAD_INPUTS;
    }
    double dot=0,cross=0;
    for(int i=0;i<n;i++){
        double px=x[i]-centroid[0],py=y[i]-centroid[1],qx=u[i]-centroid[2],qy=v[i]-centroid[3];
        dot+=px*qx+py*qy;
        cross+=px*qy-py*qx;
    }
    if(hypot(dot,cross)<1e-8)return PRO_TK_BAD_CONTEXT;
    const double angle=atan2(cross,dot),c=cos(angle),s=sin(angle);
    const double tx=centroid[2]-c*centroid[0]+s*centroid[1],ty=centroid[3]-s*centroid[0]-c*centroid[1];
    const double limit=as_arg(cmd,3);
    int misses=0;
    part_text_add(out,L"最佳平面刚体对齐：围绕坐标系原点旋转 %+.8g°，平移 (%.8g, %.8g) mm。\n",angle*180/AS_PI,tx,ty);
    for(int i=0;i<n;i++){
        double error_x=tx+c*x[i]-s*y[i]-u[i],error_y=ty+s*x[i]+c*y[i]-v[i];
        double residual=hypot(error_x,error_y);
        int pass=ax_q_within(residual,limit);
        misses+=!pass;
        part_text_add(out,L"%ls %ls 残差 (%.8g, %.8g) mm，距离 %.8g mm。%ls。\n",
            cmd->arg[4+i*4],cmd->arg[5+i*4],error_x,error_y,residual,pass?L"符合":L"孔位超差");
    }
    part_text_add(out,L"比较 %d 个已知孔位对应，超差 %d 个。未移动装配。\n",n,misses);
    return PRO_TK_NO_ERROR;
}
