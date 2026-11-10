typedef struct {ProMdl model;double z;double area;int id;} QuickFace;

static ProError quick_face_visit(ProSurface surface,ProError status,ProAppData data){
    QuickFace *search=data;
    if(status)return status;
    InspectionPlane plane;
    double area;
    if(!in_plane_read(search->model,surface,&plane)&&plane.normal[2]>1-1e-8&&
       fabs(plane.point[2]-search->z)<1e-7&&!ProSurfaceAreaEval(surface,&area)&&area>search->area){
        search->id=plane.id;search->area=area;
    }
    return PRO_TK_NO_ERROR;
}

static ProError quick_top(const FdTxn *tx,double height,int *face){
    ProSolidBody body;
    ProError error=qa_single_body(tx->model,&body);
    QuickFace search={tx->model,height*tx->scale};
    if(!error)error=ProSolidBodySurfaceVisit(&body,quick_face_visit,&search);
    if(!error&&!search.id)error=PRO_TK_E_NOT_FOUND;
    if(!error)*face=search.id;
    return error;
}

static ProError quick_frame(const FdTxn *tx,double height,PartCommand *cmd,PanelFrame *frame){
    ProError error=quick_top(tx,height,&frame->face_id);
    TemplateDatums datums={0};
    if(!error)error=ProSolidFeatVisit((ProSolid)tx->model,template_feature,NULL,&datums);
    if(error)return error;
    int orient=0;
    for(int i=0;i<datums.planes;i++)if(fabs(datums.normal[i][0])>1-1e-8){orient=datums.plane[i].id;break;}
    if(!orient)return PRO_TK_E_NOT_FOUND;
    swprintf_s(cmd->arg[3],PART_TOKEN_MAX,L"SURFACE:%d",frame->face_id);
    swprintf_s(cmd->arg[5],PART_TOKEN_MAX,L"SURFACE:%d",orient);
    ProCsysdata *axes=&frame->placement.csys;
    axes->origin[2]=height*tx->scale;
    axes->x_vector[0]=axes->y_vector[1]=axes->z_vector[2]=1;
    frame->inward[2]=-1;
    return PRO_TK_NO_ERROR;
}

static ProError quick_box_check(const FdTxn *tx,double width,double depth,double height,int folded){
    ProSolidBody body;
    ProOutline bounds;
    ProError error=qa_single_body(tx->model,&body);
    if(!error)error=ProSolidBodyOutlineGet(&body,bounds);
    if(error)return error;
    const double expected[]={width,depth,height};
    for(int i=0;i<(folded?1:3);i++){
        if(fabs(bounds[0][i]/tx->scale)>0.001||
           fabs(bounds[1][i]/tx->scale-expected[i])>0.001)return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}
