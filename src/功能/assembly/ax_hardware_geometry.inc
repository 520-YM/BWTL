typedef struct {
    AxGeometry cylinder,plane;
    ProVector contact,end[2];
    int bore;
} AxHardwareSite;

static ProError ax_hw_cylinder(AxComponent *owner,const wchar_t *token,AxGeometry *shape,ProVector ends[2],int *bore){
    FdRef ref={0};
    ProSurface surface;
    ProGeomitemdata *raw=NULL;
    ProError error=fd_ref_resolve(owner->model,token,PRO_SURFACE,&ref);
    if(!error)error=ax_geometry(&ref.item,shape);
    if(!error&&shape->type!=AX_GEOM_CYLINDER)error=PRO_TK_INVALID_TYPE;
    if(!error)error=ProGeomitemToSurface(&ref.item,&surface);
    if(!error)error=ProSurfaceExtremesEval(surface,shape->direction,ends[0],ends[1]);
    if(!error)error=ProSurfaceDataGet(surface,&raw);
    if(error)goto done;
    for(int i=0;i<5;i++)for(int j=0;j<5;j++){
        ProVector point,normal,radial;
        error=qa_interior_sample(owner->model,surface,raw->data.p_surface_data,i,j,5,point,normal);
        if(error==PRO_TK_E_NOT_FOUND)continue;
        if(error)goto done;
        ad_sub(point,shape->point,radial);
        double axial=ad_dot(radial,shape->direction);
        for(int k=0;k<3;k++)radial[k]-=axial*shape->direction[k];
        if(!ad_unit(radial)||fabs(ad_dot(radial,normal))<0.99){error=PRO_TK_BAD_CONTEXT;goto done;}
        *bore=ad_dot(radial,normal)<0;
        goto done;
    }
    error=PRO_TK_INCOMPLETE;
done:
    if(raw)ProGeomitemdataFree(&raw);
    fd_ref_free(&ref);
    return error;
}

static ProError ax_hw_site(AxComponent *owner,const wchar_t *cylinder,const wchar_t *plane,double mm,AxHardwareSite *site){
    ProSelection face=NULL;
    ProError error=ax_hw_cylinder(owner,cylinder,&site->cylinder,site->end,&site->bore);
    if(!error)error=ax_ref_resolve(owner,plane,0,&face,&site->plane);
    if(face)ProSelectionFree(&face);
    if(error)return error;
    if(site->plane.type!=AX_GEOM_PLANE)return PRO_TK_INVALID_TYPE;
    double cos_angle=ad_dot(site->cylinder.direction,site->plane.direction);
    if(fabs(cos_angle)<1-1e-7)return PRO_TK_BAD_CONTEXT;
    ProVector relative;
    ad_sub(site->plane.point,site->cylinder.point,relative);
    double t=ad_dot(relative,site->plane.direction)/cos_angle;
    for(int i=0;i<3;i++)site->contact[i]=site->cylinder.point[i]+t*site->cylinder.direction[i];
    double interval[2];
    for(int i=0;i<2;i++){
        ad_sub(site->end[i],site->contact,relative);
        interval[i]=ad_dot(relative,site->cylinder.direction)*mm;
    }
    return fmin(interval[0],interval[1])<=0.001&&fmax(interval[0],interval[1])>=-0.001?
        PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_hw_world(AxComponent *owner,AxHardwareSite *site){
    ProMatrix pose;
    ProError error=ProAsmcomppathTrfGet(&owner->path,PRO_B_TRUE,pose);
    if(error)return error;
    AxHardwareSite local=*site;
    if((error=ProPntTrfEval(local.contact,pose,site->contact))||
       (error=ProVectorTrfEval(local.plane.direction,pose,site->plane.direction))||
       (error=ProVectorTrfEval(local.cylinder.direction,pose,site->cylinder.direction)))return error;
    for(int i=0;i<2;i++)if((error=ProPntTrfEval(local.end[i],pose,site->end[i])))return error;
    return ad_unit(site->plane.direction)&&ad_unit(site->cylinder.direction)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static void ax_hw_basis(const double *normal,ProVector basis[3]){
    ProVector seed={1,0,0};
    if(fabs(normal[0])>0.8){seed[0]=0;seed[1]=1;}
    ad_cross(seed,normal,basis[0]);
    ad_unit(basis[0]);
    ad_cross(normal,basis[0],basis[1]);
    memcpy(basis[2],normal,sizeof(ProVector));
}

static void ax_hw_seat_pose(const AxHardwareSite *part,const AxHardwareSite *seat,ProMatrix pose){
    ProVector local[3],world[3],normal;
    for(int i=0;i<3;i++)normal[i]=-seat->plane.direction[i];
    ax_hw_basis(part->plane.direction,local);
    ax_hw_basis(normal,world);
    memset(pose,0,sizeof(ProMatrix));
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)
        for(int k=0;k<3;k++)pose[i][j]+=local[k][i]*world[k][j];
    ProVector moved;
    ProPntTrfEval((double *)part->contact,pose,moved);
    for(int i=0;i<3;i++)pose[3][i]=seat->contact[i]-moved[i];
    pose[3][3]=1;
}

static ProError ax_hw_occurrence(ProMdl assembly,const wchar_t *token,double mm,AxComponent *owner){
    double actual;
    ProMdlType type;
    ProError error=ax_component_resolve(assembly,token,owner);
    if(!error)error=ProMdlTypeGet(owner->model,&type);
    if(!error)error=qa_mm_per_unit(owner->model,&actual);
    return error?error:type==PRO_MDL_PART&&part_close(mm,actual)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static double ax_hw_separation(const double *a,const double *b,const double *normal,double mm){
    ProVector delta,cross;
    ad_sub(a,b,delta);
    ad_cross(delta,normal,cross);
    return sqrt(ad_dot(cross,cross))*mm;
}

static ProError ax_hw_slab(AxComponent *base,const wchar_t *hole,const wchar_t *front,const wchar_t *back,
    double mm,AxHardwareSite *a,AxHardwareSite *b,double *thickness){
    ProError error=ax_hw_site(base,hole,front,mm,a);
    if(!error)error=ax_hw_site(base,hole,back,mm,b);
    if(!error)error=ax_hw_world(base,a);
    if(!error)error=ax_hw_world(base,b);
    if(error)return error;
    ProVector relative;
    ad_sub(a->contact,b->contact,relative);
    *thickness=ad_dot(relative,a->plane.direction)*mm;
    return a->bore&&b->bore&&ad_dot(a->plane.direction,b->plane.direction)<-1+1e-7&&
        ax_hw_separation(a->contact,b->contact,a->plane.direction,mm)<0.001&&*thickness>0.001?
        PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static void ax_hw_axial_span(const AxHardwareSite *part,const double *origin,const double *direction,
    double mm,double *start,double *end){
    double t[2];
    for(int i=0;i<2;i++){
        ProVector relative;
        ad_sub(part->end[i],origin,relative);
        t[i]=ad_dot(relative,direction)*mm;
    }
    *start=fmin(t[0],t[1]);*end=fmax(t[0],t[1]);
}
