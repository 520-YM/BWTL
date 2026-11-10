typedef struct {
    ProVector center;
    AxComponent window;
    double size,pitch_x,pitch_y,clearance;
    int nx,ny;
} AxFieldProfile;

static ProError ax_field_plane(ProMdl assembly,const wchar_t *token,ProVector point,ProVector normal){
    FdRef ref={0};
    ProError error=fd_ref_resolve(assembly,token,PRO_SURFACE,&ref);
    if(!error)error=fd_plane_data(&ref.item,point,normal);
    fd_ref_free(&ref);
    return error?error:ad_unit(normal)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_field_axis_center(ProMdl assembly,const wchar_t *plane,const double *origin,
    const double *axis,ProVector center){
    ProVector point,normal,delta;
    ProError error=ax_field_plane(assembly,plane,point,normal);
    if(error)return error;
    double alignment=ad_dot(normal,axis);
    if(fabs(alignment)<1-1e-7)return PRO_TK_BAD_CONTEXT;
    ad_sub(point,origin,delta);
    double distance=ad_dot(delta,normal)/alignment;
    for(int i=0;i<3;i++)center[i]=origin[i]+distance*axis[i];
    return PRO_TK_NO_ERROR;
}

static ProError ax_field_profile(ProSection section,ProMatrix pose,const AxFieldProfile *profile,double mm){
    AxDeviceFrame frame;
    PartCommand shape={0};
    int ids[AS_LIMIT],count=0,circles[16]={0};
    double x=0,y=0,w=0,h=0;
    ProError error;
#define AF(call) do{error=(call);if(error){trace("field profile " #call,error);return error;}}while(0)
    memcpy(frame.origin,pose[3],sizeof(ProVector));
    for(int i=0;i<3;i++)memcpy(frame.axis[i],pose[i],sizeof(ProVector));
    if(profile->window.model){
        AxComponent source=profile->window;
        ProOutline bounds;
        AF(ax_device_world_bounds(&source,&frame,bounds));
        double padding=profile->clearance/mm;
        x=bounds[0][0]-padding;
        y=bounds[0][1]-padding;
        w=bounds[1][0]-bounds[0][0]+2*padding;
        h=bounds[1][1]-bounds[0][1]+2*padding;
        if(w*mm<0.1||h*mm<0.1||w*mm>10000||h*mm>10000)return PRO_TK_BAD_CONTEXT;
        shape.count=5;
        wcscpy_s(shape.arg[0],PART_TOKEN_MAX,L"RECT");
        double args[]={x,y,w,h};
        for(int i=0;i<4;i++)swprintf_s(shape.arg[i+1],PART_TOKEN_MAX,L"%.15g",args[i]*mm);
        AF(fd_shape_append(section,&shape,0,1/mm,ids,&count));
    }else{
        if(fabs(ax_project(profile->center,&frame,2))*mm>0.001)return PRO_TK_BAD_INPUTS;
        x=ax_project(profile->center,&frame,0);
        y=ax_project(profile->center,&frame,1);
        shape.count=4;
        wcscpy_s(shape.arg[0],PART_TOKEN_MAX,L"CIRCLE");
        swprintf_s(shape.arg[3],PART_TOKEN_MAX,L"%.15g",0.5*profile->size);
        for(int j=0;j<profile->ny;j++)for(int i=0;i<profile->nx;i++){
            swprintf_s(shape.arg[1],PART_TOKEN_MAX,L"%.15g",x*mm+i*profile->pitch_x);
            swprintf_s(shape.arg[2],PART_TOKEN_MAX,L"%.15g",y*mm+j*profile->pitch_y);
            AF(fd_shape_append(section,&shape,0,1/mm,ids,&count));
            circles[j*profile->nx+i]=ids[0];
        }
    }
    AF(fd_section_solve(section,1/mm));
    if(!profile->window.model){
        for(int j=0;j<profile->ny;j++)for(int i=0;i<profile->nx;i++){
            Pro2dEntdef *entity=NULL;
            AF(ProSectionEntityGet(section,circles[j*profile->nx+i],&entity));
            Pro2dCircledef *circle=(Pro2dCircledef *)entity;
            if(!entity||entity->type!=PRO_2D_CIRCLE||
                fabs((circle->center[0]-x)*mm-i*profile->pitch_x)>0.001||
                fabs((circle->center[1]-y)*mm-j*profile->pitch_y)>0.001||
                fabs(circle->radius*mm-profile->size/2)>0.001)return PRO_TK_GENERAL_ERROR;
        }
    }
    return PRO_TK_NO_ERROR;
#undef AF
}

static ProError ax_field_sketch(ProMdl assembly,const wchar_t *name,const wchar_t *plane,
    const wchar_t *orient,const AxFieldProfile *profile,double mm){
    FdTxn tx={0};
    FdSketch sketch={0};
    ProMatrix pose;
    ProError error;
#define AF(call) do{error=(call);if(error){trace("field sketch " #call,error);goto done;}}while(0)
    AF(fd_begin(&tx,assembly,name,PRO_FEAT_CURVE));
    AF(fd_sketch_setup(&tx,plane,orient));
    AF(fd_create(&tx,1));
    sketch.feature=tx.feature;
    AF(ProFeatureElemtreeExtract(&sketch.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree));
    AF(fd_section_from_tree(sketch.tree,&sketch.section));
    AF(ProSectionLocationGet(sketch.section,pose));
    AF(ax_field_profile(sketch.section,pose,profile,mm));
    tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    AF(ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,tx.options,
        PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors));
    AF(fd_verify(&tx));
done:
    fd_sketch_free(&sketch);
    fd_cleanup(&tx,error);
    if(!tx.rollback_ok)ax_recovery_uncertain=1;
    return error;
#undef AF
}
