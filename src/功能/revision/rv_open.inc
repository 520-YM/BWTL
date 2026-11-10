static ProError rv_open_shape(ProMdl model,const wchar_t *name,int circle,double sizes[2],ProMatrix placement){
    FdSketch sketch={0};
    int *ids=NULL,count=0,sides=0;
    Pro2dLinedef lines[4];
    double lo[]={INFINITY,INFINITY},hi[]={-INFINITY,-INFINITY};
    ProError error=fd_sketch_open(model,name,&sketch,0);
    if(!error)error=ProSectionLocationGet(sketch.section,placement);
    if(!error)error=ProSectionEntityIdsGet(sketch.section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        Pro2dEntdef *entity=NULL;
        error=ProSectionEntityGet(sketch.section,ids[i],&entity);
        if(error)break;
        if(circle&&entity->type==PRO_2D_CIRCLE){
            if(sides++){error=PRO_TK_BAD_CONTEXT;break;}
            sizes[0]=2*((Pro2dCircledef *)entity)->radius;
        }else if(!circle&&entity->type==PRO_2D_LINE){
            if(sides>=4){error=PRO_TK_BAD_CONTEXT;break;}
            lines[sides++]=*(Pro2dLinedef *)entity;
            for(int j=0;j<2;j++){
                lo[j]=fmin(lo[j],fmin(lines[sides-1].end1[j],lines[sides-1].end2[j]));
                hi[j]=fmax(hi[j],fmax(lines[sides-1].end1[j],lines[sides-1].end2[j]));
            }
        }else {error=PRO_TK_BAD_CONTEXT;break;}
    }
    if(!error&&sides!=(circle?1:4))error=PRO_TK_BAD_CONTEXT;
    if(!error&&!circle){
        unsigned mask=0;
        for(int i=0;i<4;i++){
            const double *a=lines[i].end1,*b=lines[i].end2;
            int found=0;
            for(int j=0;j<2;j++){
                if(part_close(fmin(a[1-j],b[1-j]),lo[1-j])&&part_close(fmax(a[1-j],b[1-j]),hi[1-j])){
                    if(part_close(a[j],lo[j])&&part_close(b[j],lo[j]))found|=1<<(j*2);
                    if(part_close(a[j],hi[j])&&part_close(b[j],hi[j]))found|=1<<(j*2+1);
                }
            }
            mask|=found;
        }
        if(mask!=15)error=PRO_TK_BAD_CONTEXT;
        sizes[0]=hi[0]-lo[0];sizes[1]=hi[1]-lo[1];
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&sketch);
    return error;
}

typedef struct {double area,depth;const double *point,*normal;int found;} RevisionOpeningFloor;

static ProError rv_open_floor_surface(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    RevisionOpeningFloor *floor=data;
    ProVector point,normal,delta;
    if(fd_plane_data(item,point,normal)||!ad_unit(normal)||fabs(ad_dot(normal,floor->normal))<1-1e-6)
        return PRO_TK_NO_ERROR;
    ad_sub(point,floor->point,delta);
    if(!part_close(fabs(ad_dot(delta,floor->normal)),floor->depth))return PRO_TK_NO_ERROR;
    ProSurface surface;
    double area;
    ProError error=ProGeomitemToSurface(item,&surface);
    if(!error)error=ProSurfaceAreaEval(surface,&area);
    if(!error&&part_close(area,floor->area))floor->found++;
    return error;
}

static ProError rv_open_floor(ProFeature *cut,ProMatrix placement,double depth,int circle,const double sizes[2]){
    double area=circle?AS_PI*sizes[0]*sizes[0]/4:sizes[0]*sizes[1];
    RevisionOpeningFloor floor={area,depth,placement[3],placement[2],0};
    ProError error=ProFeatureGeomitemVisit(cut,PRO_SURFACE,rv_open_floor_surface,NULL,&floor);
    return error?error:floor.found==1?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError rv_open(Revision *tx,const PartCommand *cmd,PartText *out){
    ProFeature cut,sketch;
    ProElement root=NULL,depth;
    int *parents=NULL,n=0,linked=0;
    int circle=!wcscmp(cmd->arg[4],L"CIRCLE");
    ProMatrix placement;
    double blind_depth;
    double old[]={as_arg(cmd,6),circle?0:as_arg(cmd,9)},next[]={as_arg(cmd,7),circle?0:as_arg(cmd,10)},actual[2];
    ProError error=rv_feature(tx->model,cmd->arg[2],PRO_FEAT_CUT,&cut);
    if(!error)error=rv_feature(tx->model,cmd->arg[3],PRO_FEAT_CURVE,&sketch);
    if(!error)error=ProFeatureElemtreeExtract(&cut,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&root);
    if(!error)error=dv_depth_tree(root,&depth);
    if(!error)error=ProElementDoubleGet(depth,NULL,&blind_depth);
    if(root)ProFeatureElemtreeFree(&cut,root);
    if(!error)error=ProFeatureParentsGet(&cut,&parents,&n);
    for(int i=0;!error&&i<n;i++)if(parents[i]==sketch.id)linked=1;
    if(parents)ProArrayFree((ProArray *)&parents);
    if(error||!linked)return error?error:PRO_TK_BAD_CONTEXT;
    if((error=rv_open_shape(tx->model,cmd->arg[3],circle,actual,placement))||
       (error=rv_open_floor(&cut,placement,blind_depth,circle,actual)))return error;
    for(int i=0;i<(circle?1:2);i++){
        if(!part_close(actual[i]*tx->mm,old[i]))return PRO_TK_BAD_CONTEXT;
        if((error=rv_symbol(tx,cmd->arg[5+3*i],sketch.id,old[i],next[i],circle)))return error;
    }
    if((error=rv_apply(tx))||(error=rv_open_shape(tx->model,cmd->arg[3],circle,actual,placement))||
       (error=rv_open_floor(&cut,placement,blind_depth,circle,actual)))return error;
    for(int i=0;i<(circle?1:2);i++)if(!part_close(actual[i]*tx->mm,next[i]))return PRO_TK_BAD_CONTEXT;
    part_text_add(out,circle?L"已更新关联圆形开口，实测直径 %.8g mm。":L"已更新关联矩形开口，实测宽高 %.8g × %.8g mm。",next[0],next[1]);
    return PRO_TK_NO_ERROR;
}
