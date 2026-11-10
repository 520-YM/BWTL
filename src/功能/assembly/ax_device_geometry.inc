typedef struct {ProVector origin,axis[3];} AxDeviceFrame;
typedef struct {ProSurface handle;ProVector point,normal;} AxDevicePlane;

static ProError ax_device_pose(AxComponent *owner,ProMatrix pose){
    return ProAsmcomppathTrfGet(&owner->path,PRO_B_TRUE,pose);
}

static void ax_transform_frame(const AxDeviceFrame *from,ProMatrix pose,AxDeviceFrame *to){
    ProPntTrfEval((double *)from->origin,pose,to->origin);
    for(int i=0;i<3;i++)ProVectorTrfEval((double *)from->axis[i],pose,to->axis[i]);
}

static void ax_device_inverse(ProMatrix pose,ProMatrix inverse){
    memset(inverse,0,sizeof(ProMatrix));
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)inverse[i][j]=pose[j][i];
    for(int i=0;i<3;i++)inverse[3][i]=-ad_dot(pose[3],pose[i]);
    inverse[3][3]=1;
}

static ProError ax_device_frame(AxComponent *owner,const wchar_t *token,int world,AxDeviceFrame *frame){
    ProCsysdata data;
    ProSelection selected=NULL;
    ProError error=ax_csys_selection(owner->model,token,0,&selected,&data);
    if(selected)ProSelectionFree(&selected);
    if(error)return error;
    AxDeviceFrame local;
    memcpy(local.origin,data.origin,sizeof(local.origin));
    memcpy(local.axis[0],data.x_vector,sizeof(ProVector));
    memcpy(local.axis[1],data.y_vector,sizeof(ProVector));
    memcpy(local.axis[2],data.z_vector,sizeof(ProVector));
    if(world){
        ProMatrix pose;
        if((error=ax_device_pose(owner,pose)))return error;
        ax_transform_frame(&local,pose,frame);
    }else *frame=local;
    for(int i=0;i<3;i++)if(!ad_unit(frame->axis[i]))return PRO_TK_BAD_CONTEXT;
    ProVector cross;
    ad_cross(frame->axis[0],frame->axis[1],cross);
    return ad_dot(cross,frame->axis[2])>1-1e-7?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static void ax_device_align(const AxDeviceFrame *source,const AxDeviceFrame *destination,ProMatrix pose){
    memset(pose,0,sizeof(ProMatrix));
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)
        for(int k=0;k<3;k++)pose[i][j]+=source->axis[k][i]*destination->axis[k][j];
    ProVector translated;
    ProPntTrfEval((double *)source->origin,pose,translated);
    for(int i=0;i<3;i++)pose[3][i]=destination->origin[i]-translated[i];
    pose[3][3]=1;
}

static double ax_project(const double *point,const AxDeviceFrame *frame,int axis){
    ProVector delta;
    ad_sub(point,frame->origin,delta);
    return ad_dot(delta,frame->axis[axis]);
}

static ProError ax_device_face(AxComponent *owner,const wchar_t *token,AxDevicePlane *plane){
    FdRef ref={0};
    ProError error=fd_ref_resolve(owner->model,token,PRO_SURFACE,&ref);
    if(!error)error=ProGeomitemToSurface(&ref.item,&plane->handle);
    if(!error)error=fd_plane_data(&ref.item,plane->point,plane->normal);
    fd_ref_free(&ref);
    return error?error:ad_unit(plane->normal)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_device_edge(ProMdl model,const wchar_t *token,ProEdge *edge){
    FdRef ref={0};
    ProError error=fd_ref_resolve(model,token,PRO_EDGE,&ref);
    if(!error)error=ProGeomitemToEdge(&ref.item,edge);
    fd_ref_free(&ref);
    return error;
}

static ProError ax_device_plane_world(AxComponent *owner,const wchar_t *token,AxDevicePlane *plane){
    ProError error=ax_device_face(owner,token,plane);
    ProMatrix pose;
    if(!error)error=ax_device_pose(owner,pose);
    if(error)return error;
    AxDevicePlane local=*plane;
    if((error=ProPntTrfEval(local.point,pose,plane->point))||
       (error=ProVectorTrfEval(local.normal,pose,plane->normal)))return error;
    return ad_unit(plane->normal)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_surface_span(ProSurface surface,const AxDeviceFrame *frame,int axis,double *low,double *high){
    ProVector a,b;
    ProError error=ProSurfaceExtremesEval(surface,(double *)frame->axis[axis],a,b);
    if(error)return error;
    double u=ax_project(a,frame,axis),v=ax_project(b,frame,axis);
    if(!isfinite(u)||!isfinite(v))return PRO_TK_BAD_CONTEXT;
    *low=fmin(u,v);*high=fmax(u,v);
    return PRO_TK_NO_ERROR;
}

typedef struct {const AxDeviceFrame *frame;ProOutline box;int seen;} AxDeviceBounds;
static ProError ax_device_bound_surface(ProSurface surface,ProError status,ProAppData data){
    AxDeviceBounds *bound=data;
    if(status)return status;
    for(int i=0;i<3;i++){
        double low,high;
        ProError error=ax_surface_span(surface,bound->frame,i,&low,&high);
        if(error)return error;
        bound->box[0][i]=fmin(low,bound->box[0][i]);
        bound->box[1][i]=fmax(high,bound->box[1][i]);
    }
    bound->seen++;
    return PRO_TK_NO_ERROR;
}
static ProError ax_device_bounds(ProMdl model,const AxDeviceFrame *frame,ProOutline bounds){
    ProSolidBody body;
    AxDeviceBounds data={frame,{{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY}},0};
    ProError error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,ax_device_bound_surface,&data);
    if(error||!data.seen)return error?error:PRO_TK_E_NOT_FOUND;
    memcpy(bounds,data.box,sizeof(data.box));
    return PRO_TK_NO_ERROR;
}
static ProError ax_device_world_bounds(AxComponent *owner,const AxDeviceFrame *world,ProOutline bounds){
    ProMatrix pose,inverse;
    AxDeviceFrame local;
    ProError error=ax_device_pose(owner,pose);
    if(error)return error;
    ax_device_inverse(pose,inverse);
    ax_transform_frame(world,inverse,&local);
    return ax_device_bounds(owner->model,&local,bounds);
}

static ProError ax_device_fit(ProMdl assembly,AxBatchItem *item,AxComponent *other,
    const AxDevicePlane *separator,double mm,double *volume){
    ProIdTable id={item->feature.id};
    AxComponent placed={0};
    placed.model=item->model;
    ProSelection a=NULL,b=NULL;
    ProError error=ProAsmcomppathInit((ProSolid)assembly,id,1,&placed.path);
    /* Creo's interference calculation can report an error for coincident
       mounting faces. Prove separation of the actual solids first. */
    if(!error&&separator){
        AxDeviceFrame frame;
        ProOutline first,second;
        memcpy(frame.origin,separator->point,sizeof(ProVector));
        ax_hw_basis(separator->normal,frame.axis);
        if(!ax_device_world_bounds(&placed,&frame,first)&&!ax_device_world_bounds(other,&frame,second)&&
           (first[1][2]<=second[0][2]+1e-6/mm||second[1][2]<=first[0][2]+1e-6/mm)){
            *volume=0;
            return PRO_TK_NO_ERROR;
        }
    }
    if(!error)error=ax_part_selection(&placed,&a);
    if(!error)error=ax_part_selection(other,&b);
    if(!error)error=ax_interference_volume(a,b,volume);
    if(a)ProSelectionFree(&a);
    if(b)ProSelectionFree(&b);
    if(!error)*volume*=mm*mm*mm;
    return error;
}

static ProError ax_device_create(ProMdl assembly,AxBatchItem *items,int count,double mm,
    AxComponent *a,const AxDevicePlane *plane_a,AxComponent *b,const AxDevicePlane *plane_b,PartText *out){
    wchar_t receipt[1400]=L"";
    PartText saved={receipt,_countof(receipt),0};
    ProError error=ax_batch_create(assembly,items,count,mm,1,&saved);
    if(error){part_text_add(out,L"%ls",receipt);return error;}
    AxComponent *checks[]={a,b};
    const AxDevicePlane *planes[]={plane_a,plane_b};
    for(int i=0;!error&&i<count;i++)for(int j=0;!error&&j<2;j++){
        if(!checks[j])continue;
        double volume=0;
        error=ax_device_fit(assembly,&items[i],checks[j],planes[j],mm,&volume);
        if(error)part_text_add(out,L"Creo 未能完成指定接合件的实体干涉校验（错误 %d）。\n",error);
        if(!error&&volume>0.01){
            part_text_add(out,L"在指定接合件上检出 %.8g mm³ 实体干涉，已拒绝该定位。\n",volume);
            error=PRO_TK_BAD_CONTEXT;
        }
    }
    if(error){
        for(int i=count-1;i>=0;i--)ax_discard_occurrence(assembly,&items[i].feature,out);
        if(!ax_recovery_uncertain)part_text_add(out,L"本次临时实例已清理。\n");
    }else part_text_add(out,L"%ls",receipt);
    return error;
}
