#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProMdl assembly;
static ProMdlName name;
static int layout_prepare(uiCmdCmdId id,uiCmdValue *value,void *data){
    const char *stage="layout fixture";
    int window;
    model_background_mode=0;
    ProError error=model_create_empty(3,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&assembly);
    if(!error)error=ProMdlWindowGet(assembly,&window);
    if(!error)error=ProWindowCurrentSet(window);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLLayoutTest`;");
    if(error){trace("LAYOUT_SETUP_FAILURE",error);ProEngineerEnd();}
    return 0;
}

static int run_layout(uiCmdCmdId id,uiCmdValue *value,void *data){
    const char *stage="layout fixture";
    ProMdl module=NULL,copy_part=NULL,old=NULL;
    ProMdlName part_name=L"LAYOUT_MODULE",copy_name=L"LAYOUT_TARGET",old_name=L"LAYOUT_PRIOR";
    ProAsmcomp placed={0},receiver={0},item={0};
    ProError error;
    wchar_t text[700];
    int skel_id=0;
    AxScan *scan=NULL;
    ModelPlan stock={0,22,15,5,0,1};
#define LP(expr) do{error=(expr);trace(#expr,error);if(error)goto done;}while(0)
#define TEST(reject,expected,format,...) LP((swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),command(assembly,text,reject,expected)))

    TEST(0,L"总装安装基准",L"ASM_LAYOUT_DATUM %ls MNT CSYS:ASM_CSYS PLANE:ASM_RIGHT PLANE:ASM_TOP PLANE:ASM_FRONT 10",name);
    TEST(0,L"Creo 原生骨架",L"ASM_LAYOUT_SKELETON %ls BWL_SKEL 100 80 120",name);
    TEST(0,L"构造主体",L"ASM_LAYOUT_ENVELOPE %ls BWL_ENVELOPE KEEPOUT 220 0 0 20 30 40",name);
    LP(ax_scan(assembly,0,&scan));
    for(int i=0;i<scan->count;i++)if(!_wcsicmp(scan->occurrences[i].name,L"BWL_SKEL"))skel_id=scan->occurrences[i].ids[0];
    free(scan);scan=NULL;
    if(!skel_id){error=PRO_TK_E_NOT_FOUND;goto done;}
    LP(quick_seed(part_name,&module));
    LP(model_build_internal(&stock,part_name,&stage,module,NULL,NULL));
    LP(add_component(assembly,module,0,&placed));
    TEST(0,L"Creo 坐标系重合约束",L"ASM_LAYOUT_LINK %ls ID:%d ID:%d CSYS:BWTL_WIDTH CSYS:PRT_CSYS_DEF",name,skel_id,placed.id);
    TEST(0,L"四个孔位基准点",L"ASM_LAYOUT_INTERFACE %ls ID:%d MOD CSYS:PRT_CSYS_DEF PLANE:RIGHT PLANE:TOP PLANE:FRONT 12 8 5",name,placed.id);
    LP(quick_seed(copy_name,&copy_part));
    LP(model_build_internal(&stock,copy_name,&stage,copy_part,NULL,NULL));
    LP(add_component(assembly,copy_part,160,&receiver));
    ProMdl skel=NULL;
    LP(ProAsmSkeletonGet((ProAssembly)assembly,&skel));
    LP(ax_layout_token_datum(skel,L"CREO_OP DATUM_CURVE A OUTLINE CSYS:PRT_CSYS_DEF 5 5 0 25 5 0"));
    LP(ax_layout_token_datum(skel,L"CREO_OP DATUM_CSYS A ROTATED CSYS:PRT_CSYS_DEF 12 15 18 0 0 30"));
    swprintf_s(text,_countof(text),L"CREO_OP ASM_LAYOUT_COPY %ls ID:%d ID:%d SHARED PLANE:BWTL_MOUNT_PLANE CSYS:ROTATED POINT:BWTL_H1 CURVE:OUTLINE",name,skel_id,receiver.id);
    LP(command(assembly,text,0,L"一次性复制"));
    ProMdl sub=NULL;
    ProAsmcomp sub_component={0};
    LP(ax_new_subassembly(L"LAYOUT_SUB",&sub));
    LP(add_component(assembly,sub,30,&sub_component));
    TEST(0,L"一次性复制",L"ASM_LAYOUT_COPY %ls ROOT ID:%d PUBLIC AXIS:MNT_AXIS CSYS:MNT_ORIGIN",name,sub_component.id);
    TEST(0,L"定位发生变化 1 个实例",L"ASM_LAYOUT_DIM %ls ID:%d BW_WIDTH 100 140",name,skel_id);
    ProMatrix pose;
    LP(ax_actual_pose(assembly,&placed,pose));
    check(fabs(pose[3][0]-140)<1e-5,"skeleton drove the installed module to X140");
    TEST(0,L"BWL_SKEL",L"ASM_LAYOUT_REFS %ls ID:%d",name,placed.id);
    TEST(0,L"参与循环的模型 0 个",L"ASM_LAYOUT_AUDIT %ls ALL",name);
    AxLayoutGraph *graph=calloc(1,sizeof(*graph));
    if(!graph){error=PRO_TK_OUT_OF_MEMORY;goto done;}
    graph->count=4;graph->edges[0][1]=graph->edges[1][2]=graph->edges[2][0]=graph->edges[2][3]=1;
    ax_layout_graph_close(graph);
    check(graph->edges[0][0]&&graph->edges[1][1]&&graph->edges[2][2]&&!graph->edges[3][3],"three-node cycle with non-cyclic tail");
    free(graph);
    LP(ProMdlnameCopy(assembly,old_name,&old));
    LP(ax_component_feature(old,receiver.id,&item));
    LP(ax_actual_pose(old,&item,pose));
    pose[3][0]+=25;
    LP(ax_set_position(old,&item,pose));
    TEST(0,L"位姿变化 1",L"ASM_LAYOUT_COMPARE %ls LAYOUT_PRIOR CSYS:MOD_ORIGIN 0.01",name);
done:
    free(scan);
    trace("LAYOUT_CASES",cases);
    trace("LAYOUT_FAILURES",failures);
    trace("LAYOUT_DONE",error);
    ProEngineerEnd();
    return 0;
#undef TEST
#undef LP
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t errors[80]){
    uiCmdCmdId id;
    ProError e=ProCmdActionAdd("BWTLLayoutTest",run_layout,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!e)e=ProCmdActionAdd("BWTLLayoutPrepare",layout_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return e?e:ProMacroLoad(L"~ Command `BWTLLayoutPrepare`;");
}