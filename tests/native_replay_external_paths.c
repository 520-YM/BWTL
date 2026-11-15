#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

static ProError path_workpiece(ProMdl *part,ProMdlName name)
{
    Fixture faces;
    return fixture(part,name,&faces);
}

static ProError path_plane(ProMdl part,ProAsmcomppath *consumer,ProAsmcomppath *provider,
                           ProModelitem *points,const wchar_t *name,ProFeature *created)
{
    FdTxn tx={0};
    FdRef ref[3]={0};
    ProModelitem owner;
    ProElement constraints=NULL;
    ProError error=sh_begin(&tx,part,name,PRO_FEAT_DATUM);
    if(!error)error=ProSelectionFree(&tx.model_selection);
    if(!error)error=ProMdlToModelitem(part,&owner);
    if(!error)error=ProSelectionAlloc(consumer,&owner,&tx.model_selection);
    if(!error)error=model_element(tx.root,PRO_E_DTMPLN_CONSTRAINTS,0,0,&constraints);
    for(int i=0;!error&&i<3;i++){
        ref[i].item=points[i];
        error=ProSelectionAlloc(provider,&points[i],&ref[i].selection);
        if(!error)error=ad_plane_constraint(&tx,constraints,PRO_DTMPLN_THRU,&ref[i],0);
    }
    if(!error)error=fd_create(&tx,0);
    if(!error)*created=tx.feature;
    for(int i=0;i<3;i++)fd_ref_free(&ref[i]);
    fd_cleanup(&tx,error);
    return error;
}

static ProError path_point_on_curve(ProMdl model,ProAsmcomppath *consumer,ProAsmcomppath *provider,ProModelitem *curve)
{
    FdTxn tx={0};
    FdRef ref={.item=*curve};
    ProModelitem owner;
    ProElement points,point,name,array,entry;
    ProError error=sh_begin(&tx,model,L"EXTERNAL_MID",PRO_FEAT_DATUM_POINT);
    if(!error)error=ProSelectionFree(&tx.model_selection);
    if(!error)error=ProMdlToModelitem(model,&owner);
    if(!error)error=ProSelectionAlloc(consumer,&owner,&tx.model_selection);
    if(!error)error=ProSelectionAlloc(provider,curve,&ref.selection);
    if(!error)error=model_element(tx.root,PRO_E_DPOINT_TYPE,1,PRO_DPOINT_TYPE_GENERAL,NULL);
    if(!error)error=model_element(tx.root,PRO_E_DPOINT_POINTS_ARRAY,0,0,&points);
    if(!error)error=model_element(points,PRO_E_DPOINT_POINT,0,0,&point);
    if(!error)error=model_element(point,PRO_E_DPOINT_POINT_NAME,0,0,&name);
    if(!error)error=ProElementWstringSet(name,L"EXTERNAL_MID");
    if(!error)error=model_element(point,PRO_E_DPOINT_PLA_CONSTRAINTS,0,0,&array);
    if(!error)error=model_element(array,PRO_E_DPOINT_PLA_CONSTRAINT,0,0,&entry);
    if(!error)error=fd_ref_element(&tx,entry,PRO_E_DPOINT_PLA_CONSTR_REF,&ref);
    if(!error)error=model_element(entry,PRO_E_DPOINT_PLA_CONSTR_TYPE,1,PRO_DTMPNT_CONSTR_TYPE_ON,NULL);
    if(!error)error=model_element(point,PRO_E_DPOINT_DIM_CONSTRAINTS,0,0,&array);
    if(!error)error=model_element(array,PRO_E_DPOINT_DIM_CONSTRAINT,0,0,&entry);
    if(!error)error=fd_ref_element(&tx,entry,PRO_E_DPOINT_DIM_CONSTR_REF,&ref);
    if(!error)error=model_element(entry,PRO_E_DPOINT_DIM_CONSTR_TYPE,1,PRO_DTMPNT_CONSTR_TYPE_RATIO,NULL);
    if(!error)error=model_element(entry,PRO_E_DPOINT_DIM_CONSTR_VAL,2,0.5,NULL);
    if(!error)error=fd_create(&tx,0);
    fd_ref_free(&ref);
    fd_cleanup(&tx,error);
    return error;
}

static ProError path_trial(FILE *log)
{
    ProMdl root=NULL,sub=NULL,source=NULL,provider=NULL,result=NULL;
    ProMdlName root_name,sub_name,source_name,provider_name;
    const char *stage=NULL;
    ProError error=model_create_empty(3,root_name,&stage);
    if(!error)error=ProMdlnameInit(root_name,PRO_MDL_ASSEMBLY,&root);
    if(!error)error=model_create_empty(3,sub_name,&stage);
    if(!error)error=ProMdlnameInit(sub_name,PRO_MDL_ASSEMBLY,&sub);
    if(!error)error=path_workpiece(&source,source_name);
    if(!error)error=path_workpiece(&provider,provider_name);
    ProUnitsystem *systems=NULL;
    if(!error)error=ProMdlUnitsystemsCollect(provider,&systems);
    int systems_count=0,inches=-1;
    if(!error)error=ProArraySizeGet((ProArray)systems,&systems_count);
    for(int i=0;!error&&i<systems_count;i++){
        ProUnititem length;
        if(!ProUnitsystemUnitGet(&systems[i],PRO_UNITTYPE_LENGTH,&length)){
            fwprintf(log,L"UNIT CANDIDATE %ls / %ls\n",systems[i].name,length.name);
            if(!_wcsicmp(length.name,L"inch")||!_wcsicmp(length.name,L"in"))inches=i;
        }
    }
    if(!error&&inches<0)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProMdlPrincipalunitsystemSet(provider,&systems[inches],PRO_UNITCONVERT_SAME_SIZE,PRO_B_FALSE,PRO_VALUE_UNUSED);
    if(systems)ProArrayFree((ProArray *)&systems);
    double provider_mm=0;
    if(!error)error=qa_mm_per_unit(provider,&provider_mm);
    if(!error&&fabs(provider_mm-25.4)>0.0002)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"PROVIDER MILLIMETRES PER UNIT=%.8g\n",provider_mm);
    ProModelitem points[3];
    const wchar_t *labels[]={L"P_FIRST",L"P_SECOND",L"P_THIRD"};
    ProVector xyz[]={{10,10,12},{20,10,12},{10,30,12}};
    for(int i=0;!error&&i<3;i++){
        error=batch_step(provider,log,L"CREO_OP DATUM_POINT %ls %ls CSYS:PRT_CSYS_DEF %.12g %.12g %.12g",
            provider_name,labels[i],xyz[i][0],xyz[i][1],xyz[i][2]);
        ProFeature point;
        if(!error)error=ux_named_feature(provider,labels[i],&point);
        if(!error)error=fd_feature_geom(&point,PRO_POINT,&points[i]);
    }
    if(!error)error=batch_step(provider,log,L"CREO_OP SKETCH_CREATE %ls UPSTREAM_LINE PLANE:FRONT PLANE:RIGHT LINE 12 22 30 25",provider_name);
    ProFeature upstream_line;
    ProModelitem line_item;
    ProCurve line;
    if(!error)error=ux_named_feature(provider,L"UPSTREAM_LINE",&upstream_line);
    if(!error)error=ad_single_curve(&upstream_line,&line_item,&line);
    ProMdl root_provider=NULL;
    ProMdlName root_provider_name;
    if(!error)error=path_workpiece(&root_provider,root_provider_name);
    ProModelitem root_points[3];
    for(int i=0;!error&&i<3;i++){
        error=batch_step(root_provider,log,L"CREO_OP DATUM_POINT %ls %ls CSYS:PRT_CSYS_DEF %.12g %.12g %.12g",
            root_provider_name,labels[i],xyz[i][0],xyz[i][1],xyz[i][2]);
        ProFeature point;
        if(!error)error=ux_named_feature(root_provider,labels[i],&point);
        if(!error)error=fd_feature_geom(&point,PRO_POINT,&root_points[i]);
    }
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProMatrix rotation={{0,1,0,0},{-1,0,0,0},{0,0,1,0},{60,8,7,1}};
    ProMatrix duplicate={{1,0,0,0},{0,1,0,0},{0,0,1,0},{100,0,0,1}};
    ProAsmcomp up={0},own={0},up_holder={0},other={0},root_link={0};
    if(!error)error=ProAsmcompAssemble((ProAssembly)sub,(ProSolid)provider,identity,&up);
    if(!error)error=ProAsmcompAssemble((ProAssembly)root,(ProSolid)sub,rotation,&up_holder);
    if(!error)error=ProAsmcompAssemble((ProAssembly)root,(ProSolid)source,identity,&own);
    if(!error)error=ProAsmcompAssemble((ProAssembly)root,(ProSolid)source,duplicate,&other);
    if(!error)error=ProAsmcompAssemble((ProAssembly)root,(ProSolid)root_provider,identity,&root_link);
    int source_ids[]={own.id},up_ids[]={up_holder.id,up.id},root_ids[]={root_link.id};
    ProAsmcomppath use={0},from={0};
    if(!error)error=ProAsmcomppathInit((ProSolid)root,source_ids,1,&use);
    if(!error)error=ProAsmcomppathInit((ProSolid)root,up_ids,2,&from);
    ProAsmcomppath root_path={0};
    if(!error)error=ProAsmcomppathInit((ProSolid)root,root_ids,1,&root_path);
    ReplayExternalReference reference={.provider=from,.consumer=use};
    ProVector converted,actual_point,actual_converted;
    ProVector provider_input;
    if(!error){
        for(int i=0;i<3;i++)provider_input[i]=xyz[0][i]/provider_mm;
        error=replay_external_position(&reference,provider_input,converted,0);
    }
    const ProVector expected={50,18,19};
    if(!error&&!ad_near(converted,expected,0.0002))error=PRO_TK_INCOMPLETE;
    FdRef provider_point={.item=points[0]};
    if(!error)error=ad_point(&provider_point,actual_point);
    if(!error)error=replay_external_position(&reference,actual_point,actual_converted,0);
    fwprintf(log,L"ROTATED NESTED POINT=%d\n",error);
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)imitation_assembly_occurrences(source,(ProSolid)root,&out);
    if(!error&&(!wcsstr(report,L"2 个可见实例")||!wcsstr(report,L"(100, 0, 0)")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"OCCURRENCES=%d\n%ls\n",error,report);
    ProFeature feature;
    if(!error)error=path_plane(provider,&from,&root_path,root_points,L"UPSTREAM_PLANE",&feature);
    if(!error)error=path_plane(source,&use,&from,points,L"FROM_ASSEMBLY",&feature);
    if(!error)error=path_point_on_curve(source,&use,&from,&line_item);
    fwprintf(log,L"CONTEXTUAL DATUM CREATED=%d\n",error);
    int refs=0;
    if(!error)error=replay_external_state(source,&refs);
    report[0]=0;
    out=(PartText){report,_countof(report),0};
    if(!error)imitation_reference_report(source,&out);
    fwprintf(log,L"REFERENCE PREFLIGHT=%d COUNT=%d\n%ls\n",error,refs,report);
    if(!error&&(refs<4||!wcsstr(report,L"上游第 2 层")))error=PRO_TK_INCOMPLETE;
    if(!error&&(!wcsstr(report,L"状态 可用（1）")||wcsstr(report,L"当前无法直接绑定")))error=PRO_TK_INCOMPLETE;
    if(!error)error=batch_reopen(source,&result,log);
    if(!error)error=dv_no_external_dependencies(result);
    if(!error)error=ux_named_feature(result,L"FROM_ASSEMBLY",&feature);
    if(!error)error=ux_named_feature(result,L"EXTERNAL_MID",&feature);
    if(!error){
        FdRef point={0};
        ProVector recovered;
        error=fd_ref_resolve(result,L"POINT:BWTL_XPT_001",PRO_POINT,&point);
        if(!error)error=ad_point(&point,recovered);
        if(!error&&!ad_near(recovered,actual_converted,0.0002))error=PRO_TK_INCOMPLETE;
        fd_ref_free(&point);
    }
    fwprintf(log,L"LOCAL DATUM REBUILD=%d\n",error);
    return error;
}

static int path_case(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-external-paths.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    const ProError error=path_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLExternalPaths",path_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLExternalPaths`;");
}
