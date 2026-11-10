#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate
static ProError inspection_command(ProMdl model,const wchar_t *text,int reject,const wchar_t *expected){
    ProWVerstamp before=NULL,after=NULL;
    ProError error=ProMdlVerstampGet(model,&before);
    ProError result=panel_command(model,text,reject,expected);
    panel_check(!error&&!ProMdlVerstampGet(model,&after)&&ProVerstampEqual(before,after),
        "inspection kept native model version stamp");
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    return result;
}

typedef struct {ProSurface face;Inspection view;int bend,id;} InspectionFixtureEdge;
static ProError inspection_fixture_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    InspectionFixtureEdge *search=data;
    int id;
    ProError error=ProEdgeIdGet(edge,&id);
    if(error)return error;
    if(search->bend){
        wchar_t token[32];
        InspectionBend bend;
        int face;
        swprintf_s(token,_countof(token),L"EDGE:%d",id);
        if(!ProSurfaceIdGet(search->face,&face)&&!in_bend_geometry(&search->view,token,&bend)&&
           bend.wall.id==face&&fabs(bend.axis[0])>0.999)search->id=id;
    }else{
        ProVector a,b;
        if(!ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL)&&!ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)&&
           fabs(a[1])<1e-7&&fabs(b[1])<1e-7&&fabs(fabs(a[0]-b[0])-80)<1e-7)search->id=id;
    }
    return PRO_TK_NO_ERROR;
}

static ProError inspection_fixture_contour(ProContour contour,ProError status,ProAppData data){
    InspectionFixtureEdge *search=data;
    return status?status:ProContourEdgeVisit(search->face,contour,inspection_fixture_edge,NULL,search);
}

static int inspection_find_edge(ProMdl model,int face,int bend){
    InspectionFixtureEdge search={0};
    search.view.model=model;search.view.mm=1;
    search.bend=bend;
    ProError error=ProSurfaceInit((ProSolid)model,face,&search.face);
    if(!error)error=ProSurfaceContourVisit(search.face,inspection_fixture_contour,NULL,&search);
    panel_check(!error&&search.id,"find required original edge");
    return search.id;
}

static ProError fabrication_command(ProMdl model,const wchar_t *text,int reject,const wchar_t *expected){
    ProPath before,after;
    ProError error=ProDirectoryCurrentGet(before);
    ProError result=inspection_command(model,text,reject,expected);
    panel_check(!error&&!ProDirectoryCurrentGet(after)&&!_wcsicmp(before,after),"fabrication restores Creo working directory");
    return result;
}

static int fabrication_sheet(ProMdl *model,ProMdlName name,int bent){
    const ModelPlan plate={0,80,60,2,0,1};
    const char *stage="fabrication sheet fixture";
    PanelFaceFixture top={NULL,2,0};
    wchar_t command[512];
    ProError error=model_create(&plate,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,model);
    if(!error)error=panel_activate(*model);
    top.model=*model;
    if(!error)error=ProSolidSurfaceVisit((ProSolid)*model,panel_pick_top,NULL,&top);
    if(!error&&top.id){
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_CSYS %ls SHEET_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",name);
        error=panel_command(*model,command,0,NULL);
        if(error)return 0;
        swprintf_s(command,_countof(command),L"CREO_OP SMT_CONVERT %ls TO_SMT SURFACE:%d 2",name,top.id);
        error=panel_command(*model,command,0,NULL);
    }
    top.id=0;
    if(!error)error=ProSolidSurfaceVisit((ProSolid)*model,panel_pick_top,NULL,&top);
    if(!error&&top.id&&bent){
        int edge=inspection_find_edge(*model,top.id,0);
        swprintf_s(command,_countof(command),L"CREO_OP SMT_FLANGE %ls FOLD EDGE:%d 14 90 2 POS",name,edge);
        error=panel_command(*model,command,0,NULL);
        top.id=0;
        if(!error)error=ProSolidSurfaceVisit((ProSolid)*model,panel_pick_top,NULL,&top);
    }
    panel_check(!error&&top.id,"create actual sheet fixture and find fixed face");
    return error?0:top.id;
}

static void fabrication_bad_dxf(const wchar_t *path,int units,int variant){
    FILE *f=_wfopen(path,L"wt");
    if(!f){panel_check(0,"write DXF negative fixture");return;}
    fprintf(f,"0\nSECTION\n2\nHEADER\n9\n$INSUNITS\n70\n%d\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",units);
    const double point[][2]={{0,0},{80,0},{80,60},{0,60}};
    for(int i=0;i<(variant==1?3:4);i++)sh_dxf_line(f,"OUTLINE",point[i],point[(i+1)%4]);
    if(variant==2)sh_dxf_line(f,"OUTLINE",point[1],point[0]);
    fprintf(f,"0\nENDSEC\n0\nEOF\n");
    panel_check(!fclose(f),"write DXF negative fixture");
}

static void fabrication_arc_fixture(void){
    FabDxf *d=calloc(1,sizeof(*d));
    FILE *f=_wfopen(L"arcs.dxf",L"wt");
    if(!d||!f){if(f)fclose(f);free(d);panel_check(0,"allocate arc fixture");return;}
    fprintf(f,"0\nSECTION\n2\nHEADER\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n"
        "0\nARC\n8\nOUTLINE\n10\n10\n20\n10\n40\n5\n50\n350\n51\n170\n"
        "0\nARC\n8\nOUTLINE\n10\n10\n20\n10\n40\n5\n50\n170\n51\n350\n"
        "0\nCIRCLE\n8\nHOLES\n10\n10\n20\n10\n40\n2\n0\nENDSEC\n0\nEOF\n");
    ProError error=fclose(f)?PRO_TK_CANT_WRITE:fab_dxf_read(L"arcs.dxf",d);
    panel_check(!error&&d->count==3&&d->holes==1&&fabs(d->low[0]-5)<1e-8&&fabs(d->high[1]-15)<1e-8,
        "analytic arc sweep, cardinal bounds, round hole and closed contour");
    free(d);
}

static int fabrication_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL,other=NULL,sheet=NULL,flat=NULL;
    ProMdlName name,second,bent_name,flat_name;
    Fixture solid,second_solid;
    ProPath directory,origin,step;
    wchar_t command[1200],leaf[PRO_PATH_SIZE];
    model_background_mode=0;
    ProError error=fixture(&model,name,&solid);
    if(!error)error=fixture(&model,name,&solid);
    if(!error)error=panel_activate(model);
    if(error){panel_check(0,"create solid fabrication fixture");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(command,_countof(command),L"CREO_OP " format,__VA_ARGS__),panel_command(model,command,reject,expected))
#define FAB(reject,expected,format,...) (swprintf_s(command,_countof(command),L"CREO_OP FAB_" format,__VA_ARGS__),fabrication_command(model,command,reject,expected))
    CMD(0,NULL,L"DATUM_CSYS %ls TEMPLATE_AT CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name);
    CMD(0,NULL,L"DATUM_CSYS %ls WRONG_AT CSYS:PRT_CSYS_DEF 0 0 19 0 0 0",name);
    CMD(0,NULL,L"HOLE %ls THRU SURFACE:%d SURFACE:%d 40 SURFACE:%d 30 8 THROUGH 0",name,solid.top,solid.x,solid.y);
    FAB(0,L"1 个完整圆柱孔段",L"HOLES %ls fab_holes MODEL",name);
    FAB(0,L"1 个完整圆柱孔段",L"HOLES %ls fab_holes_csys CSYS:TEMPLATE_AT",name);
    FAB(0,L"全部实体边界",L"PAINT %ls fab_paint ALL",name);
    FAB(0,L"指定面合计",L"PAINT %ls fab_faces FACES SURFACE:%d SURFACE:%d",name,solid.top,solid.bottom);
    FAB(1,NULL,L"PAINT %ls repeat_face FACES SURFACE:%d SURFACE:%d",name,solid.top,solid.top);
    FAB(1,NULL,L"PAINT %ls fab_paint ALL",name);
    FAB(0,L"1 个完整圆孔口",L"TEMPLATE %ls fab_template SURFACE:%d CSYS:TEMPLATE_AT",name,solid.top);
    FAB(1,NULL,L"TEMPLATE %ls wrong_frame SURFACE:%d CSYS:WRONG_AT",name,solid.top);
    FAB(0,L"其中 1 项超差",L"INSPECT %ls fab_inspect 3 X 80 0.1 0.2 Y 59 0.1 0.2 Z 20 0 0",name);
    FAB(1,NULL,L"INSPECT %ls missing_dimension 1 D9999 20 0 0",name);
    UxDimension dim;
    if(!ux_dimension(model,L"d0",&dim)){
        const wchar_t *unit;
        ProSolidBody body;
        double measured;
        error=qa_single_body(model,&body);
        if(!error)error=fab_measure(model,&body,1,L"d0",&measured,&unit);
        panel_check(!error&&isfinite(measured),"read actual native dimension symbol");
        if(!error)FAB(0,L"其中 0 项超差",L"INSPECT %ls fab_dimension 1 d0 %.10g 0.001 0.001",name,measured);
    }else panel_check(0,"locate named native dimension d0");
    FAB(1,NULL,L"SORT %ls missing_metadata",name);
    CMD(0,NULL,L"REUSE_PARAM %ls PART_CODE STRING PANEL-81",name);
    CMD(0,NULL,L"REUSE_PARAM %ls REVISION STRING A",name);
    if((error=fixture(&other,second,&second_solid))||panel_activate(other)){
        panel_check(0,"create second model for delivery groups");goto done;
    }
    ProMdl original=model;
    model=other;
    CMD(0,NULL,L"REUSE_PARAM %ls DRAWING_NO STRING PANEL-82",second);
    CMD(0,NULL,L"REUSE_PARAM %ls REVISION STRING B",second);
    model=original;
    panel_check(!panel_activate(model),"activate first delivery model");
    FAB(0,L"2 套 STEP",L"SORT %ls fab_sort %ls",name,second);
    FAB(1,NULL,L"SORT %ls fab_sort",name);
    error=ProDirectoryCurrentGet(directory);
    if(!error){
        swprintf_s(origin,_countof(origin),L"%ls\\fab_sort\\PANEL-81__A",directory);
        for(int i=0;i<2;i++){
            swprintf_s(leaf,_countof(leaf),L"%ls.%ls",name,i?L"step":L"stp");
            fab_leaf_path(origin,leaf,L"",step);
            if(dv_file_nonempty(step))break;
        }
        fab_leaf_path(directory,leaf,L"",origin);
        panel_check(CopyFileW(step,origin,TRUE),"copy actual released STEP into readback input directory");
    }
    FAB(0,L"STEP 回读通过",L"READBACK %ls STEP %ls 0.01",name,leaf);
    model=other;
    panel_check(!panel_activate(model),"activate mismatch reference model");
    FAB(1,L"检测到几何差异",L"READBACK %ls STEP %ls 0.001",second,leaf);
    model=original;
    panel_check(!panel_activate(model),"return to released reference model");
    FAB(1,NULL,L"READBACK %ls STEP unavailable.stp 0.01",name);
    int bend_face=fabrication_sheet(&sheet,bent_name,1);
    int flat_face=fabrication_sheet(&flat,flat_name,0);
    if(!bend_face||!flat_face)goto done;
    model=sheet;
    panel_check(!panel_activate(model),"activate bent sheet");
    CMD(0,L"按钮开孔",L"PN_BUTTON %ls SHEET_HOLE SURFACE:%d CSYS:SHEET_AT PLANE:RIGHT 40 30 2 6 0 0",bent_name,bend_face);
    FAB(0,L"生成 1 条折弯记录",L"BENDS %ls fab_bend_table",bent_name);
    FAB(0,L"厚 2 mm",L"BLANK %ls fab_blank SURFACE:%d",bent_name,bend_face);
    FAB(0,L"内孔图元 2，折弯线 1",L"DXF_CHECK %ls SURFACE:%d",bent_name,bend_face);
    FAB(0,L"逐份回读 2 个",L"DXF %ls fab_batch SURFACE:%d %ls SURFACE:%d",bent_name,bend_face,flat_name,flat_face);
    FAB(1,NULL,L"DXF %ls fab_bad_batch SURFACE:%d %ls SURFACE:99999",bent_name,bend_face,flat_name);
    fab_leaf_path(directory,L"fab_bad_batch",L"",origin);
    panel_check(GetFileAttributesW(origin)==INVALID_FILE_ATTRIBUTES,"failed batch does not publish a subset");
    model=flat;
    panel_check(!panel_activate(model),"activate flat plate");
    FAB(0,L"生成 0 条折弯记录",L"BENDS %ls fab_flat_bends",flat_name);
    FAB(0,L"80 × 60 mm",L"BLANK %ls fab_flat_blank SURFACE:%d",flat_name,flat_face);
    FAB(0,L"折弯线 0",L"DXF_CHECK %ls SURFACE:%d",flat_name,flat_face);
    swprintf_s(step,_countof(step),L"%ls\\fab_batch\\%ls.dxf",directory,flat_name);
    fab_leaf_path(directory,L"verified",L".dxf",origin);
    panel_check(CopyFileW(step,origin,TRUE),"copy actual Creo DXF for readback");
    FAB(0,L"DXF 回读通过",L"READBACK %ls DXF verified.dxf SURFACE:%d 0.001",flat_name,flat_face);
    model=sheet;
    panel_check(!panel_activate(model),"reactivate bent sheet");
    FAB(1,L"存在差异",L"READBACK %ls DXF verified.dxf SURFACE:%d 0.001",bent_name,bend_face);
    fabrication_bad_dxf(L"badunits.dxf",1,0);
    fabrication_bad_dxf(L"open.dxf",4,1);
    fabrication_bad_dxf(L"duplicate.dxf",4,2);
    FAB(1,L"未通过单位",L"READBACK %ls DXF badunits.dxf SURFACE:%d 0.01",bent_name,bend_face);
    FAB(1,L"未通过单位",L"READBACK %ls DXF open.dxf SURFACE:%d 0.01",bent_name,bend_face);
    FAB(1,L"未通过单位",L"READBACK %ls DXF duplicate.dxf SURFACE:%d 0.01",bent_name,bend_face);
    fabrication_arc_fixture();
    panel_check(!dv_recovery_uncertain&&!sh_recovery_uncertain,"no uncertain staging or temporary model recovery");
#undef FAB
#undef CMD
done:
    trace("FABRICATION_CASES",cases);
    trace("FABRICATION_FAILURES",failures);
    trace("FABRICATION_DONE",1);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    trace("legacy_bridge_loaded",0);
    uiCmdCmdId command;
    ProError error=ProCmdActionAdd("BWTLFabricationProbe",fabrication_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLFabricationProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
