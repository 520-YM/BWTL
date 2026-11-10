typedef struct {ProCsysdata csys;double radius,zmin,zmax;int internal;} AmFrame;
static void am_world(const AmFrame *f,double x,double y,double z,ProVector out){
    for(int i=0;i<3;i++)out[i]=f->csys.origin[i]+x*f->csys.x_vector[i]+y*f->csys.y_vector[i]+z*f->csys.z_vector[i];
}
static ProError am_frame(FdTxn *tx,const PartCommand *cmd,AmFrame *frame){
    FdRef face={0},csys={0};ProSurface surface;ProCsys geometry;ProSrftype type;ProGeomitemdata *raw=NULL,*raw_csys=NULL;
    ProUvParam uv;ProVector p,n,delta,cross,radial;ProError e;ProQuilt quilt;
    if((e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&face))||
       (e=ProGeomitemToSurface(&face.item,&surface))||(e=ProSurfaceTypeGet(surface,&type)))goto done;
    if(type!=PRO_SRF_CYL){e=PRO_TK_BAD_INPUTS;goto done;}
    e=ProSurfaceQuiltGet((ProSolid)tx->model,surface,&quilt);if(e!=PRO_TK_E_NOT_FOUND){if(!e)e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=fd_ref_resolve(tx->model,cmd->arg[5],PRO_CSYS,&csys))||(e=ProGeomitemToCsys(&csys.item,&geometry))||
       (e=ProCsysDataGet(geometry,&raw_csys))||(e=ProSurfaceDataGet(surface,&raw)))goto done;
    frame->csys=*raw_csys->data.p_csys_data;
    {ProSurfacedata *s=raw->data.p_surface_data;ProCylinderdata *c=&s->srf_shape.cylinder;
     frame->radius=c->radius;
     ad_sub(frame->csys.origin,c->origin,delta);ad_cross(delta,c->e3,cross);
     if(sqrt(ad_dot(cross,cross))>0.001*tx->scale||fabs(fabs(ad_dot(frame->csys.z_vector,c->e3))-1)>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}
     uv[0]=(s->uv_min[0]+s->uv_max[0])/2;uv[1]=(s->uv_min[1]+s->uv_max[1])/2;
     if((e=ProSurfaceXyzdataEval(surface,uv,p,NULL,NULL,n)))goto done;
     ad_sub(p,c->origin,delta);for(int i=0;i<3;i++)radial[i]=delta[i]-ad_dot(delta,c->e3)*c->e3[i];
     frame->internal=ad_dot(radial,n)<0;
     if(frame->internal!=(!wcscmp(cmd->arg[3],L"HUB"))){e=PRO_TK_BAD_INPUTS;goto done;}
     for(int i=0;i<2;i++){
        uv[1]=i?s->uv_max[1]:s->uv_min[1];if((e=ProSurfaceXyzdataEval(surface,uv,p,NULL,NULL,NULL)))goto done;
        ad_sub(p,frame->csys.origin,delta);if(!i)frame->zmin=frame->zmax=ad_dot(delta,frame->csys.z_vector);
        else {double z=ad_dot(delta,frame->csys.z_vector);frame->zmin=fmin(frame->zmin,z);frame->zmax=fmax(frame->zmax,z);}
     }
    }
done:if(raw)ProGeomitemdataFree(&raw);if(raw_csys)ProGeomitemdataFree(&raw_csys);fd_ref_free(&face);fd_ref_free(&csys);return e;
}
static ProError am_sub(ProMdl model,const wchar_t *format,...){
    wchar_t text[1600],report[PART_TEXT_MAX];PartCommand command;va_list args;int k;
    va_start(args,format);k=vswprintf_s(text,_countof(text),format,args);va_end(args);
    if(k<0||part_command_parse(text,&command)!=1)return PRO_TK_BAD_INPUTS;
    if(ad_kind(command.arg[0]))return ad_execute(&command,model,report,_countof(report));
    if(!wcsncmp(command.arg[0],L"DATUM_",6))return fd_create_datum(&command,model,report,_countof(report));
    return sd_create(&command,model,report,_countof(report));
}
static ProError am_planes(FdTxn *tx,const PartCommand *cmd,double y,double z,int ring,ProName plane,ProName orient){
    ProMdlName model;ProName points[4];ProError e;
    if((e=ProMdlMdlnameGet(tx->model,model)))return e;
    swprintf_s(plane,PRO_NAME_SIZE,L"%.20ls_PLN",tx->name);swprintf_s(orient,PRO_NAME_SIZE,L"%.20ls_ORI",tx->name);
    for(int i=0;i<4;i++){
        swprintf_s(points[i],PRO_NAME_SIZE,L"%.20ls_P%d",tx->name,i);
        if((e=am_sub(tx->model,L"CREO_OP DATUM_POINT %ls %ls %ls %.15g %.15g %.15g",model,points[i],cmd->arg[5],i==1?1.0:0.0,y+(i==2?1:0),z+(i==3?1:0))))return e;
    }
    if((e=am_sub(tx->model,L"CREO_OP DATUM_PLANE_3POINT %ls %ls POINT:%ls POINT:%ls POINT:%ls",model,plane,points[0],points[1],points[ring?2:3])))return e;
    return am_sub(tx->model,L"CREO_OP DATUM_PLANE_3POINT %ls %ls POINT:%ls POINT:%ls POINT:%ls",model,orient,points[0],points[2],points[3]);
}
static ProError am_profile(FdTxn *outer,const PartCommand *cmd,const AmFrame *f,int ring,double y,double z,double width,double length,double inner,double outer_radius,ProName sketch,ProMatrix frame){
    FdTxn tx={0};FdSketch s={0};ProName plane,orient;wchar_t plane_token[64],orient_token[64];
    ProVector center,delta;double local[2]={0},along[2]={0};int ids[4];ProError e;
    if((e=am_planes(outer,cmd,y/outer->scale,z/outer->scale,ring,plane,orient)))return e;
    swprintf_s(sketch,PRO_NAME_SIZE,L"%.20ls_SK",outer->name);
    swprintf_s(plane_token,_countof(plane_token),L"PLANE:%ls",plane);swprintf_s(orient_token,_countof(orient_token),L"PLANE:%ls",orient);
#define AP(call) do{if((e=(call)))goto done;}while(0)
    AP(fd_begin(&tx,outer->model,sketch,PRO_FEAT_CURVE));AP(fd_sketch_setup(&tx,plane_token,orient_token));AP(fd_create(&tx,1));s.feature=tx.feature;
    AP(ProFeatureElemtreeExtract(&s.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&s.tree));AP(fd_section_from_tree(s.tree,&s.section));
    AP(ProSectionLocationGet(s.section,frame));
    am_world(f,0,y,ring?z:z+length/2,center);ad_sub(center,frame[3],delta);
    for(int i=0;i<2;i++){local[i]=ad_dot(delta,frame[i]);along[i]=ad_dot(f->csys.z_vector,frame[i]);}
    if(ring){AP(model_circle(s.section,local[0],local[1],inner,&ids[0]));AP(model_circle(s.section,local[0],local[1],outer_radius,&ids[1]));}
    else{
        PartCommand slot={0};
        double numbers[]={local[0],local[1],length,width,atan2(along[1],along[0])*180/AS_PI};
        for(int i=0;i<5;i++)swprintf_s(slot.arg[i],PART_TOKEN_MAX,L"%.15g",numbers[i]);
        AP(as_slot(s.section,&slot,0,1,ids));
    }
    AP(fd_section_solve(s.section,tx.scale));tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    AP(ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,tx.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors));
    AP(fd_verify(&tx));
done:fd_sketch_free(&s);fd_cleanup(&tx,e);return e;
#undef AP
}
