
static int smx_segments_intersect(AsPoint a,AsPoint b,AsPoint c,AsPoint d){
    AsPoint v=as_minus(b,a),w=as_minus(d,c),delta=as_minus(c,a);
    double denom=as_cross2(v,w);
    if(fabs(denom)<1e-12)return fx_segment_distance(a,c,d)<0.00001||fx_segment_distance(b,c,d)<0.00001||
        fx_segment_distance(c,a,b)<0.00001||fx_segment_distance(d,a,b)<0.00001;
    double t=as_cross2(delta,w)/denom,u=as_cross2(delta,v)/denom;
    return t>=-1e-9&&t<=1+1e-9&&u>=-1e-9&&u<=1+1e-9;
}

static double smx_segment_gap(AsPoint a,AsPoint b,AsPoint c,AsPoint d){
    return smx_segments_intersect(a,b,c,d)?0:fmin(fmin(fx_segment_distance(a,c,d),fx_segment_distance(b,c,d)),
        fmin(fx_segment_distance(c,a,b),fx_segment_distance(d,a,b)));
}

static int smx_cut_crosses_bend(const FabEntity *cut,const FabEntity *bend){
    AsPoint a=as_point(bend->a[0],bend->a[1]),b=as_point(bend->b[0],bend->b[1]);
    if(cut->type==1)return smx_segments_intersect(a,b,as_point(cut->a[0],cut->a[1]),as_point(cut->b[0],cut->b[1]));
    AsPoint center=as_point(cut->center[0],cut->center[1]),v=as_minus(b,a),d=as_minus(a,center);
    double aa=as_dot2(v,v),bb=2*as_dot2(v,d),cc=as_dot2(d,d)-cut->radius*cut->radius;
    double disc=bb*bb-4*aa*cc;
    if(disc<0||aa<1e-12)return 0;
    for(int sign=-1;sign<=1;sign+=2){
        double t=(-bb+sign*sqrt(disc))/(2*aa);
        if(t<0||t>1)continue;
        if(cut->type==3)return 1;
        AsPoint p=as_plus(d,as_mul(v,t));
        double angle=fab_degrees(atan2(p.y,p.x)*180/AS_PI-cut->start);
        if(angle<=cut->sweep+1e-7)return 1;
    }
    return 0;
}

static AsPoint smx_xy(const double point[2]){return as_point(point[0],point[1]);}

static int smx_arc_contains(const FabEntity *arc,AsPoint direction){
    return arc->type==3||fab_degrees(atan2(direction.y,direction.x)*180/AS_PI-arc->start)<=arc->sweep+1e-7;
}

static double smx_point_curve(AsPoint point,const FabEntity *curve){
    if(curve->type==1)return fx_segment_distance(point,smx_xy(curve->a),smx_xy(curve->b));
    AsPoint radial=as_minus(point,smx_xy(curve->center));
    double distance=hypot(radial.x,radial.y);
    if(distance<FAB_JOIN||smx_arc_contains(curve,radial))return fabs(distance-curve->radius);
    AsPoint a=as_minus(point,smx_xy(curve->a)),b=as_minus(point,smx_xy(curve->b));
    return fmin(hypot(a.x,a.y),hypot(b.x,b.y));
}

static double smx_curve_gap(const FabEntity *a,const FabEntity *b){
    if(a->type==1&&b->type==1)return smx_segment_gap(smx_xy(a->a),smx_xy(a->b),smx_xy(b->a),smx_xy(b->b));
    if(a->type==1)return smx_curve_gap(b,a);
    double nearest=INFINITY;
    if(a->type!=3)nearest=fmin(smx_point_curve(smx_xy(a->a),b),smx_point_curve(smx_xy(a->b),b));
    if(b->type!=3)nearest=fmin(nearest,fmin(smx_point_curve(smx_xy(b->a),a),smx_point_curve(smx_xy(b->b),a)));
    AsPoint center=smx_xy(a->center);
    if(b->type==1){
        if(smx_cut_crosses_bend(a,b))return 0;
        AsPoint start=smx_xy(b->a),dir=as_minus(smx_xy(b->b),start);
        double len=hypot(dir.x,dir.y);
        dir=as_mul(dir,1/len);
        AsPoint normal=as_left(dir);
        for(int i=-1;i<=1;i+=2){
            AsPoint radial=as_mul(normal,i);
            if(!smx_arc_contains(a,radial))continue;
            AsPoint p=as_plus(center,as_mul(radial,a->radius));
            nearest=fmin(nearest,smx_point_curve(p,b));
        }
        return nearest;
    }
    AsPoint second=smx_xy(b->center),axis=as_minus(second,center);
    double d=hypot(axis.x,axis.y);
    if(d<FAB_JOIN)return (a->type==3||b->type==3)?fabs(a->radius-b->radius):nearest;
    axis=as_mul(axis,1/d);
    double x=(a->radius*a->radius-b->radius*b->radius+d*d)/(2*d);
    double height_sq=a->radius*a->radius-x*x;
    if(height_sq>=-FAB_JOIN*FAB_JOIN){
        double h=sqrt(fmax(0,height_sq));
        for(int i=-1;i<=1;i+=2){
            AsPoint from_a=as_plus(as_mul(axis,x),as_mul(as_left(axis),i*h));
            if(smx_arc_contains(a,from_a)&&smx_arc_contains(b,as_minus(from_a,as_mul(axis,d))))return 0;
        }
    }
    for(int i=-1;i<=1;i+=2)for(int j=-1;j<=1;j+=2){
        AsPoint left=as_mul(axis,i),right=as_mul(axis,j);
        if(smx_arc_contains(a,left)&&smx_arc_contains(b,right))
            nearest=fmin(nearest,fabs(i*a->radius-d-j*b->radius));
    }
    return nearest;
}

static AsPoint smx_curve_tangent(const FabEntity *curve,int end){
    const double *points[]={curve->a,curve->b};
    if(curve->type==1)return as_minus(smx_xy(points[1-end]),smx_xy(points[end]));
    AsPoint radial=as_minus(smx_xy(points[end]),smx_xy(curve->center));
    return as_mul(as_left(radial),end?-1:1);
}

typedef struct { ProMdl model; ProSurface surface; PartText *out; double mm; int count; } SmxFoldedHoles;

static ProError smx_folded_cut_edge(ProEdge edge,ProError status,ProAppData context){
    if(status)return status;
    SmxFoldedHoles *read=context;
    ProSurface nearby[2];
    ProError error=ProEdgeNeighborsGet(edge,NULL,NULL,&nearby[0],&nearby[1]);
    if(error)return error;
    for(int i=0;i<2;i++){
        ProGeomitem item;
        ProFeature feature;
        ProFeattype type;
        if(nearby[i]==read->surface)continue;
        error=ProSurfaceToGeomitem((ProSolid)read->model,nearby[i],&item);
        if(!error)error=ProGeomitemFeatureGet(&item,&feature);
        if(!error)error=ProFeatureTypeGet(&feature,&type);
        if(error)return error;
        if(type!=PRO_FEAT_CUT_SMT)continue;
        int id;
        double length;
        ProVector midpoint,d1,d2,d3;
        if((error=ProEdgeIdGet(edge,&id))||(error=ProEdgeLengthEval(edge,&length))||
           (error=ProEdgeXyzdataEval(edge,0.5,midpoint,d1,d2,d3)))return error;
        if(++read->count<=30)part_text_add(read->out,L"折后切除特征 %d 与弯面相邻的边 EDGE:%d 长 %.8g mm，中点 (%.8g, %.8g, %.8g) mm。\n",
            feature.id,id,length*read->mm,midpoint[0]*read->mm,midpoint[1]*read->mm,midpoint[2]*read->mm);
    }
    return PRO_TK_NO_ERROR;
}

static ProError smx_folded_cut_contour(ProContour contour,ProError status,ProAppData context){
    if(status)return status;
    SmxFoldedHoles *read=context;
    return ProContourEdgeVisit(read->surface,contour,smx_folded_cut_edge,NULL,context);
}

static ProError smx_folded_cut_surface(ProSurface surface,ProError status,ProAppData context){
    if(status)return status;
    SmxFoldedHoles *read=context;
    ProSmtSurfType role;
    double angle;
    if(ProSmtSurfaceTypeGet((ProPart)read->model,surface,&role)||role!=PRO_SMT_SURF_FACE||
       ProSmtBendsrfInfoGet((ProPart)read->model,surface,NULL,NULL,&angle,NULL,NULL)||fabs(angle)<0.01)return PRO_TK_NO_ERROR;
    read->surface=surface;
    return ProSurfaceContourVisit(surface,smx_folded_cut_contour,NULL,context);
}

static ProError smx_flat_openings(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    DeliveryOutput work={0};
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d)return PRO_TK_OUT_OF_MEMORY;
    ProPath path;
    ProError error=PRO_TK_NO_ERROR;
    int folded=0;
    if(kind==SMX_CROSS_BEND){
        SmxFoldedHoles read={model,NULL,out,0,0};
        error=qa_mm_per_unit(model,&read.mm);
        if(!error)error=ProSolidSurfaceVisit((ProSolid)model,smx_folded_cut_surface,NULL,&read);
        if(!error){
            folded=read.count;
            part_text_add(out,L"折后模型找到 %d 处与原生钣金切除相邻的折弯边。\n",folded);
        }
    }
    if(!error)error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_flat_read(model,work.stage,L"holes_flat",cmd->arg[2],d,path);
    error=fab_discard(&work,error,out);
    if(error){
        if(kind==SMX_CROSS_BEND)part_text_add(out,L"折后信息已单独记录；展开轮廓仍需核实（Creo %d）。\n",error);
        free(d);
        return error;
    }
    if(kind==SMX_CROSS_BEND){
        int crossed=0;
        for(int i=0;i<d->count;i++)if(d->entities[i].layer==2){
            for(int j=0;j<d->count;j++)if(d->entities[j].layer==3&&smx_cut_crosses_bend(&d->entities[i],&d->entities[j])){
                crossed++;
                if(crossed<=30)part_text_add(out,L"展开孔槽图元 #%d 与折弯线 #%d 相交。折弯线端点 (%.8g,%.8g) → (%.8g,%.8g) mm。\n",
                    i+1,j+1,d->entities[j].a[0],d->entities[j].a[1],d->entities[j].b[0],d->entities[j].b[1]);
            }
        }
        part_text_add(out,L"展开回读有 %d 个内轮廓图元、%d 条折弯线、%d 对相交。轮廓闭合及重复项检查通过。\n",
            d->holes,d->bends,crossed);
        if(folded||crossed)part_text_add(out,L"折后统计按实际折弯边，展开统计按图元与折弯线，两个数量口径不同。\n");
    }else{
        double short_limit=as_arg(cmd,3),width_limit=as_arg(cmd,4),sharp=as_arg(cmd,5);
        int short_count=0,angle_count=0,narrow_count=0;
        for(int i=0;i<d->count;i++){
            const FabEntity *a=&d->entities[i];
            if(a->layer==3)continue;
            double length=a->type==1?hypot(a->a[0]-a->b[0],a->a[1]-a->b[1]):
                a->radius*AS_PI/180*(a->type==3?360:a->sweep);
            if(length<short_limit){
                short_count++;
                if(short_count<=24)part_text_add(out,L"图元 #%d 长 %.8g mm，小于 %.8g mm。\n",i+1,length,short_limit);
            }
            for(int j=i+1;j<d->count;j++){
                const FabEntity *b=&d->entities[j];
                if(b->layer==3)continue;
                const double *a_pt[]={a->a,a->b},*b_pt[]={b->a,b->b};
                int joined=0;
                for(int x=0;a->type!=3&&b->type!=3&&x<2;x++)for(int y=0;y<2;y++)if(fab_close_xy(a_pt[x],b_pt[y],FAB_JOIN)){
                    joined=1;
                    AsPoint v=smx_curve_tangent(a,x),w=smx_curve_tangent(b,y);
                    double angle=acos(qa_clamp(as_dot2(v,w)/(hypot(v.x,v.y)*hypot(w.x,w.y))))*180/AS_PI;
                    if(angle<sharp){
                        angle_count++;
                        if(angle_count<=24)part_text_add(out,L"图元 #%d/#%d 尖角 %.8g°，低于 %.8g°。\n",i+1,j+1,angle,sharp);
                    }
                }
                if(joined)continue;
                double gap=smx_curve_gap(a,b);
                if(gap+1e-8<width_limit){
                    narrow_count++;
                    if(narrow_count<=24)part_text_add(out,L"图元 #%d/#%d 最近距离 %.8g mm，低于 %.8g mm。\n",i+1,j+1,gap,width_limit);
                }
            }
        }
        part_text_add(out,L"下料预检（直线、圆弧和整圆）：短图元 %d、端点切向尖角 %d、非相邻图元窄距 %d。\n",
            short_count,angle_count,narrow_count);
    }
    free(d);
    return PRO_TK_NO_ERROR;
}

typedef struct { int id; Pro2dCircledef original; } SmxCircleChange;

static ProError smx_edit_circles(ProMdl model,const PartCommand *cmd,PartText *out){
    ProFeature sketch,cut;
    ProFeattype type;
    ProElement original=NULL,edit=NULL;
    ProSection section;
    ProMatrix frame;
    double old=as_arg(cmd,4),next=as_arg(cmd,5),mm,thickness;
    int *ids=NULL,n=0,count=0,wanted;
    int attempted=0;
    SmxCircleChange changes[SMX_MAX_OPENINGS];
    ProMassProperty before,after;
    part_integer(cmd->arg[6],&wanted);
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=ux_named_feature(model,cmd->arg[2],&sketch);
    if(!error)error=ux_named_feature(model,cmd->arg[3],&cut);
    if(!error)error=ProFeatureTypeGet(&cut,&type);
    if(!error&&type!=PRO_FEAT_CUT_SMT)error=PRO_TK_BAD_CONTEXT;
    if(!error){
        int *parents=NULL,length=0,found=0;
        error=ProFeatureParentsGet(&cut,&parents,&length);
        for(int i=0;!error&&i<length;i++)found|=parents[i]==sketch.id;
        if(parents)ProArrayFree((ProArray *)&parents);
        if(!error&&!found)error=PRO_TK_BAD_CONTEXT;
    }
    if(!error)error=qa_geometry_mass(model,&before);
    if(!error)error=ProFeatureElemtreeExtract(&sketch,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&original);
    if(!error)error=ProFeatureElemtreeExtract(&sketch,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&edit);
    if(!error)error=fd_section_from_tree(edit,&section);
    if(!error)error=ProSectionLocationGet(section,frame);
    if(!error)error=ProSectionEntityIdsGet(section,&ids,&n);
    for(int i=0;!error&&i<n;i++){
        Pro2dEntdef *item=NULL;
        error=ProSectionEntityGet(section,ids[i],&item);
        if(error||!item||item->type!=PRO_2D_CIRCLE)continue;
        Pro2dCircledef circle=*(Pro2dCircledef *)item;
        if(fabs(circle.radius*mm*2-old)>0.0001)continue;
        if(count==_countof(changes)){error=PRO_TK_OUT_OF_RANGE;break;}
        changes[count++]=(SmxCircleChange){ids[i],circle};
    }
    if(!error&&count!=wanted)error=PRO_TK_BAD_CONTEXT;
    for(int i=0;!error&&i<count;i++){
        Pro2dCircledef change=changes[i].original;
        change.radius=next/(2*mm);
        int replacement;
        error=ProSectionEntityAdd(section,(Pro2dEntdef *)&change,&replacement);
        if(!error)error=ProSectionEntityReplace(section,changes[i].id,replacement);
    }
    if(!error)error=fd_section_solve(section,1/mm);
    if(!error){attempted=1;error=cm_apply_tree(&sketch,edit);trace("SMX hole sketch redefine",error);}
    if(!error)error=qa_geometry_mass(model,&after);
    double volume_change=AS_PI*(old*old-next*next)/4*thickness*count/(mm*mm*mm);
    if(!error&&!part_close(after.volume-before.volume,volume_change)){
        part_text_add(out,L"孔组回读去料变化 %.8g mm³，预期 %.8g mm³。\n",(after.volume-before.volume)*mm*mm*mm,volume_change*mm*mm*mm);
        error=PRO_TK_GENERAL_ERROR;
    }
    if(!error){
        SmxGeometry *geometry=calloc(1,sizeof(*geometry));
        if(!geometry)error=PRO_TK_OUT_OF_MEMORY;
        if(!error)error=smx_geometry(model,geometry);
        for(int i=0;!error&&i<count;i++){
            ProVector center,point={changes[i].original.center[0],changes[i].original.center[1],0};
            error=ProPntTrfEval(point,frame,center);
            int found=0;
            for(int j=0;!error&&j<geometry->count;j++){
                const SmxFace *face=&geometry->faces[j];
                if(face->type!=PRO_SRF_CYL||fabs(face->radius*2-next)>0.0002)continue;
                ProVector delta,cross;
                ad_sub(center,face->point,delta);
                ad_cross(delta,face->axis,cross);
                if(sqrt(ad_dot(cross,cross))*mm<0.0002)found++;
            }
            if(!error&&!found)error=PRO_TK_GENERAL_ERROR;
        }
        free(geometry);
    }
    if(error&&attempted){
        if(cm_apply_tree(&sketch,original)||qa_geometry_mass(model,&after)||!part_close(after.volume,before.volume)){
            sh_recovery_uncertain=1;
            part_text_add(out,L"草绘恢复需核对，已暂停钣金写入。\n");
        }else part_text_add(out,L"孔组尺寸修改未通过，原草绘和去料体积已恢复。\n");
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(original)ProFeatureElemtreeFree(&sketch,original);
    if(edit)ProFeatureElemtreeFree(&sketch,edit);
    if(!error)part_text_add(out,L"关联切除 %ls 的 %d 个圆孔由 %.8g 改为 %.8g mm，圆柱面、中心及去料体积已回读，未保存。\n",
        cmd->arg[3],count,old,next);
    return error;
}
