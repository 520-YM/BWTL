typedef struct {InspectionPlane plane;ProVector witness;double area;} OrganizePlane;
typedef struct {
    const OrganizeView *view;
    ProVector direction;
    double minimum;
    int use_axis,limit,count,total;
    OrganizePlane faces[24];
} OrganizePlanes;

static ProError org_plane_visit(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    OrganizePlanes *s=data;
    OrganizePlane entry={0};
    ProError error=in_plane_read(s->view->model,surface,&entry.plane);
    if(error==PRO_TK_INVALID_TYPE)return PRO_TK_NO_ERROR;
    if(error)return error;
    if(s->use_axis&&ad_dot(entry.plane.normal,s->direction)<0.999)return PRO_TK_NO_ERROR;
    if((error=ProSurfaceAreaEval(surface,&entry.area)))return error;
    entry.area*=s->view->mm*s->view->mm;
    if(entry.area<=0||entry.area<s->minimum)return PRO_TK_NO_ERROR;
    ProGeomitemdata *raw=NULL;
    if((error=ProSurfaceDataGet(surface,&raw)))return error;
    int found=0;
    for(int i=0;!found&&i<7;i++)for(int j=0;!found&&j<7;j++){
        ProVector normal;
        error=qa_interior_sample(s->view->model,surface,raw->data.p_surface_data,i,j,7,entry.witness,normal);
        if(error==PRO_TK_E_NOT_FOUND)continue;
        if(error)goto done;
        found=1;
    }
    error=PRO_TK_NO_ERROR;
    if(!found)goto done;
    s->total++;
    int at=0;
    while(at<s->count&&(s->faces[at].area>entry.area||
        (s->faces[at].area==entry.area&&s->faces[at].plane.id<entry.plane.id)))at++;
    if(at<s->limit){
        if(s->count<s->limit)s->count++;
        for(int j=s->count-1;j>at;j--)s->faces[j]=s->faces[j-1];
        s->faces[at]=entry;
    }
done:
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError org_planes(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    OrganizePlanes scan={0};
    ProSolidBody body;
    scan.view=view;scan.minimum=as_arg(cmd,3);scan.limit=(int)as_arg(cmd,4);
    scan.use_axis=wcscmp(cmd->arg[2],L"ALL")!=0;
    if(scan.use_axis)scan.direction[cmd->arg[2][1]-L'X']=cmd->arg[2][0]==L'+'?1:-1;
    ProError error=qa_single_body(view->model,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,org_plane_visit,&scan);
    if(error)return error;
    part_text_add(out,L"安装平面候选 %d 个，按实测面面积列出 %d 个。\n",scan.total,scan.count);
    for(int i=0;i<scan.count;i++){
        const OrganizePlane *p=&scan.faces[i];
        double mm=view->mm;
        part_text_add(out,L"SURFACE:%d｜%.8g mm²｜外法向 (%.6g, %.6g, %.6g)｜面内见证点 (%.8g, %.8g, %.8g) mm\n",
            p->plane.id,p->area,p->plane.normal[0],p->plane.normal[1],p->plane.normal[2],
            p->witness[0]*mm,p->witness[1]*mm,p->witness[2]*mm);
    }
    return PRO_TK_NO_ERROR;
}
