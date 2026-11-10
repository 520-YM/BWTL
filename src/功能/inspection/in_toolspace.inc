typedef struct {
    ProCsysdata csys;
    double min[3],max[3],body_min[3],body_max[3];
} InspectionBox;

static ProError in_box_frame(ProMdl model,const wchar_t *token,ProCsysdata *frame){
    FdRef ref={0};
    ProCsys csys;
    ProGeomitemdata *raw=NULL;
    ProError error=fd_ref_resolve(model,token,PRO_CSYS,&ref);
    if(!error)error=ProGeomitemToCsys(&ref.item,&csys);
    if(!error)error=ProCsysDataGet(csys,&raw);
    if(!error)*frame=*raw->data.p_csys_data;
    if(raw)ProGeomitemdataFree(&raw);
    fd_ref_free(&ref);
    return error;
}

static ProError in_box_screen(const Inspection *inspect,const PartCommand *cmd,int at,const InspectionBend *bend,PartText *out){
    InspectionBox box={0};
    ProOutline bounds;
    ProError error=in_box_frame(inspect->model,cmd->arg[at],&box.csys);
    if(!error)error=ProSolidBodyOutlineGet((ProSolidBody *)&inspect->body,bounds);
    if(error)return error;
    const double *axes[]={box.csys.x_vector,box.csys.y_vector,box.csys.z_vector};
    if(bend&&fabs(ad_dot(bend->axis,axes[0]))<1-1e-6)return PRO_TK_BAD_CONTEXT;
    double margin=as_arg(cmd,at+7),squared=0;
    for(int i=0;i<3;i++){
        double center=as_arg(cmd,at+1+i),half=as_arg(cmd,at+4+i)/2+margin;
        box.min[i]=(center-half)/inspect->mm;
        box.max[i]=(center+half)/inspect->mm;
        for(int j=0;j<3;j++){
            double a=(bounds[0][j]-box.csys.origin[j])*axes[i][j],b=(bounds[1][j]-box.csys.origin[j])*axes[i][j];
            box.body_min[i]+=fmin(a,b);box.body_max[i]+=fmax(a,b);
        }
        double gap=fmax(0,fmax(box.min[i]-box.body_max[i],box.body_min[i]-box.max[i]));
        squared+=gap*gap;
    }
    double lower=sqrt(squared)*inspect->mm;
    if(lower>0.0002){
        part_text_add(out,L"%ls净空确认。实体包围盒与含余量的检查包络完全分离，净距下界 %.8g mm。",
            bend?L"折弯工具":L"设备安装",lower);
        return PRO_TK_NO_ERROR;
    }
    int unknown=0;
    for(int i=0;i<3;i++)for(int a=0;a<5;a++)for(int b=0;b<5;b++){
        int j=(i+1)%3,k=(i+2)%3;
        double begin=fmin(box.min[i],box.body_min[i])-1/inspect->mm;
        double v=box.min[j]+(a+0.5)*(box.max[j]-box.min[j])/5;
        double w=box.min[k]+(b+0.5)*(box.max[k]-box.min[k])/5;
        ProRay ray;
        for(int c=0;c<3;c++){
            ray.start_point[c]=box.csys.origin[c]+begin*axes[i][c]+v*axes[j][c]+w*axes[k][c];
            ray.dir_vector[c]=axes[i][c];
        }
        QualityRay hits;
        error=qa_ray(inspect->model,&ray,0.0001/inspect->mm,&hits);
        if(error){unknown++;continue;}
        for(int h=0;h<hits.count;h+=2){
            double intersection=fmin(box.max[i]-begin,hits.crossing[h+1].depth)-fmax(box.min[i]-begin,hits.crossing[h].depth);
            if(intersection*inspect->mm>0.0002){
                part_text_add(out,L"%ls净空不足。原生射线证实实体侵入含余量的检查包络，测得该方向交叠 %.8g mm。",
                    bend?L"折弯工具":L"设备安装",intersection*inspect->mm);
                return PRO_TK_NO_ERROR;
            }
        }
    }
    part_text_add(out,L"%ls包络与实体的保守范围重叠，当前取样未确认侵入，需进一步复核。",
        bend?L"折弯工具":L"设备安装");
    if(unknown)part_text_add(out,L"%d 条射线因相切或求交状态未参与结论。",unknown);
    return PRO_TK_NO_ERROR;
}

static ProError in_toolspace(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    InspectionBend bend;
    ProError error=in_bend_geometry(inspect,cmd->arg[2],&bend);
    if(error)return error;
    error=in_box_screen(inspect,cmd,3,&bend,out);
    if(!error)part_text_add(out,L"已核对实际折弯 %.8g°。检查针对输入的单一姿态与避让区。",fabs(bend.angle));
    return error;
}
