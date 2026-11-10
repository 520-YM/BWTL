typedef struct {ProMdl model;int face,count;CabinetEdge edges[4];} CabinetBlank;

static ProError cm_blank_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    CabinetBlank *blank=data;
    CabinetEdge side;
    if(cm_edge_data(blank->model,edge,&side)||side.face!=blank->face)return PRO_TK_NO_ERROR;
    for(int i=0;i<blank->count;i++)if(blank->edges[i].id==side.id)return PRO_TK_NO_ERROR;
    if(blank->count==4)return PRO_TK_BAD_CONTEXT;
    blank->edges[blank->count++]=side;
    return PRO_TK_NO_ERROR;
}

static ProError cm_blank_contour(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    CabinetBlank *blank=data;
    ProSurface face;
    ProError error=ProSurfaceInit((ProSolid)blank->model,blank->face,&face);
    return error?error:ProContourEdgeVisit(face,contour,cm_blank_edge,NULL,blank);
}

static ProError cm_blank_make(FdTxn *tx,const PartCommand *cmd,double length,double width,double thickness,
    CabinetBlank *blank,ProVector long_axis){
    ProMdlName model;
    ProName sketch;
    ProFeature base;
    CabinetFace face;
    ProSurface surface;
    FdSketch section={0};
    ProMatrix frame;
    swprintf_s(sketch,PRO_NAME_SIZE,L"%ls_SK",tx->name);
    ProError error=ProMdlMdlnameGet(tx->model,model);
    if(!error)error=cm_native_command(tx->model,L"CREO_OP SMT_SKETCH %ls %ls %ls %ls RECT 0 0 %.12g %.12g",
        model,sketch,cmd->arg[3],cmd->arg[4],length,width);
    if(!error)error=fd_sketch_open(tx->model,sketch,&section,0);
    if(!error)error=ProSectionLocationGet(section.section,frame);
    if(!error)memcpy(long_axis,frame[0],sizeof(ProVector));
    fd_sketch_free(&section);
    if(!error)error=cm_native_command(tx->model,L"CREO_OP SMT_FIRST_WALL %ls %ls SKETCH:%ls %.12g POS",
        model,tx->name,sketch,thickness);
    if(!error)error=cm_feature_find(tx->model,tx->name,&base);
    if(!error)error=cm_wall_face(&base,&face);
    if(!error)error=ProSurfaceInit((ProSolid)tx->model,face.id,&surface);
    if(error)return error;
    blank->model=tx->model;
    blank->face=face.id;
    error=ProSurfaceContourVisit(surface,cm_blank_contour,NULL,blank);
    return error?error:blank->count==4?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}
