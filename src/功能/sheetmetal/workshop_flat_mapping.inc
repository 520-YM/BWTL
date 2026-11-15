static ProError smy_projection_frame(ProMdl model,const wchar_t *fixed,ShDxf *frame){
    InspectionPlane plane;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=in_plane(model,fixed,&plane);
    ProSmtSurfType side;
    if(!error)error=ProSmtSurfaceTypeGet((ProPart)model,plane.handle,&side);
    if(!error&&side!=PRO_SMT_SURF_FACE)error=PRO_TK_BAD_CONTEXT;
    if(error)return error;
    frame->model=model;
    frame->scale=1/mm;
    memcpy(frame->normal,plane.normal,sizeof(ProVector));
    memcpy(frame->origin,plane.point,sizeof(ProVector));
    ProVector seed={1,0,0};
    if(fabs(frame->normal[0])>0.9){seed[0]=0;seed[1]=1;}
    double dot=ad_dot(seed,frame->normal);
    for(int i=0;i<3;i++)frame->u[i]=seed[i]-dot*frame->normal[i];
    if(!ad_unit(frame->u))return PRO_TK_BAD_CONTEXT;
    ad_cross(frame->normal,frame->u,frame->v);
    return PRO_TK_NO_ERROR;
}

static ProError smy_unbend_copy(ProMdl source,const wchar_t *fixed,ProFeattype type,ProMdl *copy,int *bend_count){
    *copy=NULL;
    *bend_count=0;
    ShBendInfo folds={0};
    folds.model=source;
    ProError error=ProSolidSurfaceVisit((ProSolid)source,sh_count_bends,NULL,&folds);
    if(!error)error=smx_copy_scratch(source,copy);
    if(error)return error;
    *bend_count=folds.count;
    if(!folds.inside)return PRO_TK_NO_ERROR;
    FdTxn tx={0};
    error=sh_begin(&tx,*copy,L"BWTL_MAP_UNBEND",type);
    if(!error)error=sh_flatten(&tx,fixed,0);
    if(!error)error=smx_verify_changed_feature(&tx);
    fd_cleanup(&tx,error);
    return error;
}

static ProError smy_map_mouth(ProMdl model,const wchar_t *plane,const wchar_t *bore,double scale,
    ProVector center,double *radius,int *owner){
    FdTxn tx={0};
    tx.model=model;tx.scale=scale;
    ShopMouth mouth={0};
    ProError error=mp_mouth(&tx,plane,bore,&mouth);
    FdRef ref={0};
    ProFeature source;
    if(!error)error=fd_ref_resolve(model,bore,PRO_SURFACE,&ref);
    if(!error)error=ProGeomitemFeatureGet(&ref.item,&source);
    if(!error){
        memcpy(center,mouth.frame.placement.csys.origin,sizeof(ProVector));
        *radius=mouth.radius;
        *owner=source.id;
    }
    fd_ref_free(&ref);
    return error;
}

static ProError smy_flat_hole_map(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    ProMdl copy=NULL;
    ShDxf *frame=calloc(1,sizeof(*frame));
    if(!frame)return PRO_TK_OUT_OF_MEMORY;
    int folds=0;
    wchar_t before[256],after[256];
    ProError error=smy_stamp(model,before);
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=smy_unbend_copy(model,cmd->arg[3],PRO_FEAT_UNBEND,&copy,&folds);
    if(!error)error=smy_projection_frame(copy,cmd->arg[3],frame);
    FILE *file=NULL;
    if(!error)error=fab_csv_open(work.stage,L"hole-folded-flat.csv",&file);
    if(!error)fwprintf(file,L"编号,折后板面,孔面,关联特征ID,折后X_mm,折后Y_mm,折后Z_mm,展开X_mm,展开Y_mm,孔径_mm\n");
    for(int i=4;!error&&i<cmd->count;i+=2){
        const wchar_t *face=cmd->arg[i],*hole=cmd->arg[i+1];
        ProVector folded,flat;
        double r1,r2,xy[2];
        int a,b;
        error=smy_map_mouth(model,face,hole,frame->scale,folded,&r1,&a);
        if(!error)error=smy_map_mouth(copy,face,hole,frame->scale,flat,&r2,&b);
        if(!error&&(a!=b||fabs(r1-r2)>0.0001))error=PRO_TK_BAD_CONTEXT;
        if(!error)error=sh_project(frame,flat,xy);
        if(error){
            part_text_add(out,L"%ls 未能按同一面编号、关联特征和孔径映射到展开；未猜测替代参照。\n",hole);
            break;
        }
        if(fwprintf(file,L"%d,%ls,%ls,%d,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g\n",
            (i-2)/2,face,hole,a,folded[0]/frame->scale,folded[1]/frame->scale,folded[2]/frame->scale,xy[0],xy[1],2*r1)<0)
            error=PRO_TK_CANT_WRITE;
    }
    if(copy){
        ProError erased=ProMdlErase(copy);
        if(!error)error=erased;
        if(erased)sh_recovery_uncertain=1;
    }
    if(!error)error=smy_stamp(model,after);
    if(!error&&wcscmp(before,after))error=PRO_TK_BAD_CONTEXT;
    free(frame);
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"已用独立副本回读 %d 个圆孔的折后 XYZ 和展开 XY，核对了关联特征和孔径。%ls\\hole-folded-flat.csv。\n",
        (cmd->count-4)/2,work.target);
    return error;
}

static void smy_flat_tangent(const ShDxfBend *bend,int side,double a[2],double b[2]){
    double x=bend->b[0]-bend->a[0],y=bend->b[1]-bend->a[1];
    double offset=side*bend->width/(2*hypot(x,y));
    a[0]=bend->a[0]-offset*y;a[1]=bend->a[1]+offset*x;
    b[0]=bend->b[0]-offset*y;b[1]=bend->b[1]+offset*x;
}

static ProError smy_bend_mark_file(const wchar_t *path,const ShDxf *frame){
    FILE *file=_wfopen(path,L"wt");
    if(!file)return PRO_TK_CANT_WRITE;
    fprintf(file,"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n"
        "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n4\n");
    const char *const layers[]={"BEND_CENTER","BEND_POS","BEND_NEG","BEND_TANGENT"};
    for(int i=0;i<_countof(layers);i++)fprintf(file,"0\nLAYER\n2\n%s\n70\n0\n62\n%d\n6\nCONTINUOUS\n",layers[i],i+1);
    fprintf(file,"0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n");
    for(int i=0;i<frame->bend_count;i++){
        const ShDxfBend *bend=&frame->bends[i];
        sh_dxf_line(file,layers[0],bend->a,bend->b);
        sh_dxf_line(file,layers[bend->direction>0?1:2],bend->a,bend->b);
        for(int side=-1;side<=1;side+=2){
            double a[2],b[2];
            smy_flat_tangent(bend,side,a,b);
            sh_dxf_line(file,layers[3],a,b);
        }
    }
    fprintf(file,"0\nENDSEC\n0\nEOF\n");
    return fab_close(file,PRO_TK_NO_ERROR);
}

static ProError smy_bend_marks_read(const wchar_t *path,int expected,int pos,int neg){
    FILE *file=_wfopen(path,L"rt");
    if(!file)return PRO_TK_CANT_OPEN;
    const char *const layers[]={"BEND_CENTER","BEND_POS","BEND_NEG","BEND_TANGENT"};
    char code[128],value[128];
    int counts[4]={0},entities=0,ended=0;
    while(fab_line_value(file,code,sizeof(code))==1&&fab_line_value(file,value,sizeof(value))==1){
        if(!strcmp(code,"2")&&!strcmp(value,"ENTITIES"))entities=1;
        if(!strcmp(code,"0")&&!strcmp(value,"EOF"))ended=1;
        if(entities&&!strcmp(code,"8"))for(int i=0;i<_countof(layers);i++)if(!strcmp(value,layers[i]))counts[i]++;
    }
    fclose(file);
    return ended&&counts[0]==expected&&counts[1]==pos&&counts[2]==neg&&counts[3]==expected*2?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError smy_flat_bend_layers(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    ShDxf *frame=calloc(1,sizeof(*frame));
    FabDxf *base=calloc(1,sizeof(*base));
    if(!frame||!base){free(frame);free(base);return PRO_TK_OUT_OF_MEMORY;}
    FabBends folded={0};
    ProError error=smy_bends_load(model,&folded);
    if(!error&&!folded.count)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    ProPath path,marks;
    if(!error)error=fab_flat_read(model,work.stage,L"base_probe",cmd->arg[3],base,path);
    ProMdl copy=NULL;
    int bends=0;
    if(!error)error=smy_unbend_copy(model,cmd->arg[3],PRO_FEAT_FLAT_PAT,&copy,&bends);
    if(!error)error=smy_projection_frame(copy,cmd->arg[3],frame);
    if(!error)error=ProSolidSurfaceVisit((ProSolid)copy,sh_dxf_surface,NULL,frame);
    if(!error&&(frame->bend_count!=base->bends||frame->bend_count!=folded.count||bends!=folded.count)){
        part_text_add(out,L"展开几何/下料图/折后折弯/折弯统计分别为 %d/%d/%d/%d。\n",
            frame->bend_count,base->bends,folded.count,bends);
        error=PRO_TK_BAD_CONTEXT;
    }
    int pos=0,neg=0;
    for(int i=0;!error&&i<folded.count;i++){
        const FabBend *row=&folded.rows[i];
        ProSurface original,unbent;
        ProGeomitem child;
        ProBool inside;
        error=ProSurfaceInit((ProSolid)model,row->surface,&original);
        if(!error)error=ProSmtBendsrfInfoGet((ProPart)model,original,NULL,&inside,NULL,NULL,NULL);
        if(!error)error=ProSurfaceInit((ProSolid)copy,row->surface,&original);
        if(!error)error=ProSmtBendsrfChildGet((ProPart)copy,original,&unbent);
        if(!error)error=ProSurfaceToGeomitem((ProSolid)copy,unbent,&child);
        if(error){
            part_text_add(out,L"Creo 未返回折弯面 SURFACE:%d 的活动展开子面（%d）。\n",row->surface,error);
            break;
        }
        ShDxfBend *mark=NULL;
        for(int j=0;j<frame->bend_count;j++)if(frame->bends[j].surface==child.id){mark=&frame->bends[j];break;}
        if(!mark||mark->direction||fabs(row->developed-mark->width)>0.001||
           fabs(row->length-hypot(mark->b[0]-mark->a[0],mark->b[1]-mark->a[1]))>0.001){
            part_text_add(out,L"折弯面 SURFACE:%d 对应展开子面 SURFACE:%d，身份或几何复核失败。\n",row->surface,child.id);
            error=PRO_TK_BAD_CONTEXT;
            break;
        }
        mark->direction=inside?1:-1;
        if(inside)pos++;else neg++;
    }
    if(copy){
        ProError erased=ProMdlErase(copy);
        if(!error)error=erased;
        if(erased)sh_recovery_uncertain=1;
    }
    if(!error&&!DeleteFileW(path))error=PRO_TK_CANT_WRITE;
    if(!error)error=fab_leaf_path(work.stage,L"cut",L".dxf",path);
    if(!error){
        int count=0;
        for(int i=0;i<base->count;i++)if(base->entities[i].layer!=3)base->entities[count++]=base->entities[i];
        base->count=count;
        error=smy_flat_write(path,base);
    }
    if(!error)error=fab_dxf_read(path,base);
    if(!error&&(base->bends||!base->outline))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=fab_leaf_path(work.stage,L"bend-layers",L".dxf",marks);
    if(!error)error=smy_bend_mark_file(marks,frame);
    if(!error)error=smy_bend_marks_read(marks,frame->bend_count,pos,neg);
    int count=frame->bend_count;
    free(frame);free(base);
    error=dv_stage_finish(&work,error,out);
    if(!error)part_text_add(out,L"已回读 %d 道折弯中线和 %d 条切线，按展开读取侧为弯内侧或外侧分为正向 %d、反向 %d。切割轮廓与折弯图层分别写入同目录，保持同一坐标：%ls。\n",
        count,count*2,pos,neg,work.target);
    else part_text_add(out,L"折弯面身份、展开弧长和几何必须一一匹配，当前分层未交付（Creo %d）。\n",error);
    return error;
}
