static void vt_route_point(const AmFrame *frame,const double *center,double r,double theta,double scale,ProVector p){
    am_world(frame,(center[0]+r*cos(theta))*scale,(center[1]+r*sin(theta))*scale,center[2]*scale,p);
}

static ProError vt_center_inside(ProMdl model,const ProOutline bounds,const ProVector point,double scale,int *inside){
    ProRay ray={{point[0],point[1],bounds[0][2]-scale},{0,0,1}};
    QualityRay hits;
    *inside=0;
    ProError error=qa_ray(model,&ray,0.0001*scale,&hits);
    if(error)return error;
    double depth=point[2]-ray.start_point[2],epsilon=0.0002*scale;
    for(int i=0;i<hits.count;i+=2)if(depth>=hits.crossing[i].depth-epsilon&&depth<=hits.crossing[i+1].depth+epsilon){
        *inside=1;
        break;
    }
    return PRO_TK_NO_ERROR;
}

static ProError vt_route(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
    enum {segments=24};
    FdRef ref={0};
    ProCsys csys;
    ProGeomitemdata *raw=NULL;
    ProSolidBody body;
    ProOutline bounds;
    double mm,center[]={as_arg(cmd,3),as_arg(cmd,4),as_arg(cmd,5)};
    double r=as_arg(cmd,6),envelope=as_arg(cmd,7)/2+as_arg(cmd,8);
    ProError error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodyOutlineGet(&body,bounds);
    if(!error)error=qa_mm_per_unit(model,&mm);
    if(!error)error=fd_ref_resolve(model,cmd->arg[2],PRO_CSYS,&ref);
    if(!error)error=ProGeomitemToCsys(&ref.item,&csys);
    if(!error)error=ProCsysDataGet(csys,&raw);
    if(!error){
        AmFrame frame={0};
        frame.csys=*raw->data.p_csys_data;
        double scale=1/mm,step=AS_PI/(2*segments),sag=2*r*scale*pow(sin(step/4),2),lower=INFINITY;
        int collision=0;
        for(int i=0;i<segments;i++){
            ProVector a,b,p;
            vt_route_point(&frame,center,r,i*step,scale,a);
            vt_route_point(&frame,center,r,(i+1)*step,scale,b);
            double dist=0;
            for(int j=0;j<3;j++){
                double lo=fmin(a[j],b[j])-sag,hi=fmax(a[j],b[j])+sag;
                double gap=fmax(0,fmax(bounds[0][j]-hi,lo-bounds[1][j]));
                dist+=gap*gap;
            }
            double clearance=sqrt(dist)*mm-envelope;
            lower=fmin(lower,clearance);
            if(clearance>0.0002)continue;
            vt_route_point(&frame,center,r,(i+0.5)*step,scale,p);
            int inside;
            if(!vt_center_inside(model,bounds,p,scale,&inside)&&inside){collision=1;break;}
        }
        PartText out={report,capacity,0};
        if(collision)part_text_add(&out,L"转弯空间不足。已发现 90° 转弯中心线与当前零件实体相交。");
        else if(lower>0.0002)part_text_add(&out,L"转弯包络与当前实体包围盒完全分离，计入所需余量后的净距下界 %.8g mm。",lower);
        else part_text_add(&out,L"转弯包络与当前零件的保守范围重叠，需进一步核对局部几何。");
        part_text_add(&out,L"中心线半径 %.8g mm、包络半径 %.8g mm。已只读检查当前零件。",r,envelope);
    }
    if(raw)ProGeomitemdataFree(&raw);
    fd_ref_free(&ref);
    if(error)swprintf_s(report,capacity,L"线束转弯空间检查未完成（Creo %d）。请核对单实体与定位坐标系。",error);
    return error;
}
