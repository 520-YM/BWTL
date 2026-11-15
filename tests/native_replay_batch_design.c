#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES
#include <ProAnnotation.h>

typedef struct {ProMdl model;int face,edge;} ReferenceEdge;

static ProError design_reference_edge(ProEdge edge,ProError status,ProAppData data)
{
    if(status)return status;
    ReferenceEdge *pick=data;
    ProVector a,b;
    if(!ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL)&&
        !ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)&&fabs(a[0]-b[0])>75)
        return ProEdgeIdGet(edge,&pick->edge);
    return PRO_TK_NO_ERROR;
}

static ProError design_reference_contour(ProContour contour,ProError status,ProAppData data)
{
    if(status)return status;
    ReferenceEdge *pick=data;
    ProSurface surface;
    ProError error=ProSurfaceInit((ProSolid)pick->model,pick->face,&surface);
    return error?error:ProContourEdgeVisit(surface,contour,design_reference_edge,NULL,pick);
}

static ProError design_reference_dimension(ProMdl model,const Fixture *faces)
{
    ProDimAttachment *attachments=NULL;
    ProDimSense *senses=NULL;
    ProModelitem face;
    ProSelection plane_selection=NULL;
    ProAnnotationPlane plane;
    ProDimension dimension;
    ProVector direction={0,0,1};
    Pro3dPnt position={40,-15,20};
    ProError error=ProArrayAlloc(1,sizeof(*senses),1,(ProArray *)&senses);
    if(!error)error=ProArrayAlloc(1,sizeof(*attachments),1,(ProArray *)&attachments);
    if(!error){
        memset(attachments,0,sizeof(*attachments));
        memset(senses,0,sizeof(*senses));
    }
    if(!error)error=ProModelitemInit(model,faces->top,PRO_SURFACE,&face);
    if(!error)error=ProSelectionAlloc(NULL,&face,&plane_selection);
    if(!error)error=ProAnnotationplaneCreate(plane_selection,direction,&plane);
    ReferenceEdge pick={model,faces->top,0};
    ProSurface top;
    if(!error)error=ProSurfaceInit((ProSolid)model,faces->top,&top);
    if(!error)error=ProSurfaceContourVisit(top,design_reference_contour,NULL,&pick);
    if(!error&&!pick.edge)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProModelitemInit(model,pick.edge,PRO_EDGE,&face);
    if(!error)error=ProSelectionAlloc(NULL,&face,&attachments[0][0]);
    if(!error)error=ProDimensionCreate((ProSolid)model,PRO_REF_DIMENSION,&plane,attachments,senses,PRO_DIM_ORNT_NONE,position,&dimension);
    if(plane_selection)ProSelectionFree(&plane_selection);
    if(attachments){
        if(attachments[0][0])ProSelectionFree(&attachments[0][0]);
        ProArrayFree((ProArray *)&attachments);
    }
    if(senses)ProArrayFree((ProArray *)&senses);
    return error;
}

static ProError batch_design_relations(FILE *log)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&source,name,&faces);
    if(!error)error=batch_step(source,log,L"CREO_OP HOLE %ls DRIVE_HOLE SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 6 THROUGH 0",name,faces.top,faces.x,faces.y);
    if(!error)error=batch_step(source,log,L"CREO_OP HOLE %ls FOLLOW_HOLE SURFACE:%d SURFACE:%d 45 SURFACE:%d 30 8 THROUGH 0",name,faces.top,faces.x,faces.y);
    ProFeature feature;
    ReplayDimensionMatch driver={.type=PRODIMTYPE_DIAMETER,.value=6},follower={.type=PRODIMTYPE_DIAMETER,.value=8};
    if(!error)error=ux_named_feature(source,L"DRIVE_HOLE",&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&driver);
    if(!error)error=ux_named_feature(source,L"FOLLOW_HOLE",&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&follower);
    if(!error&&(driver.count!=1||follower.count!=1))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionSymbolSet(&driver.target,L"BASE_DIA");
    if(!error)error=ProDimensionSymbolSet(&follower.target,L"FINAL_DIA");
    ProModelitem owner;
    ProRelset rel;
    ProParameter param;
    ProParamvalue increment={.type=PRO_PARAM_DOUBLE,.value.d_val=2};
    if(!error)error=ProMdlToModelitem(source,&owner);
    if(!error)error=ProParameterWithUnitsCreate(&owner,L"DIA_DELTA",&increment,NULL,&param);
    if(!error){
        error=ProModelitemToRelset(&owner,&rel);
        if(error==PRO_TK_E_NOT_FOUND)error=ProRelsetCreate(&owner,&rel);
    }
    ProWstring *lines=NULL;
    if(!error)error=ProArrayAlloc(1,sizeof(*lines),1,(ProArray *)&lines);
    if(!error)lines[0]=L"FINAL_DIA = BASE_DIA + DIA_DELTA";
    if(!error)error=ProRelsetRelationsSet(&rel,lines,1);
    if(lines)ProArrayFree((ProArray *)&lines);
    if(!error)error=ProRelsetRegenerate(&rel);
    if(!error)error=part_regenerate(source);
    if(!error){
        error=design_reference_dimension(source,&faces);
        fwprintf(log,L"REFERENCE DIMENSION CREATE=%d\n",error);
        wchar_t ref_report[PART_TEXT_MAX]=L"";
        PartText ref_out={ref_report,_countof(ref_report),0};
        if(!error)imitation_dimension_report(source,&ref_out);
        if(!error&&(!wcsstr(ref_report,L"参考尺寸 1 个")||
           !wcsstr(ref_report,L"关系驱动 1 个")||!wcsstr(ref_report,L"rd0")))error=PRO_TK_INCOMPLETE;
        fwprintf(log,L"%ls\n",ref_report);
    }
    ProWstring *got=NULL;
    int lines_found=0;
    if(!error)error=ProArrayAlloc(0,sizeof(*got),2,(ProArray *)&got);
    if(!error)error=ProRelsetRelationsGet(&rel,&got);
    if(!error)error=ProArraySizeGet((ProArray)got,&lines_found);
    fwprintf(log,L"SOURCE RELATIONS=%d CREO=%d\n",lines_found,error);
    for(int i=0;!error&&i<lines_found;i++)fwprintf(log,L"RELATION %ls\n",got[i]);
    if(got)ProWstringproarrayFree(got);
    if(!error)error=batch_reopen(source,&result,log);
    UxDimension original,changed;
    if(!error)error=ux_dimension(result,L"BASE_DIA",&original);
    if(!error)error=ux_dimension(result,L"FINAL_DIA",&changed);
    ProBoolean related=PRO_B_FALSE;
    if(!error)error=ProDimensionIsReldriven(&changed.dim,&related);
    if(!error&&(!related||!part_close(changed.value,8)))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&original.dim,7);
    if(!error)error=part_regenerate(result);
    if(!error)error=ux_dimension(result,L"FINAL_DIA",&changed);
    if(!error&&!part_close(changed.value,9))error=PRO_TK_INCOMPLETE;
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)imitation_dimension_report(result,&out);
    if(!error&&(!wcsstr(report,L"FINAL_DIA")||!wcsstr(report,L"关系驱动 1 个")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"NAMED RELATION, PARAMETER AND DRIVER=%d\n%ls\n",error,report);
    if(result)ProMdlErase(result);
    if(source)ProMdlErase(source);
    return error;
}

typedef struct {ProMdl model;int face,edge;} DesignStock;
static ProError design_edge(ProEdge edge,ProError status,ProAppData data)
{
    if(status)return status;
    DesignStock *stock=data;
    ProVector a,b;
    ProError error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL);
    if(!error)error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL);
    if(!error&&fabs(a[1])<0.001&&fabs(b[1])<0.001&&fabs(a[0]-b[0])>70)
        error=ProEdgeIdGet(edge,&stock->edge);
    return error;
}
static ProError design_contour(ProContour contour,ProError status,ProAppData data)
{
    if(status)return status;
    DesignStock *stock=data;
    ProSurface surface;
    ProError error=ProSurfaceInit((ProSolid)stock->model,stock->face,&surface);
    return error?error:ProContourEdgeVisit(surface,contour,design_edge,NULL,stock);
}
static ProError design_stock(ProMdl model,DesignStock *stock)
{
    BatchFace face={model,0};
    ProError error=ProSolidSurfaceVisit((ProSolid)model,batch_face,NULL,&face);
    if(!error&&!face.face)error=PRO_TK_E_NOT_FOUND;
    ProSurface surface;
    if(!error)error=ProSurfaceInit((ProSolid)model,face.face,&surface);
    *stock=(DesignStock){model,face.face,0};
    if(!error)error=ProSurfaceContourVisit(surface,design_contour,NULL,stock);
    return !error&&!stock->edge?PRO_TK_E_NOT_FOUND:error;
}

static ProError design_vent_face(ProSurface surface,ProError status,ProAppData data)
{
    if(status)return status;
    DesignStock *stock=data;
    ProGeomitem item;
    ProVector p,n;
    if(!ProSurfaceToGeomitem((ProSolid)stock->model,surface,&item)&&!fd_plane_data(&item,p,n)&&
       n[2]>0.999&&fabs(p[2]-2)<1e-5)stock->face=item.id;
    return PRO_TK_NO_ERROR;
}

static ProError design_vent_pattern_edit(ProMdl model,FILE *log)
{
    ProFeature seed,sketch,*members=NULL;
    ProPattern pattern;
    ProMassProperty before,after,restored;
    ReplayDimensionMatch diameter={.type=PRODIMTYPE_DIAMETER,.value=4};
    int count=0;
    ProError error=ux_named_feature(model,L"VENT_SEED",&seed);
    if(!error)error=ProFeaturePatternGet(&seed,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternMembersGet(&pattern,&members);
    if(!error)error=ProArraySizeGet((ProArray)members,&count);
    if(!error&&count!=6)error=PRO_TK_INCOMPLETE;
    if(!error)error=ux_named_feature(model,L"VENT_SEED_SK",&sketch);
    if(!error)error=ProFeatureDimensionVisit(&sketch,replay_match_dimension,NULL,&diameter);
    if(!error&&diameter.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(model,&before);
    if(!error)error=ProDimensionValueSet(&diameter.target,4.25);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&after);
    if(!error&&!part_close(before.volume-after.volume,count*AS_PI*(2.125*2.125-4)*2))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&diameter.target,4);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&restored);
    if(!error&&!part_close(restored.volume,before.volume))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SIX VENT INSTANCES FOLLOW SEED=%d\n",error);
    if(members)ProArrayFree((ProArray *)&members);
    return error;
}

static ProError batch_design_sheet(int kind,FILE *log)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    ProError error=PRO_TK_NO_ERROR;
    DesignStock stock;
    if(kind==4){
        ModelPlan plan={0,80,60,2,0,1};
        const char *stage="vent fixture";
        error=model_create(&plan,name,&stage);
        if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
        if(!error)error=batch_step(source,log,L"CREO_OP DATUM_CSYS %ls VENT_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",name);
        if(!error)error=batch_step(source,log,L"CREO_OP SMT_CONVERT %ls TO_SMT SURFACE:96 2",name);
        stock=(DesignStock){.model=source};
        if(!error)error=ProSolidSurfaceVisit((ProSolid)source,design_vent_face,NULL,&stock);
        if(!error&&!stock.face)error=PRO_TK_E_NOT_FOUND;
        if(!error)error=batch_step(source,log,L"CREO_OP VENT_ROUND %ls FINISH SURFACE:%d CSYS:VENT_AT PLANE:RIGHT 17 13 2 2 3 6 6 4 GRID",name,stock.face);
    }else{
        error=batch_sheet(&source,name,log);
        if(!error)error=design_stock(source,&stock);
    }
    if(!error&&(kind==0||kind==1||kind==5))error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls BEND_LINE PLANE:FRONT PLANE:RIGHT LINE 0 40 80 40",name);
    if(!error&&(kind==0||kind==1||kind==5))error=batch_step(source,log,L"CREO_OP SMT_BEND %ls BEND_A CURVE:BEND_LINE 90 2 POS FIX1",name);
    if(!error&&kind==0)error=batch_step(source,log,L"CREO_OP SMT_RELIEF %ls BEND_A ALL OBROUND 3 4 3",name);
    if(!error&&kind==5)error=batch_step(source,log,L"CREO_OP SMT_RELIEF %ls BEND_A ALL RECT 3 4 3",name);
    if(!error&&kind==1)error=batch_step(source,log,L"CREO_OP SMT_UNBEND %ls FLAT_A SURFACE:%d",name,stock.face);
    if(!error&&kind==1)error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls CROSS_SK PLANE:FRONT PLANE:RIGHT SLOT 40 40 22 6 90",name);
    if(!error&&kind==1)error=batch_step(source,log,L"CREO_OP SMT_CUT %ls CROSS_CUT SKETCH:CROSS_SK NEG",name);
    if(!error&&kind==1)error=batch_step(source,log,L"CREO_OP SMT_REBEND %ls FINISH SURFACE:%d",name,stock.face);
    if(!error&&kind==2)error=batch_step(source,log,L"CREO_OP SMT_HEM %ls FINISH EDGE:%d OPEN 20 2 POS",name,stock.edge);
    if(!error&&kind==3)error=batch_step(source,log,L"CREO_OP SMT_HEM %ls FINISH EDGE:%d ROUND 10 4 POS",name,stock.edge);
    if(!error&&kind==6)error=batch_step(source,log,L"CREO_OP SMT_SKETCH %ls VENT_SEED_SK PLANE:FRONT PLANE:RIGHT CIRCLE 20 20 2",name);
    if(!error&&kind==6)error=batch_step(source,log,L"CREO_OP SMT_CUT %ls VENT_SEED SKETCH:VENT_SEED_SK NEG",name);
    if(!error&&kind==6){
        wchar_t protocol[512];
        PartCommand command;
        FdTxn tx={0};
        swprintf_s(protocol,_countof(protocol),L"CREO_OP REUSE_GRID %ls VENT_GRID VENT_SEED PLANE:RIGHT 3 14 POS PLANE:TOP 2 12 POS",name);
        if(part_command_parse(protocol,&command)!=1)error=PRO_TK_BAD_INPUTS;
        if(!error)error=sh_begin(&tx,source,L"VENT_GRID",PRO_FEAT_GROUP_HEAD);
        if(!error)error=ux_pattern(&tx,&command,UX_GRID);
        fwprintf(log,L"NATIVE SHEET VENT GRID CREATION=%d\n",error);
        fd_cleanup(&tx,0);
    }
    fwprintf(log,L"DESIGN CASE %d SOURCE=%d\n",kind,error);
    if(!error)error=batch_reopen(source,&result,log);
    if(!error&&(kind==0||kind==5)){
        ProFeature bend;
        error=ux_named_feature(result,L"BEND_A",&bend);
        for(int side=0;!error&&side<2;side++){
            ProElemId path[]={PRO_E_SMT_BEND_RELIEF,side?PRO_E_SMT_BEND_RELIEF_SIDE2:PRO_E_SMT_BEND_RELIEF_SIDE1,PRO_E_BEND_RELIEF_TYPE};
            error=mc_tree_value(&bend,path,3,kind==0?PRO_BEND_RLF_OBROUND:PRO_BEND_RLF_RECTANGULAR,1);
            path[2]=PRO_E_BEND_RELIEF_WIDTH;
            if(!error)error=mc_tree_value(&bend,path,3,3,0);
        }
    }
    if(!error&&kind==1)error=batch_edit(result,L"CROSS_SK",PRODIMTYPE_LINEAR,16,17,log);
    if(!error&&kind==1){
        wchar_t flat[PART_TEXT_MAX]=L"";
        PartText out={flat,_countof(flat),0};
        ProMdl models[]={source,result};
        ProMdl history[2]={NULL,NULL};
        for(int i=0;!error&&i<2;i++){
            ProName name;
            swprintf_s(name,_countof(name),L"cross_flat_%d",i);
            error=ProMdlnameCopy(models[i],name,&history[i]);
            ProFeature last;
            if(!error)error=ux_named_feature(history[i],L"CROSS_CUT",&last);
            if(!error)error=ProFeatureInsertmodeWithoptionsActivate(&last,PRO_REGEN_NO_RESOLVE_MODE);
        }
        ImitationShapeMetrics shape;
        if(!error)error=imitation_shape_read(history[0],&shape);
        if(!error)error=imitation_shape_verify_mode(history[0],history[1],&shape,0,&out);
        for(int i=0;i<2;i++)if(history[i])ProMdlErase(history[i]);
        fwprintf(log,L"CROSS BEND FLAT=%d\n%ls\n",error,flat);
    }
    if(!error&&kind==2)error=batch_edit(result,L"FINISH",PRODIMTYPE_RADIUS,2,2.25,log);
    if(!error&&kind==3)error=batch_edit(result,L"FINISH",PRODIMTYPE_RADIUS,4,4.25,log);
    if(!error&&kind==4)error=batch_edit(result,L"FINISH_SK",PRODIMTYPE_DIAMETER,4,4.25,log);
    if(!error&&kind==6)error=design_vent_pattern_edit(result,log);
    if(result)ProMdlErase(result);
    if(source)ProMdlErase(source);
    fwprintf(log,L"DESIGN CASE %d RESULT=%d\n",kind,error);
    fflush(log);
    return error;
}

static ProError design_projection_length(ProMdl model,double *length)
{
    FdSketch sketch={0};
    int *ids=NULL,count=0,found=0;
    ProError error=fd_sketch_open(model,L"PROJECT_SK",&sketch,0);
    if(!error)error=ProSectionEntityIdsGet(sketch.section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        ProBoolean projected=PRO_B_FALSE;
        error=ProSectionEntityIsProjection(sketch.section,ids[i],&projected);
        if(error||!projected)continue;
        Pro2dEntdef *entity=NULL;
        error=ProSectionEntityGet(sketch.section,ids[i],&entity);
        if(!error&&entity->type==PRO_2D_LINE){
            const Pro2dLinedef *line=(const Pro2dLinedef *)entity;
            *length=hypot(line->end1[0]-line->end2[0],line->end1[1]-line->end2[1]);
            found++;
        }
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&sketch);
    return error?error:found==1?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError batch_design_projection(FILE *log)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&source,name,&faces);
    if(!error)error=batch_step(source,log,L"CREO_OP DATUM_CURVE %ls PROJECT_SOURCE CSYS:PRT_CSYS_DEF 0 0 0 20 0 10",name);
    if(!error)error=batch_step(source,log,L"CREO_OP SKETCH_CREATE %ls PROJECT_SK PLANE:TOP PLANE:RIGHT CIRCLE 30 30 2",name);
    if(!error)error=batch_step(source,log,L"CREO_OP SKETCH_PROJECT %ls PROJECT_SK CURVE:PROJECT_SOURCE",name);
    if(!error)error=batch_reopen(source,&result,log);
    double before=0,after=0;
    if(!error)error=design_projection_length(result,&before);
    wchar_t preview[PART_TEXT_MAX]=L"";
    PartText out={preview,_countof(preview),0};
    if(!error)error=imitation_diagnose(result,&out);
    if(!error&&(!wcsstr(preview,L"模板基准 RIGHT")||!wcsstr(preview,L"位于页脚的特征 3")||!wcsstr(preview,L"投影图元 1 个")||
        !wcsstr(preview,L"约束 ")||!wcsstr(preview,L"轮廓类型 ")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"FEATURE TREE AND SECTION AUDIT=%d\n%ls\n",error,preview);
    ProFeature feature;
    ProElement tree=NULL,equation=NULL;
    ProFeatureCreateOptions *options=NULL;
    if(!error)error=ux_named_feature(result,L"PROJECT_SOURCE",&feature);
    if(!error)error=ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_CRV_ENTER_EQUATION},1,&equation);
    if(!error)error=ProElementWstringSet(equation,L"x=40*t\ny=0\nz=20*t");
    if(!error)error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        options[0]=PRO_FEAT_CR_NO_OPTS;
        error=ProFeatureWithoptionsRedefine(NULL,&feature,tree,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,NULL);
    }
    if(!error)error=part_regenerate(result);
    if(!error)error=design_projection_length(result,&after);
    if(!error&&(!part_close(before,sqrt(500.0))||!part_close(after,2*before)))error=PRO_TK_INCOMPLETE;
    if(options)ProArrayFree((ProArray *)&options);
    if(tree)ProFeatureElemtreeFree(&feature,tree);
    if(result)ProMdlErase(result);
    if(source)ProMdlErase(source);
    fwprintf(log,L"SKETCH PROJECTION LINK %.8g -> %.8g ERROR=%d\n",before,after,error);
    return error;
}

static ProError batch_design_difference(FILE *log)
{
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&source,name,&faces);
    if(!error)error=batch_step(source,log,L"CREO_OP HOLE %ls OFFSET_HOLE SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 6 THROUGH 0",
        name,faces.top,faces.x,faces.y);
    if(!error)error=fixture(&result,name,&faces);
    if(!error)error=batch_step(result,log,L"CREO_OP HOLE %ls OFFSET_HOLE SURFACE:%d SURFACE:%d 26 SURFACE:%d 20 6 THROUGH 0",
        name,faces.top,faces.x,faces.y);
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error&&imitation_compare(source,result,&out)!=PRO_TK_INCOMPLETE)error=PRO_TK_INCOMPLETE;
    const wchar_t *original=wcsstr(report,L"原件 SURFACE:");
    const wchar_t *rebuilt=wcsstr(report,L"结果 SURFACE:");
    if(!error&&(!original||!rebuilt||!wcsstr(original,L"圆柱面直径 6 mm")||
        !wcsstr(rebuilt,L"圆柱面直径 6 mm")||!wcsstr(rebuilt,L"轴线距模型原点最近点")||
        wcsstr(report,L"实体校验差异。")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SHIFTED HOLE WITH MATCHING GLOBAL METRICS=%d\n%ls\n",error,report);
    if(result)ProMdlErase(result);
    if(source)ProMdlErase(source);
    return error;
}

static int batch_design_test(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-batch-design.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    int pass=batch_design_relations(log)==PRO_TK_NO_ERROR;
    pass+=batch_design_projection(log)==PRO_TK_NO_ERROR;
    for(int kind=0;kind<7;kind++)pass+=batch_design_sheet(kind,log)==PRO_TK_NO_ERROR;
    pass+=batch_design_difference(log)==PRO_TK_NO_ERROR;
    fwprintf(log,L"DESIGN CASES=%d PASS=%d\n",pass,pass==10);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayBatchDesign",batch_design_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLReplayBatchDesign`;");
}
