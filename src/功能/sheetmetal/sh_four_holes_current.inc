
/* Sheet-metal plate four-hole group. References are read from trimmed solid
   geometry, rather than guessing a datum or a signed extrusion direction. */
typedef struct {
    ProMdl model;
    double mm,thickness,volume;
    CabinetFace face;
    int count;
} ShFourFaces;

static ProError sh_four_face(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    ShFourFaces *faces=data;
    ProSmtSurfType side;
    InspectionPlane plane;
    double area;
    if(ProSmtSurfaceTypeGet((ProPart)faces->model,surface,&side)||side!=PRO_SMT_SURF_FACE||
       in_plane_read(faces->model,surface,&plane))return PRO_TK_NO_ERROR;
    ProError error=ProSurfaceAreaEval(surface,&area);
    if(error)return error;
    if(!part_close(area*faces->mm*faces->mm*faces->thickness,faces->volume))return PRO_TK_NO_ERROR;
    faces->count++;
    faces->face.id=plane.id;
    faces->face.area=area;
    memcpy(faces->face.point,plane.point,sizeof(plane.point));
    memcpy(faces->face.normal,plane.normal,sizeof(plane.normal));
    return PRO_TK_NO_ERROR;
}

static ProError sh_four_outer(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    return error||traversal!=PRO_CONTOUR_TRAV_EXTERNAL?error:cm_blank_contour(contour,0,data);
}

static ProError sh_four_frame(ShFourFaces *faces,const ProSolidBody *body,PanelFrame *panel,
    double *width,double *height,int *orient_id){
    ProError error=ProSolidBodySurfaceVisit((ProSolidBody *)body,sh_four_face,faces);
    if(error||faces->count!=1)return error?error:PRO_TK_E_NOT_FOUND;
    ProSurface drive;
    CabinetBlank bounds={.model=faces->model,.face=faces->face.id};
    if((error=ProSurfaceInit((ProSolid)faces->model,bounds.face,&drive))||
       (error=ProSurfaceContourVisit(drive,sh_four_outer,NULL,&bounds)))return error;
    if(bounds.count!=4)return PRO_TK_INVALID_TYPE;

    int longest=0;
    for(int i=1;i<bounds.count;i++){
        if(bounds.edges[i].length>bounds.edges[longest].length+1e-7||
            (part_close(bounds.edges[i].length,bounds.edges[longest].length)&&
             fabs(bounds.edges[i].axis[0])>fabs(bounds.edges[longest].axis[0])+1e-7))longest=i;
    }
    const CabinetEdge *line=&bounds.edges[longest];
    ProCsysdata *frame=&panel->placement.csys;
    memcpy(frame->x_vector,line->axis,sizeof(ProVector));
    memcpy(frame->z_vector,faces->face.normal,sizeof(ProVector));
    ad_cross(frame->z_vector,frame->x_vector,frame->y_vector);
    if(!ad_unit(frame->y_vector))return PRO_TK_BAD_CONTEXT;
    int aligned_x=0,aligned_y=0;
    for(int i=0;i<bounds.count;i++){
        const ProVector *axis=&bounds.edges[i].axis;
        if(fabs(ad_dot(*axis,frame->x_vector))>1-1e-8)aligned_x++;
        else if(fabs(ad_dot(*axis,frame->y_vector))>1-1e-8)aligned_y++;
    }
    if(aligned_x!=2||aligned_y!=2)return PRO_TK_INVALID_TYPE;
    for(int i=0;i<3;i++)panel->inward[i]=-frame->z_vector[i];
    panel->face_id=bounds.face;

    double x0=INFINITY,x1=-INFINITY,y0=INFINITY,y1=-INFINITY;
    for(int i=0;i<bounds.count;i++){
        const CabinetEdge *edge=&bounds.edges[i];
        const double *ends[]={edge->a,edge->b};
        for(int j=0;j<2;j++){
            const double x=ad_dot(ends[j],frame->x_vector),y=ad_dot(ends[j],frame->y_vector);
            x0=fmin(x0,x);x1=fmax(x1,x);
            y0=fmin(y0,y);y1=fmax(y1,y);
        }
    }
    *width=(x1-x0)*faces->mm;
    *height=(y1-y0)*faces->mm;
    const double z=ad_dot(faces->face.point,frame->z_vector);
    for(int i=0;i<3;i++)frame->origin[i]=(x0+x1)*0.5*frame->x_vector[i]+
        (y0+y1)*0.5*frame->y_vector[i]+z*frame->z_vector[i];

    ProEdge edge;
    ProSurface neighbor[2];
    if((error=ProEdgeInit((ProSolid)faces->model,line->id,&edge))||
       (error=ProEdgeNeighborsGet(edge,NULL,NULL,&neighbor[0],&neighbor[1])))return error;
    for(int i=0;i<2;i++){
        ProSmtSurfType type;
        if(!ProSmtSurfaceTypeGet((ProPart)faces->model,neighbor[i],&type)&&type==PRO_SMT_SURF_SIDE)
            return ProSurfaceIdGet(neighbor[i],orient_id);
    }
    return PRO_TK_E_NOT_FOUND;
}

typedef struct {
    const PanelFrame *frame;
    const PanelPlan *plan;
    double scale;
    unsigned matched;
} ShFourReadback;

static ProError sh_four_cylinder(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    ShFourReadback *check=data;
    ProSurface face;
    ProGeomitemdata *raw=NULL;
    ProError error=ProGeomitemToSurface(item,&face);
    if(!error)error=ProSurfaceDataGet(face,&raw);
    if(!error&&(!raw||!raw->data.p_surface_data))error=PRO_TK_GENERAL_ERROR;
    if(!error&&raw->data.p_surface_data->type==PRO_SRF_CYL){
        const ProCylinderdata *c=&raw->data.p_surface_data->srf_shape.cylinder;
        AsPoint point=fx_local_xy(check->frame,c->origin,check->scale);
        error=PRO_TK_GENERAL_ERROR;
        if(fabs(ad_dot(c->e3,check->frame->placement.csys.z_vector))>1-1e-8){
            for(int i=0;i<check->plan->count;i++){
                const PanelProfile *hole=&check->plan->through[i];
                if(hypot(point.x-hole->x,point.y-hole->y)>0.0002||
                   fabs(2*fabs(c->radius)/check->scale-hole->width)>0.0002)continue;
                check->matched|=1u<<i;
                error=PRO_TK_NO_ERROR;
                break;
            }
        }
    }
    if(raw)ProGeomitemdataFree(&raw);
    return error;
}

static ProError sh_four_verify(ProMdl model,const wchar_t *name,const PanelFrame *frame,
    const PanelPlan *plan,double scale){
    ProFeature cut;
    ProFeattype type;
    ProFeatStatus state;
    ShFourReadback check={frame,plan,scale,0};
    ProError error=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,(ProModelitem *)&cut);
    if(!error)error=ProFeatureTypeGet(&cut,&type);
    if(!error)error=ProFeatureStatusGet(&cut,&state);
    if(!error&&(type!=PRO_FEAT_CUT_SMT||state!=PRO_FEAT_ACTIVE))error=PRO_TK_INVALID_TYPE;
    if(!error)error=ProFeatureGeomitemVisit(&cut,PRO_SURFACE,sh_four_cylinder,NULL,&check);
    return error?error:check.matched==15?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError sh_four_names(ProMdl model,ProName cut,ProName sketch){
    ProModelitem used;
    for(int i=1;i<=100;i++){
        swprintf_s(cut,PRO_NAME_SIZE,L"BWTL_FOUR_HOLES_%d",i);
        swprintf_s(sketch,PRO_NAME_SIZE,L"%ls_SK",cut);
        const ProError a=ProModelitemByNameInit(model,PRO_FEATURE,cut,&used);
        const ProError b=ProModelitemByNameInit(model,PRO_FEATURE,sketch,&used);
        if(a==PRO_TK_E_NOT_FOUND&&b==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
        if(a&&a!=PRO_TK_E_NOT_FOUND)return a;
        if(b&&b!=PRO_TK_E_NOT_FOUND)return b;
    }
    return PRO_TK_E_FOUND;
}

static ProError sh_four_holes_current(ProMdl model,double diameter,double pitch_x,double pitch_y,
    wchar_t *report,size_t capacity){
    ProMdl current=NULL;
    ProMdlsubtype subtype;
    ProSolidBody body;
    FdTxn tx={0};
    ProName name,sketch;
    PartCommand refs={0};
    PanelFrame frame={0};
    FastenerPlan plan={.margin=0.01};
    ProMatrix placement;
    ShFourFaces faces={.model=model};
    int mutated=0,orientation=0;
    const wchar_t *stage=L"确认当前模型和孔径、孔距";
    ProError error=PRO_TK_BAD_INPUTS;
    if(!report||!capacity)return error;
    report[0]=0;
    if(!model||ProMdlCurrentGet(&current)||current!=model||
       ProMdlSubtypeGet(model,&subtype)||subtype!=PROMDLSTYPE_PART_SHEETMETAL){
        wcscpy_s(report,capacity,L"请先激活要打孔的单实体钣金平板。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(sh_recovery_uncertain||pn_recovery_uncertain){
        wcscpy_s(report,capacity,L"之前的加工恢复未核实，请先检查该钣金件，再在新的 Creo 会话中继续。");
        return PRO_TK_BAD_CONTEXT;
    }
    if(!isfinite(diameter)||diameter<0.1||!isfinite(pitch_x)||!isfinite(pitch_y)||
       pitch_x<diameter+0.01||pitch_y<diameter+0.01){
        wcscpy_s(report,capacity,L"孔径须至少 0.1 mm，横纵孔距都须大于孔径，给相邻孔留出材料。");
        return error;
    }

#define SH4(label,call) do{stage=label;error=(call);if(error)goto done;}while(0)
    SH4(L"核对单实体及板厚",fab_thickness(model,&faces.mm,&faces.thickness));
    SH4(L"读取钣金实体",qa_single_body(model,&body));
    SH4(L"分配四孔特征与草绘名称",sh_four_names(model,name,sketch));
    SH4(L"读取原板实体和加工前体积",sh_begin(&tx,model,name,PRO_FEAT_CUT));
    if(!tx.has_volume||!isfinite(tx.volume)||tx.volume<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
    faces.volume=tx.volume*pow(faces.mm,3);
    SH4(L"定位单一平板的矩形边界与板面",sh_four_frame(&faces,&body,&frame,
        &plan.box_width,&plan.box_height,&orientation));
    swprintf_s(refs.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",frame.face_id);
    swprintf_s(refs.arg[5],PART_TOKEN_MAX,L"SURFACE:%d",orientation);
    swprintf_s(refs.arg[6],PART_TOKEN_MAX,L"%.15g",-plan.box_width/2);
    swprintf_s(refs.arg[7],PART_TOKEN_MAX,L"%.15g",-plan.box_height/2);
    pn_hole_rectangle(&plan.panel,0,0,pitch_x,pitch_y,diameter);
    SH4(L"检查四孔与板边、现有开口之间的净距",fx_check_boundary(&tx,&refs,&frame,&plan));
    for(int i=0;i<plan.panel.count;i++)
        SH4(L"确认每个孔位都有完整板厚的材料",pn_profile_probes(&tx,&frame,&plan.panel.through[i],faces.thickness,0));

    mutated=1;
    SH4(L"在真实板面绘制四个圆",pn_sketch(model,&refs,&frame,sketch,plan.panel.through,plan.panel.count,1,placement));
    SH4(L"沿板厚向内创建原生钣金贯穿切口",pn_cut(&tx,&frame,name,sketch,placement,faces.thickness,1));
    SH4(L"逐孔回读实际孔径和中心位置",sh_four_verify(model,name,&frame,&plan.panel,tx.scale));
    for(int i=0;i<plan.panel.count;i++)
        SH4(L"逐孔确认整块板厚已切穿",pn_profile_probes(&tx,&frame,&plan.panel.through[i],faces.thickness,1));
    SH4(L"核对四孔去料体积",md_expected_volume(&tx,-AS_PI*diameter*diameter*faces.thickness));
    SH4(L"核对钣金件重生成状态",part_health(model));
    swprintf_s(report,capacity,L"已在当前板面居中打好 4 个 Ø%.8g mm 通孔，沿板长和板宽孔距分别为 %.8g 和 %.8g mm。已从 Creo 实体回读孔径、孔位和贯穿结果。未保存。",
        diameter,pitch_x,pitch_y);
done:
    if(error){
        if(mutated){
            wchar_t rollback[512];
            am_finish(&tx,error,1,rollback,_countof(rollback));
            if(!tx.rollback_ok)pn_recovery_uncertain=1;
        }else fd_cleanup(&tx,0);
        swprintf_s(report,capacity,L"四孔未完成。具体卡在「%ls」（Creo %d）。%ls",stage,error,
            mutated?(tx.rollback_ok?L"已核对撤销这次新建的草绘和切口，原板保留。":L"恢复未能完整核实，请检查当前钣金件。"):
                L"还没有写入模型。");
    }else fd_cleanup(&tx,0);
    trace("BWTL native centered four holes",error);
    return error;
#undef SH4
}
