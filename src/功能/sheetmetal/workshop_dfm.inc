typedef struct {
    const wchar_t *grade,*die;
    double thickness,angle,height,contact,length;
    int found;
} SmyDfmRule;

static int smy_dfm_rule(wchar_t **fields,void *context){
    SmyDfmRule *rule=context;
    if(!dv_safe_leaf(fields[0])||!dv_safe_leaf(fields[3]))return 0;
    if(!smx_number(fields[1],0.01,100)||!smx_number(fields[2],0.01,180))return 0;
    for(int i=4;i<7;i++)if(!smx_number(fields[i],0.01,10000))return 0;
    if(_wcsicmp(fields[0],rule->grade)||_wcsicmp(fields[3],rule->die)||
       fabs(wcstod(fields[1],NULL)-rule->thickness)>0.0001||
       fabs(wcstod(fields[2],NULL)-rule->angle)>0.001)return 1;
    if(rule->found)return 0;
    rule->height=wcstod(fields[4],NULL);
    rule->contact=wcstod(fields[5],NULL);
    rule->length=wcstod(fields[6],NULL);
    rule->found=1;
    return 1;
}

typedef struct {
    ProMdl model;
    ProSurface bend;
    ProVector axis;
    double heights[8],contacts[8];
    int faces[8],count;
} SmyFlangeScan;

static ProError smy_dfm_edge(ProEdge edge,ProError status,ProAppData context){
    if(status)return status;
    SmyFlangeScan *scan=context;
    ProEnttype type;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(error||type!=PRO_ENT_LINE)return error;
    ProVector a,b,d1,d2,d3,direction;
    error=ProEdgeXyzdataEval(edge,0,a,d1,d2,d3);
    if(!error)error=ProEdgeXyzdataEval(edge,1,b,d1,d2,d3);
    if(error)return error;
    ad_sub(b,a,direction);
    if(!ad_unit(direction)||fabs(ad_dot(direction,scan->axis))<0.999999)return PRO_TK_NO_ERROR;
    ProSurface faces[2];
    error=ProEdgeNeighborsGet(edge,NULL,NULL,&faces[0],&faces[1]);
    if(error)return error;
    ProSurface wall=faces[0]==scan->bend?faces[1]:faces[0];
    if(!wall||wall==scan->bend)return PRO_TK_BAD_CONTEXT;
    ProSmtSurfType role;
    ProSrftype surface_type;
    error=ProSmtSurfaceTypeGet((ProPart)scan->model,wall,&role);
    if(!error)error=ProSurfaceTypeGet(wall,&surface_type);
    if(error||role!=PRO_SMT_SURF_FACE||surface_type!=PRO_SRF_PLANE)return error;
    InspectionPlane plane;
    error=in_plane_read(scan->model,wall,&plane);
    if(error)return error;
    for(int i=0;i<scan->count;i++)if(scan->faces[i]==plane.id)return PRO_TK_NO_ERROR;
    if(scan->count==_countof(scan->faces))return PRO_TK_OUT_OF_RANGE;
    ProVector across;
    ad_cross(plane.normal,scan->axis,across);
    if(!ad_unit(across))return PRO_TK_BAD_CONTEXT;
    ProVector low,high;
    error=ProSurfaceExtremesEval(wall,across,low,high);
    if(error)return error;
    double origin=ad_dot(a,across);
    double height=fmax(fabs(ad_dot(low,across)-origin),fabs(ad_dot(high,across)-origin));
    CabinetEdgeSearch search={0};
    search.model=scan->model;search.face_id=plane.id;search.parallel=scan->axis;
    error=cm_edges_on_face(&search);
    if(error)return error;
    int i=scan->count++;
    scan->faces[i]=plane.id;
    scan->heights[i]=height;
    scan->contacts[i]=search.matches?search.edge.length:0;
    return PRO_TK_NO_ERROR;
}

static ProError smy_dfm_contour(ProContour contour,ProError status,ProAppData context){
    if(status)return status;
    SmyFlangeScan *scan=context;
    return ProContourEdgeVisit(scan->bend,contour,smy_dfm_edge,NULL,context);
}

static ProError smy_bend_span(ProMdl model,int surface,ProSurface *handle,ProVector axis,ProVector low,ProVector high){
    ProError error=ProSurfaceInit((ProSolid)model,surface,handle);
    ProSrftype shape;
    if(!error)error=ProSurfaceTypeGet(*handle,&shape);
    if(!error&&shape!=PRO_SRF_CYL)error=PRO_TK_BAD_CONTEXT;
    ProGeomitemdata *raw=NULL;
    if(!error)error=ProSurfaceDataGet(*handle,&raw);
    if(!error){
        memcpy(axis,raw->data.p_surface_data->srf_shape.cylinder.e3,sizeof(ProVector));
        if(!ad_unit(axis))error=PRO_TK_BAD_CONTEXT;
    }
    if(raw)ProGeomitemdataFree(&raw);
    return error?error:ProSurfaceExtremesEval(*handle,axis,low,high);
}

static ProError smy_bends_load(ProMdl model,FabBends *bends){
    bends->model=model;
    ProError error=fab_thickness(model,&bends->mm,&bends->thickness);
    if(!error)error=ProSolidSurfaceVisit((ProSolid)model,fab_bend_visit,NULL,bends);
    if(!error)qsort(bends->rows,bends->count,sizeof(bends->rows[0]),fab_bend_order);
    return error;
}

static ProError smy_dfm_run(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    FabBends bends={0};
    ProError error=smy_bends_load(model,&bends);
    if(!error&&!bends.count)return PRO_TK_E_NOT_FOUND;
    int missing=0,bad=0;
    for(int i=0;!error&&i<bends.count;i++){
        const FabBend *row=&bends.rows[i];
        SmyDfmRule rule={cmd->arg[2],cmd->arg[3],bends.thickness,fabs(row->angle)};
        wchar_t text[800]=L"";
        PartText detail={text,_countof(text),0};
        ProError found=smx_table(L"BWTL_SMT_DFM.tsv",7,smy_dfm_rule,&rule,&detail);
        if(found){
            part_text_add(out,L"%ls\n",text);
            return found;
        }
        if(!rule.found){
            missing++;
            part_text_add(out,L"SURFACE:%d 的 %.8g°、板厚 %.8g mm 无匹配模具规则。\n",row->surface,fabs(row->angle),bends.thickness);
            continue;
        }
        SmyFlangeScan scan={0};
        scan.model=model;
        ProVector low,high;
        error=smy_bend_span(model,row->surface,&scan.bend,scan.axis,low,high);
        if(!error)error=ProSurfaceContourVisit(scan.bend,smy_dfm_contour,NULL,&scan);
        if(error)break;
        if(scan.count<2)missing++;
        int too_short=row->length<rule.length;
        if(kind==SMY_DFM_SHORT){
            part_text_add(out,L"SURFACE:%d 原生折弯长 %.8g mm，下限 %.8g mm，%ls。相切平板 %d 面。\n",
                row->surface,row->length,rule.length,too_short?L"过短":L"符合",scan.count);
            bad+=too_short;
        }
        for(int j=0;j<scan.count;j++){
            double h=scan.heights[j]*bends.mm,hold=scan.contacts[j]*bends.mm;
            int height_short=h<rule.height,contact_short=hold<rule.contact;
            if(kind==SMY_DFM_FLANGE)
                part_text_add(out,L"弯面 %d / 板面 %d：直段包络深度 %.8g mm，下限 %.8g mm，%ls；最长自由挡料直边 %.8g mm，下限 %.8g mm，%ls。\n",
                    row->surface,scan.faces[j],h,rule.height,height_short?L"不足":L"符合",hold,rule.contact,contact_short?L"不足":L"符合");
            else part_text_add(out,L"板面 %d 的最长连续自由边 %.8g mm，要求 %.8g mm，%ls。\n",
                scan.faces[j],hold,rule.contact,contact_short?L"挡料不足":L"符合");
            bad+=contact_short+(kind==SMY_DFM_FLANGE&&height_short);
        }
    }
    if(!error)part_text_add(out,L"超限 %d 项，待复核 %d 处。直段取几何包络，挡料取实际连续直边。\n",bad,missing);
    return error?error:missing?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
