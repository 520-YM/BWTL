
typedef struct { ProVector points[4]; } SmxTrimPolygon;

static ProError smx_trim_polygon(ProSection section,const ProMatrix frame,double scale,void *context){
    const SmxTrimPolygon *polygon=context;
    AsPoint points[4];
    for(int i=0;i<4;i++){
        ProVector delta;
        ad_sub(polygon->points[i],frame[3],delta);
        points[i]=as_point(ad_dot(delta,frame[0]),ad_dot(delta,frame[1]));
    }
    for(int i=0;i<4;i++){
        int id;
        ProError error=as_add_line(section,points[i],points[(i+1)%4],&id);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError smx_miter_tip(ProMdl model,const wchar_t *reference,CabinetEdge *edge){
    ProError error=cm_edge_get(model,reference,edge);
    ProFeature wall;
    ProFeattype type;
    if(!error)error=ProFeatureInit((ProSolid)model,edge->owner,&wall);
    if(!error)error=ProFeatureTypeGet(&wall,&type);
    if(!error&&type!=PRO_FEAT_WALL)error=PRO_TK_BAD_CONTEXT;
    if(!error){
        CabinetEdge readback;
        error=cm_wall_tip(model,&wall,edge,&readback);
        if(!error&&readback.id!=edge->id)error=PRO_TK_BAD_CONTEXT;
    }
    return error;
}

static ProError smx_trim_probe(FdTxn *tx,const PanelFrame *panel,AsPoint sample,double thickness,int present){
    ProRay ray;
    QualityRay hits;
    const double eps=0.0001*tx->scale;
    const double depth=thickness*tx->scale;
    am_world(&panel->placement,sample.x*tx->scale,sample.y*tx->scale,eps*5,ray.start_point);
    memcpy(ray.dir_vector,panel->inward,sizeof(ray.dir_vector));
    ProError error=qa_ray(tx->model,&ray,eps,&hits);
    if(error)return error;
    int local=0;
    for(int i=0;i<hits.count;i++)
        if(hits.crossing[i].depth>=0&&hits.crossing[i].depth<=depth+eps*6)local++;
    if(!present)return local?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
    return local==2&&fabs(hits.crossing[0].depth-eps*5)<=eps&&
        fabs(hits.crossing[1].depth-depth-eps*5)<=eps?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError smx_trim_cut(FdTxn *outer,const PanelFrame *panel,const wchar_t *name,const wchar_t *sketch,
    const ProMatrix frame,double thickness){
    FdTxn cut={0};
    PartCommand cmd={0};
    swprintf_s(cmd.arg[3],PART_TOKEN_MAX,L"SKETCH:%ls",sketch);
    wcscpy_s(cmd.arg[4],PART_TOKEN_MAX,ad_dot(frame[2],panel->inward)<0?L"POS":L"NEG");
    ProError error=sh_begin(&cut,outer->model,name,PRO_FEAT_CUT);
    if(!error)error=sh_cut_depth(&cut,&cmd,thickness+0.001);
    if(!error)error=part_regenerate(outer->model);
    if(!error)error=sh_verify(&cut,&cmd,SH_CUT);
    fd_cleanup(&cut,error);
    return error;
}

static ProError smx_cut_miter(FdTxn *outer,const CabinetEdge *tip,int end,const wchar_t *name,
    double at_tip,double at_root,double retained,double thickness,int *cut_face){
    CabinetSpan span;
    ProError error=cm_face_span(outer->model,tip->face,tip->inward,&span);
    if(error)return error;
    double depth=(span.max-span.min)/outer->scale-retained;
    double width=tip->length/outer->scale;
    if(depth<=0.1||fmax(at_tip,at_root)>=width-0.1||fabs(at_tip-at_root)<0.0001)return PRO_TK_BAD_INPUTS;
    PanelFrame panel={0};
    ProCsysdata *frame=&panel.placement.csys;
    memcpy(frame->origin,end?tip->b:tip->a,sizeof(ProVector));
    memcpy(frame->y_vector,tip->inward,sizeof(ProVector));
    memcpy(frame->z_vector,tip->normal,sizeof(ProVector));
    for(int i=0;i<3;i++){
        frame->x_vector[i]=(end?-1:1)*tip->axis[i];
        panel.inward[i]=-tip->normal[i];
    }
    panel.face_id=tip->face;
    const double slope=(at_root-at_tip)/depth;
    const double overhang=fmin(0.02,fmin(at_tip,at_root)/4);
    AsPoint points[]={{-overhang,-overhang},{at_tip-overhang*slope,-overhang},{at_root,depth},{-overhang,depth}};
    SmxTrimPolygon polygon;
    for(int i=0;i<4;i++)
        am_world(&panel.placement,points[i].x*outer->scale,points[i].y*outer->scale,0,polygon.points[i]);
    AsPoint in[3],keep[3];
    for(int i=0;i<3;i++){
        double y=depth*(i+1)/4,limit=at_tip+slope*y;
        in[i]=as_point(limit-fmin(0.01,limit/4),y);
        keep[i]=as_point(limit+fmin(0.01,(width-limit)/4),y);
        if((error=smx_trim_probe(outer,&panel,in[i],thickness,1))||
           (error=smx_trim_probe(outer,&panel,keep[i],thickness,1))){outer->stage="validate sheet at miter locations";return error;}
    }
    ShOrient orient={0};
    memcpy(orient.normal,tip->normal,sizeof(ProVector));
    error=ProSolidFeatVisit((ProSolid)outer->model,sh_orient_plane,NULL,&orient);
    if(error!=PRO_TK_E_FOUND)return error?error:PRO_TK_E_NOT_FOUND;
    ProName sketch;
    wchar_t face[32],reference[64];
    ProMatrix sketch_frame;
    ProMassProperty before,after;
    swprintf_s(sketch,_countof(sketch),L"%.25ls_SK",name);
    swprintf_s(face,_countof(face),L"SURFACE:%d",tip->face);
    swprintf_s(reference,_countof(reference),L"PLANE:%ls",orient.name);
    outer->stage="draw exact diagonal miter polygon";
    if((error=qa_geometry_mass(outer->model,&before))||
       (error=smx_custom_sketch(outer->model,sketch,face,reference,smx_trim_polygon,&polygon,sketch_frame)))return error;
    outer->stage="create and verify native miter cut";
    if((error=smx_trim_cut(outer,&panel,name,sketch,sketch_frame,thickness))||
       (error=qa_geometry_mass(outer->model,&after)))return error;
    outer->stage="check actual miter removal volume";
    double removed=(at_tip+at_root)/2*depth*thickness*pow(outer->scale,3);
    if(!part_close(before.volume-after.volume,removed)){
        trace("SMX actual miter removed micro cubic model units",(int)((before.volume-after.volume)*10000));
        trace("SMX expected miter removed micro cubic model units",(int)(removed*10000));
        return PRO_TK_GENERAL_ERROR;
    }
    for(int i=0;i<3;i++)
        if((error=smx_trim_probe(outer,&panel,in[i],thickness,0))||
           (error=smx_trim_probe(outer,&panel,keep[i],thickness,1))){outer->stage="verify trimmed sheet at miter locations";return error;}
    ProFeature cut;
    error=cm_feature_find(outer->model,name,&cut);
    SmxGeometry *geometry=calloc(1,sizeof(*geometry));
    if(!geometry&&!error)error=PRO_TK_OUT_OF_MEMORY;
    if(!error)error=smx_geometry(outer->model,geometry);
    *cut_face=0;
    ProVector target,witness;
    for(int i=0;i<3;i++)target[i]=-frame->x_vector[i]+slope*frame->y_vector[i];
    if(!ad_unit(target))error=PRO_TK_BAD_CONTEXT;
    am_world(&panel.placement,(at_tip+at_root)/2*outer->scale,depth/2*outer->scale,-thickness/2*outer->scale,witness);
    for(int i=0;!error&&i<geometry->count;i++){
        const SmxFace *face_info=&geometry->faces[i];
        if(face_info->owner!=cut.id||face_info->type!=PRO_SRF_PLANE||ad_dot(face_info->axis,target)<0.9999)continue;
        ProVector delta;
        ad_sub(witness,face_info->point,delta);
        if(fabs(ad_dot(delta,target))<0.001*outer->scale)*cut_face=face_info->id;
    }
    free(geometry);
    return error?error:*cut_face?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError smx_miter_pair(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    FdTxn tx={0};
    CabinetEdge tips[2];
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    for(int i=0;!error&&i<2;i++)error=smx_miter_tip(model,cmd->arg[3+i],&tips[i]);
    if(!error&&(tips[0].owner==tips[1].owner||fabs(ad_dot(tips[0].axis,tips[1].axis))>0.99))error=PRO_TK_BAD_CONTEXT;
    if(error)return error;
    int chosen[2]={0};
    double closest=INFINITY;
    for(int i=0;i<2;i++)for(int j=0;j<2;j++){
        double separation=cm_distance(i?tips[0].b:tips[0].a,j?tips[1].b:tips[1].a);
        if(separation<closest){closest=separation;chosen[0]=i;chosen[1]=j;}
    }
    if(closest>fmin(tips[0].length,tips[1].length)/2)return PRO_TK_BAD_CONTEXT;
    error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_CUT_SMT);
    int surfaces[2]={0};
    double first[2]={as_arg(cmd,5),as_arg(cmd,5)},second[2]={as_arg(cmd,6),as_arg(cmd,6)};
    int tail=7;
    if(kind==SMX_MITER_OFFSET||kind==SMX_WELD_SEAM){
        first[1]=as_arg(cmd,7);second[1]=as_arg(cmd,8);tail=9;
    }
    if(kind==SMX_WELD_SEAM){
        first[0]+=as_arg(cmd,11);first[1]+=as_arg(cmd,11);
        second[0]+=as_arg(cmd,12);second[1]+=as_arg(cmd,12);
        first[0]+=as_arg(cmd,13);first[1]+=as_arg(cmd,13);
        second[0]+=as_arg(cmd,13);second[1]+=as_arg(cmd,13);
    }
    for(int i=0;!error&&i<2;i++){
        ProName cut;
        swprintf_s(cut,_countof(cut),L"%.24ls_%lc",cmd->arg[2],L'A'+i);
        error=smx_cut_miter(&tx,&tips[i],chosen[i],cut,first[i],second[i],as_arg(cmd,tail),thickness,&surfaces[i]);
    }
    if(!error){
        wchar_t one[32],two[32];
        swprintf_s(one,_countof(one),L"SURFACE:%d",surfaces[0]);
        swprintf_s(two,_countof(two),L"SURFACE:%d",surfaces[1]);
        double gap;
        error=smx_surface_distance(model,one,two,&gap);
        if(!error){
            gap*=mm;
            double limit=as_arg(cmd,tail+1);
            if(kind==SMX_WELD_SEAM?fabs(gap-limit)>as_arg(cmd,14):gap<limit)error=PRO_TK_BAD_CONTEXT;
            part_text_add(out,L"两处原生斜切面 %ls/%ls 的实测最小缝隙 %.8g mm。%ls\n",
                one,two,gap,error?L"未满足目标。":L"达到设定要求。");
        }
    }
    if(error&&tx.root){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"两处斜切均已完成，去料、根部保留及缝隙均已核验，未保存。\n");
    return error;
}
