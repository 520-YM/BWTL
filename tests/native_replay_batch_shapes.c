#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError batch_step(ProMdl model,FILE *log,const wchar_t *format,...)
{
    wchar_t command[512];
    va_list args;
    va_start(args,format);
    int length=vswprintf_s(command,_countof(command),format,args);
    va_end(args);
    return length<0?PRO_TK_BAD_INPUTS:expansion_build(model,command,log);
}

static ProError batch_empty(int kind,ProMdl *model,ProMdlName name)
{
    const char *stage="create empty replay fixture";
    ProError error=model_create_empty(kind,name,&stage);
    return error?error:ProMdlnameInit(name,PRO_MDL_PART,model);
}

static ProError batch_sheet(ProMdl *model,ProMdlName name,FILE *log)
{
    ProError error=batch_empty(2,model,name);
    if(!error)error=batch_step(*model,log,L"CREO_OP SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name);
    if(!error)error=batch_step(*model,log,L"CREO_OP SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",name);
    return error;
}

typedef struct {ProMdl model;int face;} BatchFace;
static ProError batch_face(ProSurface surface,ProError status,ProAppData data)
{
    if(status)return status;
    BatchFace *face=data;
    ProSmtSurfType side;
    ProGeomitem item;
    ProVector point,normal;
    if(!ProSmtSurfaceTypeGet((ProPart)face->model,surface,&side)&&side==PRO_SMT_SURF_FACE&&
       !ProSurfaceToGeomitem((ProSolid)face->model,surface,&item)&&!fd_plane_data(&item,point,normal)&&
       fabs(normal[2])>.999&&fabs(point[2])<.001)face->face=item.id;
    return PRO_TK_NO_ERROR;
}

static ProError batch_reopen(ProMdl source,ProMdl *result,FILE *log)
{
    wchar_t report[20000]=L"";
    PartText out={report,_countof(report),0};
    ProPath path=L"";
    ProError error=ProMdlSave(source);
    if(!error)error=imitation_run_mode(source,path,0,0,&out);
    if(!error)error=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,result);
    fwprintf(log,L"REPLAY=%d\n%ls\n",error,report);
    fflush(log);
    return error;
}

static ProError batch_edit(ProMdl result,const wchar_t *name,ProDimensiontype type,double value,double next,FILE *log)
{
    ProFeature feature;
    ReplayDimensionMatch edit={.type=type,.value=value};
    ProMassProperty before,changed,restored;
    ProError error=ux_named_feature(result,name,&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&edit);
    if(!error&&edit.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(result,&before);
    if(!error)error=ProDimensionValueSet(&edit.target,next);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&changed);
    if(!error&&part_close(before.volume,changed.volume)&&part_close(before.surface_area,changed.surface_area)){
        ProVector delta;
        ad_sub(before.center_of_gravity,changed.center_of_gravity,delta);
        if(sqrt(ad_dot(delta,delta))<0.001)error=PRO_TK_INCOMPLETE;
    }
    if(!error)error=ProDimensionValueSet(&edit.target,value);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&restored);
    if(!error&&(!part_close(before.volume,restored.volume)||!part_close(before.surface_area,restored.surface_area)))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"EDIT %ls %.6g -> %.6g -> %.6g ERROR=%d\n",name,value,next,value,error);
    return error;
}

static ProError batch_sheet_edit(ProMdl model,FILE *log)
{
    ProDimension thickness;
    double before,after,restored,mm;
    ProMassProperty a,b,c;
    ProError error=fab_thickness(model,&mm,&before);
    if(!error)error=qa_geometry_mass(model,&a);
    if(!error)error=ProSmtPartThicknessGet((ProPart)model,&thickness);
    if(!error)error=ProDimensionValueSet(&thickness,2.5/mm);
    if(!error)error=part_regenerate(model);
    if(!error)error=fab_thickness(model,&mm,&after);
    if(!error)error=qa_geometry_mass(model,&b);
    if(!error&&(!part_close(after,2.5)||b.volume<=a.volume))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&thickness,before/mm);
    if(!error)error=part_regenerate(model);
    if(!error)error=fab_thickness(model,&mm,&restored);
    if(!error)error=qa_geometry_mass(model,&c);
    if(!error&&(!part_close(restored,before)||!part_close(a.volume,c.volume)))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"THICKNESS EDIT 2 -> 2.5 -> 2 ERROR=%d\n",error);
    return error;
}

static ProError batch_material(ProMdl model)
{
    const wchar_t material_name[]=L"metal_steel_low_carbon";
    ProPath directory,destination;
    ProError error=ProDirectoryCurrentGet(directory);
    if(!error&&swprintf_s(destination,_countof(destination),L"%ls\\%ls.mtl",directory,material_name)<0)
        error=PRO_TK_BAD_INPUTS;
    if(!error&&!CopyFileW(L"C:\\Program Files\\PTC\\Creo 10.0.0.0\\Common Files\\creo_standards\\material_database\\metal_steel_low_carbon.mtl",
        destination,TRUE))error=PRO_TK_CANT_WRITE;
    ProMaterial material={.part=(ProSolid)model};
    wcscpy_s(material.matl_name,_countof(material.matl_name),material_name);
    if(!error)error=ProMaterialfileRead((ProSolid)model,material.matl_name);
    if(!error)error=ProMaterialCurrentSet(&material);
    if(!error)error=part_regenerate(model);
    return error;
}

static ProError batch_slot_edit(ProMdl result,FILE *log)
{
    ProFeature feature;
    ReplayDimensionMatch edit={.type=PRODIMTYPE_LINEAR,.value=14};
    ProError error=ux_named_feature(result,L"PROFILE",&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&edit);
    if(error)return error;
    if(edit.count==1)return batch_edit(result,L"PROFILE",PRODIMTYPE_LINEAR,14,15,log);
    return batch_edit(result,L"PROFILE",PRODIMTYPE_DIAMETER,6,6.5,log);
}

static ProError batch_openings(ProMdl source,FILE *log,ProMdlName name)
{
    FdTxn tx={0};
    FdSketch sketch={0};
    ProError error=sh_begin(&tx,source,L"PROFILE",PRO_FEAT_CURVE);
    if(!error)error=fd_sketch_setup(&tx,L"PLANE:FRONT",L"PLANE:RIGHT");
    if(!error)error=fd_create(&tx,1);
    sketch.feature=tx.feature;
    if(!error)error=ProFeatureElemtreeExtract(&sketch.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&sketch.tree);
    if(!error)error=fd_section_from_tree(sketch.tree,&sketch.section);
    ProBoolean intent=PRO_B_FALSE;
    if(!error)error=ProSectionIntentManagerModeGet(sketch.section,&intent);
    if(!error&&intent)error=ProSectionIntentManagerModeSet(sketch.section,PRO_B_FALSE);
    PanelSketch section={sketch.section,{0,0},{1,0},{0,1},1};
    PanelProfile key={.shape=PN_KEY,.x=25,.y=20,.width=12,.height=6,.detail=15};
    if(!error)error=pn_draw(&section,&key);
    if(!error)error=pn_rectangle(&section,55,22,65,36,0);
    if(!error)error=fd_section_solve(sketch.section,1);
    if(!error){
        tx.options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
        error=ProFeatureWithoptionsRedefine(NULL,&sketch.feature,sketch.tree,tx.options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors);
    }
    fd_sketch_free(&sketch);
    fd_cleanup(&tx,error);
    if(!error)error=batch_step(source,log,L"CREO_OP SMT_CUT %ls FINISH SKETCH:PROFILE NEG",name);
    return error;
}

static ProError batch_shapes_trial(int kind,FILE *log)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=PRO_TK_NO_ERROR;
    if(kind<=1){
        error=fixture(&source,name,&faces);
        if(!error&&kind==0)error=batch_step(source,log,L"CREO_OP EDGE_ROUND %ls FINISH 2 EDGE:%d EDGE:%d",name,faces.vertical[0],faces.vertical[2]);
        if(!error&&kind==1)error=batch_step(source,log,L"CREO_OP EDGE_ROUND_VARIABLE %ls FINISH EDGE:%d 1 3",name,faces.vertical[0]);
    }else if(kind==2){
        error=fixture(&source,name,&faces);
        if(!error)error=batch_step(source,log,L"CREO_OP DATUM_PLANE %ls CUT_START PLANE:FRONT 10",name);
        if(!error)error=batch_step(source,log,L"CREO_OP SKETCH_CREATE %ls PROFILE PLANE:CUT_START PLANE:RIGHT RECT 25 8 5 4",name);
        if(!error)error=batch_step(source,log,L"CREO_OP SOLID_EXTRUDE_TO %ls FINISH PROFILE CUT SURFACE:%d POS",name,faces.top);
    }else if(kind==8){
        ModelPlan plan={0,80,60,2,0,1};
        const char *stage="conversion fixture";
        error=model_create(&plan,name,&stage);
        if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
        if(!error)error=batch_step(source,log,L"CREO_OP SMT_CONVERT %ls FINISH SURFACE:96 2",name);
    }else{
        error=batch_sheet(&source,name,log);
        BatchFace face={source,0};
        if(!error)error=ProSolidSurfaceVisit((ProSolid)source,batch_face,NULL,&face);
        if(!error&&!face.face)error=PRO_TK_E_NOT_FOUND;
        if(!error&&(kind==4||kind==5||kind==9))error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls BEND_LINE PLANE:FRONT PLANE:RIGHT LINE 0 40 80 40",name);
        if(!error&&(kind==4||kind==5||kind==9))error=batch_step(source,log,L"CREO_OP SMT_BEND %ls BEND_A CURVE:BEND_LINE 90 2 POS FIX1",name);
        if(!error&&kind==4)error=batch_material(source);
        if(!error&&kind==4)error=batch_step(source,log,L"CREO_OP SMT_ALLOWANCE %ls BEND_A K 0.4",name);
        if(!error&&kind==5)error=batch_step(source,log,L"CREO_OP SMT_UNBEND %ls FLAT_A SURFACE:%d",name,face.face);
        if(!error&&kind==5)error=batch_step(source,log,L"CREO_OP SMT_REBEND %ls FINISH SURFACE:%d",name,face.face);
        if(!error&&kind==6)error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls PROFILE PLANE:FRONT PLANE:RIGHT SLOT 35 30 20 6 0",name);
        if(!error&&kind==6)error=batch_step(source,log,L"CREO_OP SMT_CUT %ls FINISH SKETCH:PROFILE NEG",name);
        if(!error&&kind==3)error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls PROFILE PLANE:FRONT PLANE:RIGHT CIRCLE 20 20 3",name);
        if(!error&&kind==3){
            FdTxn tx={0};
            PartCommand command={0};
            wchar_t protocol[512];
            swprintf_s(protocol,_countof(protocol),L"CREO_OP SMT_CUT %ls FINISH SKETCH:PROFILE NEG",name);
            if(part_command_parse(protocol,&command)!=1)error=PRO_TK_BAD_INPUTS;
            if(!error)error=sh_begin(&tx,source,L"FINISH",PRO_FEAT_CUT);
            if(!error)error=sh_cut_depth(&tx,&command,2.5);
            if(!error)error=part_regenerate(source);
            fd_cleanup(&tx,error);
        }
        if(!error&&kind==7)error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls PROFILE SURFACE:%d PLANE:RIGHT RECT 20 -32 20 12",name,face.face);
        if(!error&&kind==7)error=batch_step(source,log,L"CREO_OP SMT_FORM %ls FINISH SKETCH:PROFILE 8 1 POS",name);
        if(!error&&kind==9){
            FdTxn tx={0};
            wchar_t fixed[64];
            swprintf_s(fixed,_countof(fixed),L"SURFACE:%d",face.face);
            error=sh_begin(&tx,source,L"FINISH",PRO_FEAT_FLAT_PAT);
            if(!error)error=sh_flatten(&tx,fixed,0);
            fd_cleanup(&tx,error);
        }
        if(!error&&kind==10)error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls PROFILE PLANE:FRONT PLANE:RIGHT CIRCLE 20 20 3",name);
        if(!error&&kind==10)error=batch_step(source,log,L"CREO_OP SMT_CUT %ls FINISH SKETCH:PROFILE NEG",name);
        if(!error&&kind==11)error=batch_openings(source,log,name);
    }
    fwprintf(log,L"CASE %d SOURCE=%d\n",kind,error);
    if(!error)error=batch_reopen(source,&result,log);
    if(!error&&kind==0)error=batch_edit(result,L"FINISH",PRODIMTYPE_RADIUS,2,2.5,log);
    if(!error&&kind==1)error=batch_edit(result,L"FINISH",PRODIMTYPE_RADIUS,3,3.5,log);
    if(!error&&kind==4)error=batch_edit(result,L"BEND_A",PRODIMTYPE_ANGLE,90,85,log);
    if(!error&&kind==4)error=batch_sheet_edit(result,log);
    if(!error&&kind==5){
        wchar_t report[PART_TEXT_MAX]=L"";
        PartText out={report,_countof(report),0};
        error=imitation_flat_compare(source,result,&out);
        fwprintf(log,L"FLAT MATCH=%d\n%ls\n",error,report);
    }
    if(!error&&kind==6)error=batch_slot_edit(result,log);
    if(!error&&kind==7)error=batch_edit(result,L"FINISH",PRODIMTYPE_LINEAR,8,8.5,log);
    if(!error&&kind==10){
        ProFeature hole;
        ReplayDimensionMatch diameter={.type=PRODIMTYPE_DIAMETER,.value=6};
        error=ux_named_feature(result,L"PROFILE",&hole);
        if(!error)error=ProFeatureDimensionVisit(&hole,replay_match_dimension,NULL,&diameter);
        if(!error&&diameter.count!=1)error=PRO_TK_INCOMPLETE;
        if(!error)error=ProDimensionValueSet(&diameter.target,7);
        if(!error)error=part_regenerate(result);
        wchar_t report[PART_TEXT_MAX]=L"";
        PartText out={report,_countof(report),0};
        if(!error){
            ProError mismatch=imitation_compare(source,result,&out);
            if(mismatch!=PRO_TK_INCOMPLETE||!wcsstr(report,L"圆柱面直径"))error=PRO_TK_INCOMPLETE;
        }
        fwprintf(log,L"LOCAL HOLE DIFFERENCE=%d\n%ls\n",error,report);
        report[0]=0;
        if(!error){
            ProError mismatch=imitation_flat_compare(source,result,&out);
            if(mismatch!=PRO_TK_INCOMPLETE||!wcsstr(report,L"未匹配图元 4 个"))error=PRO_TK_INCOMPLETE;
        }
        fwprintf(log,L"FLAT HOLE DIFFERENCE=%d\n%ls\n",error,report);
    }
    if(result)ProMdlErase(result);
    if(source)ProMdlErase(source);
    fwprintf(log,L"CASE %d RESULT=%d\n",kind,error);
    fflush(log);
    return error;
}

static int batch_shapes_test(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-batch-shapes.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    HMENU menu=native_imitation_menu_create();
    wchar_t menu_label[40]=L"";
    const int menu_ok=menu&&GetMenuStringW(menu,IMITATION_COMPARE_FLAT,menu_label,_countof(menu_label),MF_BYCOMMAND)>0&&
        !wcscmp(menu_label,L"比对钣金展开…")&&GetMenuState(menu,IMITATION_COMPARE,MF_BYCOMMAND)!=UINT_MAX;
    if(menu)DestroyMenu(menu);
    fwprintf(log,L"FLAT MENU=%d\n",menu_ok);
    int passed=0;
    for(int kind=0;kind<12;kind++)passed+=batch_shapes_trial(kind,log)==PRO_TK_NO_ERROR;
    fwprintf(log,L"CASES=%d PASS=%d\n",passed,passed==12&&menu_ok);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

#ifndef BWTL_REUSE_BATCH_SHAPES
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayBatchShapes",batch_shapes_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLReplayBatchShapes`;");
}
#endif
