#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"

typedef struct {ProMdl model;wchar_t token[PRO_NAME_SIZE+8];ProSrftype wanted;int surface;ProVector on_surface;} DatumFixture;

static ProError find_csys(ProCsys csys,ProError status,ProAppData data){
    DatumFixture *fixture=data;
    ProGeomitem item;
    ProName name;
    ProError e;
    if(status)return status;
    if((e=ProCsysToGeomitem((ProSolid)fixture->model,csys,&item))||(e=ProModelitemNameGet(&item,name)))return e;
    swprintf_s(fixture->token,_countof(fixture->token),L"CSYS:%ls",name);
    return PRO_TK_E_FOUND;
}

static ProError find_surface(ProSurface surface,ProError status,ProAppData data){
    DatumFixture *fixture=data;
    ProGeomitemdata *raw=NULL;
    ProSurfacedata *s;
    ProUvParam uv;
    ProVector first[2],second[3],normal;
    ProSrftype type;
    ProError e;
    if(status)return status;
    if((e=ProSurfaceTypeGet(surface,&type)))return e;
    if(type!=fixture->wanted)return PRO_TK_NO_ERROR;
    if((e=ProSurfaceIdGet(surface,&fixture->surface))||(e=ProSurfaceDataGet(surface,&raw)))return e;
    s=raw->data.p_surface_data;
    uv[0]=(s->uv_min[0]+s->uv_max[0])/2;uv[1]=(s->uv_min[1]+s->uv_max[1])/2;
    e=ProSurfaceXyzdataEval(surface,uv,fixture->on_surface,first,second,normal);
    ProGeomitemdataFree(&raw);
    return e?e:PRO_TK_E_FOUND;
}

static ProError model_to_local(DatumFixture *fixture,ProVector local){
    FdRef ref={0};
    ProCsys csys;
    ProGeomitemdata *raw=NULL;
    ProCsysdata *frame;
    ProVector difference;
    ProError e;
    if((e=fd_ref_resolve(fixture->model,fixture->token,PRO_CSYS,&ref))||(e=ProGeomitemToCsys(&ref.item,&csys))||
       (e=ProCsysDataGet(csys,&raw)))goto done;
    frame=raw->data.p_csys_data;
    ad_sub(fixture->on_surface,frame->origin,difference);
    local[0]=ad_dot(difference,frame->x_vector);local[1]=ad_dot(difference,frame->y_vector);local[2]=ad_dot(difference,frame->z_vector);
done:
    if(raw)ProGeomitemdataFree(&raw);
    fd_ref_free(&ref);
    return e;
}

static ProError first_dimension(ProDimension *dimension,ProError status,ProAppData data){
    if(status)return status;
    *(ProDimension *)data=*dimension;
    return PRO_TK_E_FOUND;
}

static ProError offset_plane_again(ProMdl model,wchar_t *name,double distance){
    ProModelitem feature;
    ProDimension dim;
    ProError e;
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,name,&feature)))return e;
    e=ProFeatureDimensionVisit((ProFeature *)&feature,first_dimension,NULL,&dim);
    if(e!=PRO_TK_E_FOUND)return e?e:PRO_TK_E_NOT_FOUND;
    if((e=ProDimensionValueSet(&dim,distance)))return e;
    return part_regenerate(model);
}

static ProError verify_changed_references(ProMdl model,int cylinder){
    FdRef source[3]={{0}},moved_plane={0};
    FdTxn tx={0};
    AdGeometry expected={0};
    wchar_t surface[32];
    ProVector point,normal;
    ProError e;
    tx.model=model;tx.scale=1;
    if((e=offset_plane_again(model,L"END_X",36))||
       (e=offset_plane_again(model,L"MID_Z",14))||
       (e=fd_ref_resolve(model,L"PLANE:MID_X",PRO_SURFACE,&moved_plane))||
       (e=fd_plane_data(&moved_plane.item,point,normal)))goto done;
    if(fabs(point[0]-18)>1e-5){e=PRO_TK_GENERAL_ERROR;goto done;}
    expected.point[0]=18;expected.point[2]=14;
    expected.normal[0]=-1;expected.axis_y[1]=-1;
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,L"SYMMETRY_NEG",(ProModelitem *)&tx.feature))||
       (e=ad_verify_geometry(&tx,AD_SYM_CSYS,PRO_CSYS,source,&expected)))goto done;
    swprintf_s(surface,_countof(surface),L"SURFACE:%d",cylinder);
    if((e=fd_ref_resolve(model,surface,PRO_SURFACE,&source[0]))||
       (e=fd_ref_resolve(model,L"PLANE:MID_Z",PRO_SURFACE,&source[1]))||
       (e=ProModelitemByNameInit(model,PRO_FEATURE,L"INTERSECTION",(ProModelitem *)&tx.feature)))goto done;
    e=ad_verify_intersection(&tx,source);
done:
    fd_ref_free(&moved_plane);
    fd_ref_free(&source[0]);
    fd_ref_free(&source[1]);
    return e;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ModelPlan plan={1,40,40,20,0,1};
    DatumFixture f={0};
    Fixture block;
    ProMdl model=NULL;
    ProMdlName name;
    const char *stage="fixture cylinder";
    wchar_t command[1500];
    char marker[16];
    ProVector local;
    ProError e;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    model_background_mode=1;
    if((e=model_create(&plan,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){trace(stage,e);failures++;goto done;}
    f.model=model;f.wanted=PRO_SRF_CYL;
    if((e=ProSolidCsysVisit((ProSolid)model,find_csys,NULL,&f))!=PRO_TK_E_FOUND||
       (e=ProSolidSurfaceVisit((ProSolid)model,find_surface,NULL,&f))!=PRO_TK_E_FOUND||
       (e=model_to_local(&f,local))){trace("get physical cylinder and coordinate system",e);failures++;goto done;}
#define CASE(reject,format,...) do{swprintf_s(command,_countof(command),L"CREO_OP " format,__VA_ARGS__);run(model,command,reject);}while(0)
#define POINT(label,x,y,z) CASE(0,L"DATUM_POINT %ls %ls %ls %.9f %.9f %.9f",name,label,f.token,(double)(x),(double)(y),(double)(z))
    POINT(L"P_ZERO",0,0,0);POINT(L"P_X",20,0,0);POINT(L"P_YZ",0,20,8);POINT(L"P_COL",10,0,0);
    CASE(0,L"DATUM_AXIS_2POINT %ls POINT_AXIS POINT:P_ZERO POINT:P_X",name);
    CASE(1,L"DATUM_AXIS_2POINT %ls BAD_AXIS POINT:P_X POINT:UNDEFINED",name);
    CASE(0,L"DATUM_PLANE_3POINT %ls THREE POINT:P_ZERO POINT:P_X POINT:P_YZ",name);
    CASE(1,L"DATUM_PLANE_3POINT %ls COLLINEAR POINT:P_ZERO POINT:P_X POINT:P_COL",name);
    CASE(0,L"DATUM_AXIS %ls TILT_AXIS PLANES PLANE:RIGHT PLANE:TOP",name);
    CASE(0,L"DATUM_PLANE_ANGLE %ls ANGLED PLANE:RIGHT AXIS:TILT_AXIS 35",name);
    CASE(0,L"DATUM_PLANE_ANGLE %ls ANGLED_BACK PLANE:RIGHT AXIS:TILT_AXIS -25",name);
    CASE(1,L"DATUM_PLANE_ANGLE %ls BAD_ANGLE PLANE:FRONT AXIS:TILT_AXIS 35",name);
    CASE(0,L"DATUM_PLANE %ls END_X PLANE:RIGHT 20",name);
    CASE(0,L"DATUM_PLANE_MID %ls MID_X PLANE:RIGHT PLANE:END_X",name);
    CASE(1,L"DATUM_PLANE_MID %ls SKEW_MID PLANE:RIGHT PLANE:TOP",name);
    POINT(L"P_TOUCH",local[0],local[1],local[2]);
    CASE(0,L"DATUM_PLANE_TANGENT %ls TANGENT SURFACE:%d POINT:P_TOUCH",name,f.surface);
    CASE(1,L"DATUM_PLANE_TANGENT %ls MISSED_TANGENT SURFACE:%d POINT:P_COL",name,f.surface);
    CASE(0,L"DATUM_CURVE %ls LINE_40 %ls 0 0 0 40 0 0",name,f.token);
    CASE(0,L"DATUM_POINT_CURVE %ls PERCENT_25 CURVE:LINE_40 PERCENT 25",name);
    CASE(0,L"DATUM_POINT_CURVE %ls LENGTH_12 CURVE:LINE_40 LENGTH 12",name);
    CASE(0,L"DATUM_POINT_CURVE %ls AT_START CURVE:LINE_40 START 0",name);
    CASE(0,L"DATUM_POINT_CURVE %ls AT_END CURVE:LINE_40 END 0",name);
    CASE(1,L"DATUM_POINT_CURVE %ls OUTSIDE CURVE:LINE_40 LENGTH 50",name);
    CASE(0,L"DATUM_PLANE_NORMAL %ls LINE_NORMAL CURVE:LINE_40 POINT:LENGTH_12",name);
    CASE(1,L"DATUM_PLANE_NORMAL %ls MISSED_NORMAL CURVE:LINE_40 POINT:P_YZ",name);
    CASE(0,L"SKETCH_CREATE %ls ARC_ROUTE PLANE:TOP PLANE:RIGHT ARC 0 0 20 0 90",name);
    CASE(0,L"DATUM_POINT_CURVE %ls ARC_POINT CURVE:ARC_ROUTE PERCENT 40",name);
    CASE(0,L"DATUM_PLANE_NORMAL %ls ARC_NORMAL CURVE:ARC_ROUTE POINT:ARC_POINT",name);
    CASE(0,L"DATUM_PLANE %ls MID_Z PLANE:FRONT 10",name);
    CASE(0,L"DATUM_CURVE_INTERSECT %ls INTERSECTION SURFACE:%d PLANE:MID_Z",name,f.surface);
    CASE(0,L"DATUM_PLANE %ls OUT_Z PLANE:FRONT 50",name);
    CASE(1,L"DATUM_CURVE_INTERSECT %ls DISJOINT SURFACE:%d PLANE:OUT_Z",name,f.surface);
    CASE(0,L"DATUM_CURVE_SPLINE %ls SPACE_SPLINE %ls 0 0 0 8 12 3 17 -2 11 28 10 19",name,f.token);
    CASE(1,L"DATUM_CURVE_SPLINE %ls DUPLICATE_SPLINE %ls 0 0 0 8 12 3 8 12 3",name,f.token);
    CASE(0,L"DATUM_CSYS_SYM %ls SYMMETRY PLANE:MID_X PLANE:TOP PLANE:MID_Z POS POS",name);
    CASE(0,L"DATUM_CSYS_SYM %ls SYMMETRY_NEG PLANE:MID_X PLANE:TOP PLANE:MID_Z NEG NEG",name);
    CASE(1,L"DATUM_CSYS_SYM %ls BAD_SYMMETRY PLANE:MID_X PLANE:END_X PLANE:MID_Z POS POS",name);
    CASE(0,L"DATUM_CURVE_SPLINE %ls TRANSFORMED_SPLINE CSYS:SYMMETRY_NEG 0 0 0 8 12 3 17 -2 11 28 10 19",name);
    e=verify_changed_references(model,f.surface);
    cases++;if(e)failures++;trace("verify midplane, coordinate system and intersection after parent dimensions change",e);
    expect_volume(model,8000*3.141592653589793);
    if((e=fixture(&model,name,&block))){trace("fixture countersink block",e);failures++;goto done;}
    {
        int i,edge=0;
        for(i=0;i<block.nv;i++){
            ProGeomitem item;
            ProEdge candidate;
            ProVector xyz,d1,d2,d3;
            if(!ProModelitemInit(model,block.vertical[i],PRO_EDGE,&item)&&!ProGeomitemToEdge(&item,&candidate)&&
               !ProEdgeXyzdataEval(candidate,0,xyz,d1,d2,d3)&&fabs(xyz[0])<1e-6){edge=block.vertical[i];break;}
        }
        CASE(0,L"DATUM_PLANE_ANGLE %ls EDGE_HINGE PLANE:RIGHT EDGE:%d 25",name,edge);
    }
    f.model=model;f.wanted=PRO_SRF_CONE;
    CASE(0,L"HOLE_COUNTERSINK %ls SINK SURFACE:%d SURFACE:%d 40 SURFACE:%d 30 6 12 90 THROUGH 0",name,block.top,block.x,block.y);
    if((e=ProSolidCsysVisit((ProSolid)model,find_csys,NULL,&f))!=PRO_TK_E_FOUND||
       (e=ProSolidSurfaceVisit((ProSolid)model,find_surface,NULL,&f))!=PRO_TK_E_FOUND||
       (e=model_to_local(&f,local))){trace("get conical countersink geometry",e);failures++;goto done;}
    POINT(L"P_CONE_TOUCH",local[0],local[1],local[2]);
    CASE(0,L"DATUM_PLANE_TANGENT %ls CONE_TANGENT SURFACE:%d POINT:P_CONE_TOUCH",name,f.surface);
    expect_volume(model,96000-216*3.141592653589793);
done:
    trace("ADVANCED_DATUM_CASES",cases);trace("ADVANCED_DATUM_FAILURES",failures);trace("ADVANCED_DATUM_DONE",1);
    ProEngineerEnd();return 0;
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
