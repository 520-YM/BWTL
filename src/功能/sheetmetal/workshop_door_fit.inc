typedef struct {
    ProMdl model;
    ProCsysdata frame;
    InspectionPlane plane;
    double mm;
} SmyDoorFace;

static ProError smy_door_face(ProMdl model,const wchar_t *csys,const wchar_t *surface,SmyDoorFace *face){
    face->model=model;
    ProError error=qa_mm_per_unit(model,&face->mm);
    if(!error)error=in_box_frame(model,csys,&face->frame);
    if(!error)error=in_plane(model,surface,&face->plane);
    if(!error&&fabs(ad_dot(face->frame.z_vector,face->plane.normal))<0.999999)error=PRO_TK_BAD_CONTEXT;
    return error;
}

static int smy_door_sample(const SmyDoorFace *face,double x,double y,double *z){
    const ProCsysdata *frame=&face->frame;
    ProVector delta,point;
    ad_sub(face->plane.point,frame->origin,delta);
    *z=ad_dot(delta,frame->z_vector)*face->mm;
    for(int i=0;i<3;i++)
        point[i]=frame->origin[i]+(x*frame->x_vector[i]+y*frame->y_vector[i]+*z*frame->z_vector[i])/face->mm;
    return in_on_plane(face->model,&face->plane,point);
}

static ProError smy_door_fit(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    ProMdl other=NULL;
    int loaded=0;
    ProError error=ax_load_part(cmd->arg[2],&other,&loaded);
    SmyDoorFace door={0},case_face={0};
    if(!error)error=smy_door_face(model,cmd->arg[3],cmd->arg[4],&door);
    if(!error)error=smy_door_face(other,cmd->arg[5],cmd->arg[6],&case_face);
    if(!error){
        double dz=ad_dot(door.frame.z_vector,door.plane.normal);
        double cz=ad_dot(case_face.frame.z_vector,case_face.plane.normal);
        if((kind==SMY_CASE_FLUSH&&dz*cz<=0)||(kind==SMY_CASE_SEAL&&dz*cz>=0)){
            part_text_add(out,L"面法向与检查类型不匹配。请核对两件对应坐标系的 Z 方向及实际板面。\n");
            error=PRO_TK_BAD_CONTEXT;
        }
        double x=as_arg(cmd,7),y=as_arg(cmd,8);
        double door_z,case_z;
        if(!error&&(!smy_door_sample(&door,x,y,&door_z)||!smy_door_sample(&case_face,x,y,&case_z))){
            part_text_add(out,L"指定 XY 检测点未同时落在两件的有效板面内。\n");
            error=PRO_TK_BAD_CONTEXT;
        }
        if(!error&&kind==SMY_CASE_FLUSH){
            double offset=door_z-case_z,target=as_arg(cmd,9),tolerance=as_arg(cmd,10);
            part_text_add(out,L"齐平检查：门板减箱体 %.8g mm，目标 %.8g mm，偏差 %.8g mm，允许 ±%.8g mm，%ls。\n",
                offset,target,offset-target,tolerance,fabs(offset-target)<=tolerance+0.000001?L"符合":L"超限");
        }
        if(!error&&kind==SMY_CASE_SEAL){
            double gap=(case_z-door_z)*dz,free_height=as_arg(cmd,9),min=as_arg(cmd,10),max=as_arg(cmd,11);
            double half_width=as_arg(cmd,12)/2,half_height=as_arg(cmd,13)/2;
            for(int i=-1;!error&&i<=1;i++)for(int j=-1;!error&&j<=1;j++){
                if(i==0&&j==0)continue;
                double a,b,px=x+i*half_width,py=y+j*half_height;
                if(!smy_door_sample(&door,px,py,&a)||!smy_door_sample(&case_face,px,py,&b)){
                    part_text_add(out,L"密封区域 XY=(%.8g, %.8g) 未同时落在槽底和承压面内。\n",px,py);
                    error=PRO_TK_BAD_CONTEXT;
                }else if(fabs((b-a)*dz-gap)>0.001)error=PRO_TK_BAD_CONTEXT;
            }
            if(!error&&(gap<0||max<min)){
                part_text_add(out,L"槽底与承压面沿指定法向互相穿过，或压缩率区间颠倒。\n");
                error=PRO_TK_BAD_CONTEXT;
            }else if(!error){
                double compression=100*(free_height-gap)/free_height;
                part_text_add(out,L"槽底至承压面 %.8g mm，密封条自由高 %.8g mm，压缩率 %.8g%%，允许 %.8g–%.8g%%，%ls。已核对指定矩形内 9 个位置。\n",
                    gap,free_height,compression,min,max,compression>=min-0.000001&&compression<=max+0.000001?L"符合":L"超限");
            }
        }
    }
    if(loaded&&other){
        ProError erase=ProMdlErase(other);
        if(!error)error=erase;
    }
    return error;
}
