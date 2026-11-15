
typedef ProError (*SmxSketchDraw)(ProSection section,const ProMatrix frame,double scale,void *context);

static ProError smx_custom_sketch(ProMdl model,const wchar_t *name,const wchar_t *plane,const wchar_t *orient,
    SmxSketchDraw draw,void *context,ProMatrix frame){
    FdTxn tx={0};
    FdSketch sketch={0};
    ProError error=sh_begin(&tx,model,name,PRO_FEAT_CURVE);
    if(!error)error=fd_sketch_setup(&tx,plane,orient);
    if(!error)error=fd_create(&tx,1);
    if(!error){
        sketch.feature=tx.feature;
        error=ProFeatureElemtreeExtract(&sketch.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree);
    }
    if(!error)error=fd_section_from_tree(sketch.tree,&sketch.section);
    if(!error)error=ProSectionLocationGet(sketch.section,frame);
    if(!error)error=draw(sketch.section,frame,tx.scale,context);
    if(!error)error=fd_section_solve(sketch.section,tx.scale);
    if(!error){
        tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
        error=ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,tx.options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors);
    }
    if(!error)error=fd_verify(&tx);
    fd_sketch_free(&sketch);
    fd_cleanup(&tx,error);
    return error;
}

static ProError smx_dxf_draw(ProSection section,const ProMatrix frame,double scale,void *context){
    const FabDxf *dxf=context;
    for(int i=0;i<dxf->count;i++){
        const FabEntity *item=&dxf->entities[i];
        int id;
        ProError error;
        if(item->layer==3)continue;
        if(item->type==1)error=as_add_line(section,as_point(item->a[0]*scale,item->a[1]*scale),
            as_point(item->b[0]*scale,item->b[1]*scale),&id);
        else if(item->type==2)error=as_add_arc(section,as_point(item->center[0]*scale,item->center[1]*scale),
            item->radius*scale,item->start*AS_PI/180,(item->start+item->sweep)*AS_PI/180,&id);
        else error=model_circle(section,item->center[0]*scale,item->center[1]*scale,item->radius*scale,&id);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError smx_dxf_rebuild(ProMdl model,const PartCommand *cmd,PartText *out){
    ProPath directory,path;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error)error=fab_leaf_path(directory,cmd->arg[3],L".dxf",path);
    FabDxf *dxf=calloc(1,sizeof(*dxf));
    if(!dxf)return PRO_TK_OUT_OF_MEMORY;
    if(!error)error=fab_dxf_read(path,dxf);
    if(!error&&(dxf->count>256||dxf->bends)){
        part_text_add(out,L"首壁反建需要没有折弯线的单平板轮廓，且切割图元不超过 256 个。\n");
        error=PRO_TK_BAD_CONTEXT;
    }
    FdTxn tx={0};
    ProMatrix frame;
    ProName sketch;
    if(!error)error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_DATUM_SURF);
    if(!error&&!tx.was_empty)error=PRO_TK_BAD_CONTEXT;
    swprintf_s(sketch,_countof(sketch),L"%.25ls_SK",cmd->arg[2]);
    if(!error)error=smx_custom_sketch(model,sketch,cmd->arg[4],cmd->arg[5],smx_dxf_draw,dxf,frame);
    if(!error)error=cm_native_command(model,L"CREO_OP SMT_FIRST_WALL %ls %ls SKETCH:%ls %.12g %ls",
        cmd->arg[1],cmd->arg[2],sketch,as_arg(cmd,6),cmd->arg[7]);
    if(!error){
        double mm,thickness;
        CabinetFace face;
        ProFeature wall;
        CabinetSpan x,y;
        error=fab_thickness(model,&mm,&thickness);
        if(!error)error=cm_feature_find(model,cmd->arg[2],&wall);
        if(!error)error=cm_wall_face(&wall,&face);
        if(!error)error=cm_face_span(model,face.id,frame[0],&x);
        if(!error)error=cm_face_span(model,face.id,frame[1],&y);
        if(!error&&(fabs((x.max-x.min)*mm-dxf->high[0]+dxf->low[0])>0.001||
           fabs((y.max-y.min)*mm-dxf->high[1]+dxf->low[1])>0.001||
           fabs(thickness-as_arg(cmd,6))>0.0001))error=PRO_TK_GENERAL_ERROR;
        if(!error){
            wchar_t fixed[32];
            swprintf_s(fixed,_countof(fixed),L"SURFACE:%d",face.id);
            DeliveryOutput scratch={0};
            FabDxf *actual=calloc(1,sizeof(*actual));
            if(!actual)error=PRO_TK_OUT_OF_MEMORY;
            if(!error)error=fab_scratch(&scratch);
            if(!error)error=dv_stage_enter(&scratch);
            ProPath ignored;
            if(!error)error=fab_flat_read(model,scratch.stage,L"verify",fixed,actual,ignored);
            if(!error&&(actual->outline!=dxf->outline||actual->holes!=dxf->holes))error=PRO_TK_GENERAL_ERROR;
            free(actual);
            error=fab_discard(&scratch,error,out);
        }
    }
    if(error&&tx.root&&tx.was_empty){
        wchar_t rollback[768];
        am_finish(&tx,error,0,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    if(!error)part_text_add(out,L"已按毫米 DXF 重建原生可编辑板壁 %ls，包络 %.8g × %.8g mm，板厚 %.8g mm，重新展开的内外轮廓图元数一致。未保存。\n",
        cmd->arg[2],dxf->high[0]-dxf->low[0],dxf->high[1]-dxf->low[1],as_arg(cmd,6));
    free(dxf);
    return error;
}

typedef struct { ProVector a,b; } SmxLine;
static ProError smx_line_draw(ProSection section,const ProMatrix frame,double scale,void *context){
    const SmxLine *line=context;
    ProVector first,second;
    ad_sub(line->a,frame[3],first);ad_sub(line->b,frame[3],second);
    if(fabs(ad_dot(first,frame[2]))>1e-5*scale||fabs(ad_dot(second,frame[2]))>1e-5*scale)return PRO_TK_BAD_CONTEXT;
    int id;
    return as_add_line(section,as_point(ad_dot(first,frame[0]),ad_dot(first,frame[1])),
        as_point(ad_dot(second,frame[0]),ad_dot(second,frame[1])),&id);
}

static ProError smx_bend_offset(ProMdl model,const PartCommand *cmd,PartText *out){
    FdTxn tx={0};
    CabinetEdge edge;
    InspectionPlane skin;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=cm_edge_get(model,cmd->arg[3],&edge);
    if(error)return error;
    wchar_t face[32];
    swprintf_s(face,_countof(face),L"SURFACE:%d",edge.face);
    error=in_plane(model,face,&skin);
    SmxLine line;
    double offset=as_arg(cmd,4)/mm;
    if(!error&&cmd->count==11){
        FdRef hole={0};
        QualityBore bore;
        error=qa_bore(model,cmd->arg[10],&hole,&bore);
        if(!error&&fabs(ad_dot(bore.axis,skin.normal))<0.999999)error=PRO_TK_BAD_CONTEXT;
        if(!error){
            ProVector to_hole;
            ad_sub(bore.origin,edge.a,to_hole);
            offset+=ad_dot(to_hole,edge.inward);
        }
        fd_ref_free(&hole);
    }
    if(!error&&offset<=0)error=PRO_TK_BAD_CONTEXT;
    cm_point(edge.a,edge.inward,offset,line.a);
    cm_point(edge.b,edge.inward,offset,line.b);
    for(int i=1;!error&&i<8;i++){
        ProVector point;
        cm_point(line.a,edge.axis,edge.length*i/8,point);
        if(!in_on_plane(model,&skin,point))error=PRO_TK_BAD_CONTEXT;
    }
    if(!error)error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_BEND);
    ProName sketch;
    ProMatrix frame;
    swprintf_s(sketch,_countof(sketch),L"%.25ls_SK",cmd->arg[2]);
    if(!error)error=smx_custom_sketch(model,sketch,face,cmd->arg[5],smx_line_draw,&line,frame);
    if(!error)error=cm_native_command(model,L"CREO_OP SMT_BEND %ls %ls CURVE:%ls %.12g %.12g %ls %ls",
        cmd->arg[1],cmd->arg[2],sketch,as_arg(cmd,6),as_arg(cmd,7),cmd->arg[8],cmd->arg[9]);
    if(!error){
        ProFeature bend;
        error=cm_feature_find(model,cmd->arg[2],&bend);
        if(!error)error=cm_bend_verified(&bend,as_arg(cmd,6),as_arg(cmd,7),1/mm);
    }
    if(error&&tx.root){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    if(!error){
        part_text_add(out,L"已在所选板边向内 %.8g mm 创建平行折弯草绘，原生折角和内 R 已回读。未保存。\n",offset*mm);
        if(cmd->count==11)part_text_add(out,L"折弯线距所选孔轴中心的有向偏移 %.8g mm。\n",as_arg(cmd,4));
    }
    return error;
}
