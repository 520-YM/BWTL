#define IMITATION_QUILT_SAMPLES 7

typedef struct {
    int quilt,id,joined,matched;
    ProEdge handle;
    ProSurface faces[2];
    double length;
    ProVector point[IMITATION_QUILT_SAMPLES];
} ImitationQuiltEdge;

typedef struct {
    ProMdl model;
    double mm;
    int quilt,count,capacity;
    ProSurface surface;
    ImitationQuiltEdge *edges;
} ImitationQuiltEdges;

static ProError imitation_quilt_edge(ProEdge edge,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationQuiltEdges *scan=data;
    int id;
    ProError error=ProEdgeIdGet(edge,&id);
    if(error)return error;
    for(int i=0;i<scan->count;i++)
        if(scan->edges[i].quilt==scan->quilt&&scan->edges[i].id==id)return PRO_TK_NO_ERROR;
    if(scan->count==scan->capacity){
        int capacity=scan->capacity?2*scan->capacity:64;
        if(capacity>8192)return PRO_TK_OUT_OF_RANGE;
        ImitationQuiltEdge *grown=realloc(scan->edges,capacity*sizeof(*grown));
        if(!grown)return PRO_TK_OUT_OF_MEMORY;
        scan->edges=grown;
        scan->capacity=capacity;
    }
    ImitationQuiltEdge item={.quilt=scan->quilt,.id=id,.handle=edge};
    error=ProEdgeLengthEval(edge,&item.length);
    if(!error)error=ProEdgeNeighborsGet(edge,NULL,NULL,&item.faces[0],&item.faces[1]);
    item.joined=item.faces[0]&&item.faces[1];
    for(int i=0;!error&&i<IMITATION_QUILT_SAMPLES;i++){
        error=ProEdgeXyzdataEval(edge,(double)i/(IMITATION_QUILT_SAMPLES-1),item.point[i],NULL,NULL,NULL);
        for(int j=0;!error&&j<3;j++)item.point[i][j]*=scan->mm;
    }
    if(!error){
        item.length*=scan->mm;
        if(!isfinite(item.length)||item.length<=0)return PRO_TK_INCOMPLETE;
        scan->edges[scan->count++]=item;
    }
    return error;
}

static ProError imitation_quilt_contour(ProContour contour,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationQuiltEdges *scan=data;
    return ProContourEdgeVisit(scan->surface,contour,imitation_quilt_edge,NULL,scan);
}

static ProError imitation_quilt_edge_face(ProSurface surface,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationQuiltEdges *scan=data;
    double area;
    ProBoolean datum;
    ProError error=ProSurfaceIsDatumPlane((ProSolid)scan->model,surface,&datum);
    if(error||datum)return error;
    error=imitation_blend_face(scan->model,surface)?
        imitation_blend_face_area(surface,scan->mm,&area):ProSurfaceAreaEval(surface,&area);
    if(error==PRO_TK_EMPTY||error==PRO_TK_OUT_OF_RANGE)return PRO_TK_NO_ERROR;
    if(error)return error;
    scan->surface=surface;
    return ProSurfaceContourVisit(surface,imitation_quilt_contour,NULL,scan);
}

static ProError imitation_quilt_edges_read(const ImitationShapeMetrics *shape,ImitationQuiltEdges *scan)
{
    scan->model=shape->model;
    scan->mm=shape->mm;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<shape->quilt_ids_count;i++){
        ProQuilt quilt;
        scan->quilt=shape->quilt_ids[i];
        error=ProQuiltInit((ProSolid)shape->model,scan->quilt,&quilt);
        int previous=scan->count;
        if(!error)error=ProQuiltSurfaceVisit(quilt,imitation_quilt_edge_face,NULL,scan);
        if(!error&&scan->count==previous)error=PRO_TK_INCOMPLETE;
    }
    return error;
}

static int imitation_edge_position(const ImitationQuiltEdges *scan,const ImitationQuiltEdge *edge,
    const ProVector point,double tolerance,double *parameter)
{
    ProVector native,evaluated;
    for(int i=0;i<3;i++)native[i]=point[i]/scan->mm;
    if(ProEdgeParamEval(edge->handle,native,parameter)||!isfinite(*parameter)||*parameter<-1e-7||*parameter>1+1e-7||
       ProEdgeXyzdataEval(edge->handle,fmax(0,fmin(1,*parameter)),evaluated,NULL,NULL,NULL))return 0;
    for(int i=0;i<3;i++)evaluated[i]*=scan->mm;
    return ad_near(point,evaluated,tolerance);
}

static int imitation_edge_geometry(const ImitationQuiltEdges *a,const ImitationQuiltEdge *old,
    const ImitationQuiltEdges *b,const ImitationQuiltEdge *next,double tolerance)
{
    if(fabs(old->length-next->length)>tolerance)return 0;
    const int last=IMITATION_QUILT_SAMPLES-1;
    if(!ad_near(old->point[0],old->point[last],tolerance)&&
       !(ad_near(old->point[0],next->point[0],tolerance)&&ad_near(old->point[last],next->point[last],tolerance))&&
       !(ad_near(old->point[0],next->point[last],tolerance)&&ad_near(old->point[last],next->point[0],tolerance)))return 0;
    double t;
    for(int i=0;i<IMITATION_QUILT_SAMPLES;i++){
        if(!imitation_edge_position(b,next,old->point[i],tolerance,&t)||
           !imitation_edge_position(a,old,next->point[i],tolerance,&t))return 0;
    }
    return 1;
}

static ProError imitation_quilt_seam(const ImitationQuiltEdges *scan,const ImitationQuiltEdge *edge,
    double t,double *gap,double *angle)
{
    ProVector position,point[2],normal[2];
    ProError error=ProEdgeXyzdataEval(edge->handle,t,position,NULL,NULL,NULL);
    for(int i=0;!error&&i<2;i++){
        ProUvParam uv;
        error=ProSurfaceParamEval((ProSolid)scan->model,edge->faces[i],position,uv);
        if(!error)error=ProSurfaceXyzdataEval(edge->faces[i],uv,point[i],NULL,NULL,normal[i]);
        if(!error&&!ad_unit(normal[i]))error=PRO_TK_INCOMPLETE;
    }
    if(!error){
        *gap=cm_distance(point[0],point[1])*scan->mm;
        *angle=acos(fabs(qa_clamp(ad_dot(normal[0],normal[1]))))*180.0/AS_PI;
    }
    return error;
}

static ProError imitation_quilt_compare(const ImitationShapeMetrics *source,const ImitationShapeMetrics *result,
    double tolerance,PartText *out)
{
    ImitationQuiltEdges a={0},b={0};
    int seams=0,boundaries=0;
    ProError error=imitation_quilt_edges_read(source,&a);
    if(!error)error=imitation_quilt_edges_read(result,&b);
    if(error)part_text_add(out,L"面组边界读取失败，面组 %d，Creo %d。\n",b.quilt?b.quilt:a.quilt,error);
    for(int i=0;!error&&i<a.count;i++){
        const ImitationQuiltEdge *old=&a.edges[i];
        ImitationQuiltEdge *next=NULL;
        for(int j=0;j<b.count;j++){
            ImitationQuiltEdge *candidate=&b.edges[j];
            if(candidate->matched||candidate->joined!=old->joined||
               !imitation_edge_geometry(&a,old,&b,candidate,tolerance))continue;
            next=candidate;
            break;
        }
        if(!next){
            const double *p=old->point[IMITATION_QUILT_SAMPLES/2];
            part_text_add(out,L"面组 %d 的 %ls EDGE:%d 在 (%.6g, %.6g, %.6g) mm 附近存在轮廓或连接差异。\n",
                old->quilt,old->joined?L"接缝":L"轮廓",old->id,p[0],p[1],p[2]);
            error=PRO_TK_INCOMPLETE;
            break;
        }
        next->matched=1;
        if(!old->joined){boundaries++;continue;}
        for(int sample=1;!error&&sample<IMITATION_QUILT_SAMPLES-1;sample++){
            double p=(double)sample/(IMITATION_QUILT_SAMPLES-1),q;
            double gap_a,gap_b,angle_a,angle_b;
            if(!imitation_edge_position(&b,next,old->point[sample],tolerance,&q))error=PRO_TK_INCOMPLETE;
            if(!error)error=imitation_quilt_seam(&a,old,p,&gap_a,&angle_a);
            if(!error)error=imitation_quilt_seam(&b,next,q,&gap_b,&angle_b);
            if(error){
                part_text_add(out,L"面组 %d 的接缝 EDGE:%d 第 %d 个采样点无法量测，Creo %d。\n",
                    old->quilt,old->id,sample,error);
            }else if(fabs(gap_a-gap_b)>tolerance||fabs(angle_a-angle_b)>0.1){
                const double *xyz=old->point[sample];
                part_text_add(out,L"面组 %d 接缝 EDGE:%d 与结果 EDGE:%d 在 (%.6g, %.6g, %.6g) mm 附近不同。间隙 %.6g / %.6g mm，切平面夹角 %.6g / %.6g°。\n",
                    old->quilt,old->id,next->id,xyz[0],xyz[1],xyz[2],gap_a,gap_b,angle_a,angle_b);
                error=PRO_TK_INCOMPLETE;
            }
        }
        if(!error)seams++;
    }
    for(int i=0;!error&&i<b.count;i++)if(!b.edges[i].matched){
        const ImitationQuiltEdge *edge=&b.edges[i];
        const double *p=edge->point[IMITATION_QUILT_SAMPLES/2];
        part_text_add(out,L"结果面组 %d 的 EDGE:%d 在 (%.6g, %.6g, %.6g) mm 附近缺少原件对应边。\n",
            edge->quilt,edge->id,p[0],p[1],p[2]);
        error=PRO_TK_INCOMPLETE;
    }
    if(!error){
        if(boundaries||seams)
            part_text_add(out,L"面组局部核对通过。轮廓边 %d 条，缝合接缝 %d 条，核对位置与切平面夹角。\n",boundaries,seams);
        if(source->quilt_ids_count<source->quilts||result->quilt_ids_count<result->quilts)
            part_text_add(out,L"仅能从历史模型量测的面组沿用面积与实体几何核对。\n");
    }
    free(a.edges);
    free(b.edges);
    return error;
}
