
typedef struct {
    PartText *out;
    int checked,failed,unknown;
} AxQuality;

static const wchar_t *ax_q_check(AxQuality *q,int pass){
    q->checked++;
    if(!pass)q->failed++;
    return pass?L"符合":L"不符合";
}

static void ax_q_missing(AxQuality *q,const wchar_t *label,ProError error){
    q->unknown++;
    part_text_add(q->out,L"%ls 未核实（Creo %d）。\n",label,error);
}

static int ax_q_within(double actual,double limit){
    return isfinite(actual)&&actual<=limit+1e-8*fmax(1,limit);
}

static double ax_q_angle(const double *a,const double *b){
    return acos(fmin(1,fabs(ad_dot(a,b))))*180/AS_PI;
}

static ProError ax_q_bore(ProMdl assembly,const wchar_t *path,const wchar_t *surface,
    double mm,ProVector point,ProVector axis){
    AxComponent owner={0};
    AxGeometry cylinder;
    ProVector ends[2],center;
    ProMatrix pose;
    int bore=0;
    ProError error=ax_hw_occurrence(assembly,path,mm,&owner);
    if(!error)error=ax_hw_cylinder(&owner,surface,&cylinder,ends,&bore);
    if(error)return error;
    if(!bore)return PRO_TK_INVALID_TYPE;
    ProVector span;
    for(int i=0;i<3;i++)span[i]=(ends[0][i]+ends[1][i])*0.5-cylinder.point[i];
    double t=ad_dot(span,cylinder.direction);
    for(int i=0;i<3;i++)center[i]=cylinder.point[i]+t*cylinder.direction[i];
    if((error=ax_device_pose(&owner,pose))||(error=ProPntTrfEval(center,pose,point))||
       (error=ProVectorTrfEval(cylinder.direction,pose,axis)))return error;
    return ad_unit(axis)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_q_holes(ProMdl assembly,const PartCommand *cmd,double mm,AxQuality *q){
    ProVector a,b,u,v,delta;
    ProError error=ax_q_bore(assembly,cmd->arg[2],cmd->arg[3],mm,a,u);
    if(!error)error=ax_q_bore(assembly,cmd->arg[4],cmd->arg[5],mm,b,v);
    if(error)return error;
    double angle=ax_q_angle(u,v),limit=as_arg(cmd,6),max_angle=as_arg(cmd,7);
    double projection=ad_dot(v,u);
    if(fabs(projection)<1e-8){
        part_text_add(q->out,L"%ls/%ls ↔ %ls/%ls，孔轴夹角 %.8g°（≤ %.8g°），第二孔轴不能投至首孔中截面，偏心未定义。%ls。\n",
            cmd->arg[2],cmd->arg[3],cmd->arg[4],cmd->arg[5],angle,max_angle,ax_q_check(q,0));
        return PRO_TK_NO_ERROR;
    }
    ad_sub(b,a,delta);
    double t=-ad_dot(delta,u)/projection;
    for(int i=0;i<3;i++)delta[i]+=t*v[i];
    double offset=sqrt(ad_dot(delta,delta))*mm;
    part_text_add(q->out,L"%ls/%ls ↔ %ls/%ls，首孔中截面偏心 %.8g mm（≤ %.8g），孔轴夹角 %.8g°（≤ %.8g°）。%ls。\n",
        cmd->arg[2],cmd->arg[3],cmd->arg[4],cmd->arg[5],offset,limit,angle,max_angle,
        ax_q_check(q,ax_q_within(offset,limit)&&ax_q_within(angle,max_angle)));
    return PRO_TK_NO_ERROR;
}

typedef struct {AxComponent owner;AxDevicePlane plane;} AxQualityFace;

static ProError ax_q_face(ProMdl assembly,const wchar_t *path,const wchar_t *token,double mm,AxQualityFace *face){
    ProError error=ax_hw_occurrence(assembly,path,mm,&face->owner);
    FdRef material={0};ProSurface solid;
    if(!error)error=ax_device_plane_world(&face->owner,token,&face->plane);
    if(!error)error=qa_surface(face->owner.model,token,1,&material,&solid);
    fd_ref_free(&material);
    return error;
}

static ProError ax_q_span(AxQualityFace *face,AxDevicePlane *reference,double mm,double *low,double *high){
    ProMatrix pose,inverse;
    AxDeviceFrame local={0};
    ProError error=ax_device_pose(&face->owner,pose);
    if(error)return error;
    ax_device_inverse(pose,inverse);
    if((error=ProPntTrfEval(reference->point,inverse,local.origin))||
       (error=ProVectorTrfEval(reference->normal,inverse,local.axis[0]))||
       (error=ax_surface_span(face->plane.handle,&local,0,low,high)))return error;
    *low*=mm;
    *high*=mm;
    return PRO_TK_NO_ERROR;
}

static ProError ax_q_planes(ProMdl assembly,const PartCommand *cmd,int kind,double mm,AxQuality *q){
    int flush=kind==AX_Q_FLUSH,start=flush?2:4;
    double limit=as_arg(cmd,flush?6:2),max_angle=as_arg(cmd,flush?7:3);
    AxQualityFace reference={0};
    ProError error=ax_q_face(assembly,cmd->arg[start],cmd->arg[start+1],mm,&reference);
    if(error)return error;
    for(int i=start+2;i<(flush?6:cmd->count);i+=2){
        AxQualityFace face={0};
        double low,high;
        if((error=ax_q_face(assembly,cmd->arg[i],cmd->arg[i+1],mm,&face))||
           (error=ax_q_span(&face,&reference.plane,mm,&low,&high)))return error;
        double step=fmax(fabs(low),fabs(high)),angle=ax_q_angle(face.plane.normal,reference.plane.normal);
        if(flush){
            double reverse_low,reverse_high;
            if((error=ax_q_span(&reference,&face.plane,mm,&reverse_low,&reverse_high)))return error;
            step=fmax(step,fmax(fabs(reverse_low),fabs(reverse_high)));
        }
        part_text_add(q->out,L"%ls/%ls ↔ %ls/%ls，沿基准法向 [%.8g, %.8g] mm，最大%ls %.8g mm（≤ %.8g），夹角 %.8g°（≤ %.8g°）。%ls。\n",
            cmd->arg[start],cmd->arg[start+1],cmd->arg[i],cmd->arg[i+1],low,high,flush?L"段差":L"偏离",
            step,limit,angle,max_angle,ax_q_check(q,ax_q_within(step,limit)&&ax_q_within(angle,max_angle)));
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_q_stack(const PartCommand *cmd,AxQuality *q){
    double nominal=0,low=0,high=0,required_low=as_arg(cmd,2),required_high=as_arg(cmd,3);
    if(required_low>required_high)return PRO_TK_BAD_INPUTS;
    for(int i=4;i<cmd->count;i+=5){
        double value=as_arg(cmd,i+2),minus=as_arg(cmd,i+3),plus=as_arg(cmd,i+4);
        int add=!wcscmp(cmd->arg[i],L"ADD");
        nominal+=add?value:-value;
        low+=add?value-minus:-value-plus;
        high+=add?value+plus:-value+minus;
        part_text_add(q->out,L"%ls %ls %.8g mm，公差 -%.8g / +%.8g mm。\n",cmd->arg[i],cmd->arg[i+1],value,minus,plus);
    }
    part_text_add(q->out,L"尺寸链名义值 %.9g mm，最坏区间 [%.9g, %.9g] mm，要求 [%.9g, %.9g] mm。%ls。负值表示过盈。\n",
        nominal,low,high,required_low,required_high,ax_q_check(q,low>=required_low-1e-8&&high<=required_high+1e-8));
    return PRO_TK_NO_ERROR;
}

static ProError ax_q_root_frame(ProMdl assembly,const wchar_t *token,AxDeviceFrame *frame){
    AxComponent root={.model=assembly};
    return ax_device_frame(&root,token,0,frame);
}

static double ax_q_side(const double *a,const double *b,const double *p){
    return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);
}

static ProError ax_q_support(ProMdl assembly,const PartCommand *cmd,double mm,AxQuality *q){
    double polygon[12][2];
    int n=(cmd->count-4)/2,sign=0;
    AxDeviceFrame frame;
    ProMassProperty material;
    AxScan *scan=NULL;
    ProError error=ax_q_root_frame(assembly,cmd->arg[2],&frame);
    if(error)return error;
    for(int i=0;i<n;i++)for(int j=0;j<2;j++)polygon[i][j]=as_arg(cmd,4+2*i+j);
    for(int i=0;i<n;i++)for(int j=2;j<n;j++){
        double side=ax_q_side(polygon[i],polygon[(i+1)%n],polygon[(i+j)%n]);
        if(fabs(side)<1e-7||(sign&&sign!=(side>0?1:-1)))return PRO_TK_BAD_INPUTS;
        sign=side>0?1:-1;
    }
    if((error=ax_scan(assembly,1,&scan)))return error;
    int parts=scan->parts;
    free(scan);
    if(!parts)return PRO_TK_E_NOT_FOUND;
    if((error=ProSolidMassPropertyWithDensityGet((ProSolid)assembly,NULL,PRO_MP_DENS_DEFAULT,0,&material)))return error;
    if(!isfinite(material.mass)||material.mass<=0)return PRO_TK_BAD_CONTEXT;
    double center[2];
    for(int i=0;i<2;i++){
        center[i]=ax_project(material.center_of_gravity,&frame,i)*mm;
        if(!isfinite(center[i]))return PRO_TK_BAD_CONTEXT;
    }
    double margin=INFINITY,limit=as_arg(cmd,3);
    for(int i=0;i<n;i++){
        const double *a=polygon[i],*b=polygon[(i+1)%n];
        margin=fmin(margin,sign*ax_q_side(a,b,center)/hypot(b[0]-a[0],b[1]-a[1]));
    }
    part_text_add(q->out,L"%ls XY 面，按 Creo 当前密度设置计算重心投影 (%.8g, %.8g) mm，最小内侧支撑裕量 %.8g mm（≥ %.8g）。%ls。\n",
        cmd->arg[2],center[0],center[1],margin,limit,ax_q_check(q,margin>=limit-1e-8));
    return PRO_TK_NO_ERROR;
}
