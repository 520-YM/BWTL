typedef struct {
    ShDxf *projection;
    FILE *dxf,*csv;
    int holes,skipped,outer;
    int internal,circular,edges;
    double center[2],radius,length;
} FabTemplate;

static ProError fab_template_edge(ProEdge edge,ProError status,ProAppData data){
    FabTemplate *s=data;
    if(status)return status;
    if(!s->internal){
        ShDxfEdge entry={0};
        entry.edge=edge;
        s->outer++;
        return sh_dxf_write_edge(s->dxf,s->projection,&entry);
    }
    if(!s->circular)return PRO_TK_NO_ERROR;
    ProEnttype type;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(error)return error;
    if(type!=PRO_ENT_ARC&&type!=PRO_ENT_CIRCLE){s->circular=0;return PRO_TK_NO_ERROR;}
    ProGeomitemdata *raw=NULL;
    if((error=ProEdgeDataGet(edge,&raw)))return error;
    const ProCurvedata *curve=raw->data.p_curve_data;
    double center[2],length;
    double radius=type==PRO_ENT_ARC?curve->arc.radius:curve->circle.radius;
    error=sh_project(s->projection,type==PRO_ENT_ARC?curve->arc.origin:curve->circle.center,center);
    ProGeomitemdataFree(&raw);
    if(error||(error=ProEdgeLengthEval(edge,&length)))return error;
    radius/=s->projection->scale;
    if(!s->edges){
        memcpy(s->center,center,sizeof(center));
        s->radius=radius;
    }else if(!fab_close_xy(center,s->center,FAB_JOIN)||fabs(radius-s->radius)>FAB_JOIN)s->circular=0;
    s->length+=length/s->projection->scale;
    s->edges++;
    return PRO_TK_NO_ERROR;
}

static ProError fab_template_loop(ProContour contour,ProError status,ProAppData data){
    FabTemplate *s=data;
    if(status)return status;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    if(error)return error;
    if(traversal!=PRO_CONTOUR_TRAV_INTERNAL&&traversal!=PRO_CONTOUR_TRAV_EXTERNAL)return PRO_TK_BAD_CONTEXT;
    s->internal=traversal==PRO_CONTOUR_TRAV_INTERNAL;
    s->edges=0;s->length=0;s->circular=1;
    error=ProContourEdgeVisit(s->projection->surface,contour,fab_template_edge,NULL,s);
    if(error||!s->internal)return error;
    if(!s->edges||!s->circular||s->radius<=FAB_JOIN||
       fabs(s->length-2*AS_PI*s->radius)>fmax(0.0001,s->radius*1e-6)){
        s->skipped++;
        return PRO_TK_NO_ERROR;
    }
    if(fprintf(s->dxf,"0\nCIRCLE\n8\nHOLES\n10\n%.15g\n20\n%.15g\n30\n0\n40\n%.15g\n",s->center[0],s->center[1],s->radius)<0||
       fwprintf(s->csv,L"%d,%.12g,%.12g,%.12g\n",++s->holes,s->center[0],s->center[1],2*s->radius)<0)
        return PRO_TK_CANT_WRITE;
    return PRO_TK_NO_ERROR;
}

static ProError fab_template(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm;
    InspectionPlane plane;
    ProCsysdata frame;
    ProError error=qa_single_body(model,NULL);
    if(!error)error=qa_mm_per_unit(model,&mm);
    if(!error)error=in_plane(model,cmd->arg[3],&plane);
    if(!error)error=in_box_frame(model,cmd->arg[4],&frame);
    if(error)return error;
    ProVector delta;
    ad_sub(frame.origin,plane.point,delta);
    if(fabs(ad_dot(delta,plane.normal))*mm>FAB_JOIN||ad_dot(frame.z_vector,plane.normal)<1-1e-7)
        return PRO_TK_BAD_CONTEXT;
    ShDxf *projection=calloc(1,sizeof(*projection));
    FabDxf *check=calloc(1,sizeof(*check));
    if(!projection||!check){free(projection);free(check);return PRO_TK_OUT_OF_MEMORY;}
    projection->surface=plane.handle;projection->scale=1/mm;
    memcpy(projection->origin,frame.origin,sizeof(ProVector));
    memcpy(projection->u,frame.x_vector,sizeof(ProVector));
    memcpy(projection->v,frame.y_vector,sizeof(ProVector));
    memcpy(projection->normal,plane.normal,sizeof(ProVector));

    DeliveryOutput work={0};
    FabTemplate s={projection};
    ProPath path;
    error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_leaf_path(work.stage,L"hole-template",L".dxf",path);
    if(!error&&!(s.dxf=_wfopen(path,L"wt")))error=PRO_TK_CANT_WRITE;
    if(!error)error=fab_csv_open(work.stage,L"hole-centers.csv",&s.csv);
    if(!error){
        fprintf(s.dxf,"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n"
            "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n2\n"
            "0\nLAYER\n2\nOUTLINE\n70\n0\n62\n1\n6\nCONTINUOUS\n"
            "0\nLAYER\n2\nHOLES\n70\n0\n62\n2\n6\nCONTINUOUS\n"
            "0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n");
        fwprintf(s.csv,L"孔口编号,X_mm,Y_mm,直径_mm\n");
        error=ProSurfaceContourVisit(plane.handle,fab_template_loop,NULL,&s);
        if(!error&&(!s.outer||!s.holes))error=PRO_TK_E_NOT_FOUND;
        fprintf(s.dxf,"0\nENDSEC\n0\nEOF\n");
    }
    error=fab_close(s.csv,error);
    error=fab_close(s.dxf,error);
    if(!error)error=fab_dxf_read(path,check);
    if(!error&&check->holes!=s.holes)error=PRO_TK_BAD_CONTEXT;
    free(check);free(projection);
    error=dv_stage_finish(&work,error,out);
    if(!error)part_text_add(out,L"已生成 %d 个完整圆孔口的二维模板，跳过 %d 个非整圆内环。\n%ls\\hole-template.dxf\n%ls\\hole-centers.csv",
        s.holes,s.skipped,work.target,work.target);
    return error;
}
