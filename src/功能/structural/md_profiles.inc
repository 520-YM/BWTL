static ProError md_frame(FdTxn *tx,const wchar_t *surface_token,const wchar_t *csys_token,int normal_axis,AmFrame *f){
    FdRef face={0},csys={0};ProCsys geometry;ProGeomitemdata *raw=NULL;ProVector p,n,d;ProSurface surface;ProUvParam uv;ProUvStatus inside;ProError e;
    if((e=md_solid_face(tx,surface_token,&face))||(e=fd_plane_data(&face.item,p,n))||
       (e=fd_ref_resolve(tx->model,csys_token,PRO_CSYS,&csys))||(e=ProGeomitemToCsys(&csys.item,&geometry))||
       (e=ProCsysDataGet(geometry,&raw)))goto done;
    f->csys=*raw->data.p_csys_data;ad_sub(f->csys.origin,p,d);
    if(fabs(ad_dot(d,n))>0.001*tx->scale||ad_dot(n,normal_axis?f->csys.z_vector:f->csys.x_vector)<1-1e-6){e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=ProGeomitemToSurface(&face.item,&surface))||(e=ProSurfaceParamEval((ProSolid)tx->model,surface,f->csys.origin,uv))||
       (e=ProSurfaceUvpntVerify((ProSolid)tx->model,surface,uv,&inside)))goto done;
    if(inside!=PRO_UV_INSIDE)e=PRO_TK_BAD_INPUTS;
done:if(raw)ProGeomitemdataFree(&raw);fd_ref_free(&face);fd_ref_free(&csys);return e;
}
/* Profiles use caller's local XYZ in millimetres and are mapped into the
   actual native sketch frame. Plane kind: 0=XY, 1=XZ, 2=YZ. */
static ProError md_profile(FdTxn *outer,const wchar_t *csys,const AmFrame *f,int plane_kind,double offset,const wchar_t *suffix,
    const double points[][3],int count,const double *radii,const wchar_t *support,ProName sketch,ProMatrix frame){
    FdTxn naming={0},tx={0};FdSketch s={0};PartCommand context={0};ProName plane,orient;wchar_t plane_token[64],orient_token[64];
    int id;ProError e;
    naming.model=outer->model;naming.scale=outer->scale;swprintf_s(naming.name,PRO_NAME_SIZE,L"%.16ls_%ls",outer->name,suffix);
    wcscpy_s(context.arg[5],PART_TOKEN_MAX,csys);
    if((e=am_planes(&naming,&context,plane_kind==1?offset:0,plane_kind==0?offset:0,plane_kind==0,plane,orient)))return e;
    swprintf_s(sketch,PRO_NAME_SIZE,L"%.22ls_SK",naming.name);
    swprintf_s(plane_token,_countof(plane_token),L"PLANE:%ls",plane_kind==2?orient:plane);
    swprintf_s(orient_token,_countof(orient_token),L"PLANE:%ls",plane_kind==2?plane:orient);
    if(support&&plane_kind==0&&offset==0)wcscpy_s(plane_token,_countof(plane_token),support);
#define PR(call) do{if((e=(call)))goto done;}while(0)
    PR(fd_begin(&tx,outer->model,sketch,PRO_FEAT_CURVE));PR(fd_sketch_setup(&tx,plane_token,orient_token));PR(fd_create(&tx,1));s.feature=tx.feature;
    PR(ProFeatureElemtreeExtract(&s.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&s.tree));PR(fd_section_from_tree(s.tree,&s.section));PR(ProSectionLocationGet(s.section,frame));
    if(radii){
        ProVector xyz,d;double center[2];am_world(f,0,0,offset*tx.scale,xyz);ad_sub(xyz,frame[3],d);
        center[0]=ad_dot(d,frame[0]);center[1]=ad_dot(d,frame[1]);
        for(int i=0;i<count;i++)PR(model_circle(s.section,center[0],center[1],radii[i]*tx.scale,&id));
    }else{
        AsPoint local[12];if(count<3||count>12){e=PRO_TK_BAD_INPUTS;goto done;}
        for(int i=0;i<count;i++){
            ProVector xyz,d;am_world(f,points[i][0]*tx.scale,points[i][1]*tx.scale,points[i][2]*tx.scale,xyz);ad_sub(xyz,frame[3],d);
            if(fabs(ad_dot(d,frame[2]))>0.001*tx.scale){e=PRO_TK_BAD_INPUTS;goto done;}
            local[i]=as_point(ad_dot(d,frame[0]),ad_dot(d,frame[1]));
        }
        for(int i=0;i<count;i++)PR(as_add_line(s.section,local[i],local[(i+1)%count],&id));
    }
    PR(fd_section_solve(s.section,tx.scale));tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    PR(ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,tx.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors));PR(fd_verify(&tx));
done:fd_sketch_free(&s);fd_cleanup(&tx,e);return e;
#undef PR
}
static ProError md_extrude(FdTxn *tx,const wchar_t *name,const wchar_t *sketch,const ProMatrix frame,const double *direction,double depth,int cut){
    ProMdlName model;ProError e=ProMdlMdlnameGet(tx->model,model);if(e)return e;
    /* Native POS extrudes opposite the reported sketch normal. */
    return am_sub(tx->model,L"CREO_OP SOLID_EXTRUDE %ls %ls %ls %ls BLIND %.15g %ls",model,name,sketch,cut?L"CUT":L"ADD",depth,ad_dot(frame[2],direction)<0?L"POS":L"NEG");
}
static ProError md_expected_volume(FdTxn *tx,double expected_delta){
    ProMassProperty mass;ProError e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);
    if(e)return e;double actual=(mass.volume-tx->volume)/pow(tx->scale,3);
    return fabs(actual-expected_delta)<=fmax(0.02,fabs(expected_delta)*0.0005)?0:PRO_TK_GENERAL_ERROR;
}
