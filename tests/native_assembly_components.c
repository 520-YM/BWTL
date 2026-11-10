#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProError component_fixture(ProMdlName name,double width,ProMdl *model){
    ModelPlan stock={0,width,30,12,0,1};
    const char *stage="component stock";
    ProError error=quick_seed(name,model);
    return error?error:model_build_internal(&stock,name,&stage,*model,NULL,NULL);
}

static ProError component_insert(ProMdl root,ProMdl model,double x,double y,double z,ProAsmcomp *feature,wchar_t id[32]){
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,y,z,1}};
    ProError error=ProAsmcompAssemble((ProAssembly)root,(ProSolid)model,pose,feature);
    if(!error&&!ax_path_label(&feature->id,1,id,32))error=PRO_TK_GENERAL_ERROR;
    return error;
}

static int component_run(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    ProMdl root=NULL,base=NULL,part=NULL,new_part=NULL,sub=NULL,other=NULL;
    ProMdlName root_name,base_name=L"MG_BASE",part_name=L"MG_BRACKET",new_name=L"MG_UPGRADE",sub_name=L"MG_OLD_SUB";
    ProAsmcomp a,b,target,sibling,rotor,inner;
    ProMatrix pose,original,actual;
    wchar_t aid[32],bid[32],tid[32],sid[32],rid[32],inner_id[32],text[1024];
    const char *stage="component setup";
    double mm;
    int window;
    ProError error;
#define SETUP(call) do{if((error=(call))){trace("component setup " #call,error);check(0,"fixture setup");goto done;}}while(0)
#define DO(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),command(root,text,reject,expected))
    SETUP(component_fixture(base_name,100,&base));
    SETUP(component_fixture(part_name,25,&part));
    SETUP(component_fixture(new_name,40,&new_part));
    swprintf_s(text,_countof(text),L"CREO_OP DATUM_PLANE %ls MG_TEMP PLANE:FRONT 6",base_name);
    SETUP(run(base,text,0));
    SETUP(ax_new_subassembly(sub_name,&sub));
    SETUP(component_insert(sub,part,0,0,0,&inner,inner_id));
    SETUP(part_regenerate(sub));
    SETUP(ProMdlSave(sub));
    SETUP(ProMdlErase(sub));
    model_background_mode=0;
    SETUP(model_create_empty(3,root_name,&stage));
    SETUP(ProMdlnameInit(root_name,PRO_MDL_ASSEMBLY,&root));
    SETUP(ProMdlWindowGet(root,&window));
    SETUP(ProWindowCurrentSet(window));
    SETUP(qa_mm_per_unit(root,&mm));
    DO(0,L"含 1 个下级实例",L"ASM_INSERT_ASM %ls MG_OLD_SUB 0 20 0",root_name);
    DO(0,L"已新建并装入子装配",L"ASM_NEW_ASM %ls MG_NEW_SUB 50 30 0",root_name);
    DO(1,NULL,L"ASM_NEW_ASM %ls MG_NEW_SUB 50 30 0",root_name);
    DO(1,NULL,L"ASM_INSERT_ASM %ls %ls 0 0 0",root_name,root_name);
    SETUP(component_insert(root,base,0,0,0,&a,aid));
    SETUP(component_insert(root,base,0,0,40,&b,bid));
    SETUP(component_insert(root,part,20,10,24,&target,tid));
    SETUP(component_insert(root,part,40,0,34,&sibling,sid));
    SETUP(component_insert(root,part,10,0,0,&rotor,rid));
    SETUP(part_regenerate(root));
    SETUP(ax_actual_pose(root,&sibling,original));
    DO(0,L"已完成平面偏距",L"ASM_OFFSET %ls %ls %ls PLANE:FRONT PLANE:FRONT 10",root_name,tid,aid);
    DO(0,L"重新绑定",L"ASM_COPY_COMPONENT %ls %ls %ls %ls",root_name,tid,aid,bid);
    AxScan *scan=NULL;
    SETUP(ax_scan(root,1,&scan));
    int found=0;
    for(int i=0;i<scan->count;i++){
        AxOccurrence *entry=&scan->occurrences[i];
        if(entry->depth!=1||entry->model!=part||entry->ids[0]<=rotor.id)continue;
        ProAsmcomp duplicate;
        int n=0;
        if(!ax_component_feature(root,entry->ids[0],&duplicate)&&!ax_actual_pose(root,&duplicate,pose)&&
           !ax_constraint_count(&duplicate,&n)&&n==1&&fabs(pose[3][2]*mm-50)<0.001)found++;
    }
    free(scan);
    check(found==1,"copy stays on second instance's plane at correct offset");
    DO(0,L"已改绑参照",L"ASM_EDIT_REF %ls %ls 1 BASE %ls PLANE:FRONT",root_name,tid,bid);
    SETUP(ax_actual_pose(root,&target,pose));
    check(fabs(pose[3][2]*mm-50)<0.001,"reference edit changed the target relative to the specified occurrence");
    DO(0,L"已修复参照",L"ASM_REPAIR_REF %ls %ls 1 BASE %ls PLANE:FRONT",root_name,tid,aid);
    DO(0,L"候选参照",L"ASM_REPAIR_REF %ls %ls 1 BASE ROOT AUTO",root_name,tid);
    DO(0,L"已改绑参照",L"ASM_EDIT_REF %ls %ls 1 BASE %ls PLANE:FRONT",root_name,tid,aid);
    SETUP(ax_actual_pose(root,&target,pose));
    check(fabs(pose[3][2]*mm-10)<0.001,"explicit reference repair rebinds the row");
    DO(0,L"位置和 1 条定位约束",L"ASM_REPLACE_COMPONENT %ls %ls %ls %ls",root_name,tid,part_name,new_name);
    int old_id=target.id;
    SETUP(ax_scan(root,1,&scan));
    int replacement_id=0;
    for(int i=0;i<scan->count;i++)if(scan->occurrences[i].depth==1&&scan->occurrences[i].model==new_part)
        replacement_id=scan->occurrences[i].ids[0];
    free(scan);
    check(replacement_id&&ax_component_feature(root,old_id,&target)!=0,"old occurrence removed only after verified replacement");
    if(replacement_id){
        ax_path_label(&replacement_id,1,tid,_countof(tid));
        DO(0,L"位置和 1 条定位约束",L"ASM_REPLACE_COMPONENT %ls %ls %ls %ls",root_name,tid,new_name,part_name);
    }else check(0,"repeat replacement fixture exists");
    SETUP(ax_actual_pose(root,&sibling,actual));
    check(ax_same_pose(original,actual,mm),"unmodified same-model occurrence remains in place");
    DO(0,L"已旋转",L"ASM_ROTATE_COMPONENT %ls %ls Z 90 0 0 0",root_name,rid);
    SETUP(ax_actual_pose(root,&rotor,actual));
    check(fabs(actual[3][0])<1e-6&&fabs(actual[3][1]*mm-10)<1e-6&&fabs(actual[0][1]-1)<1e-6,"rotate about world pivot");
    DO(0,L"翻转 180",L"ASM_FLIP_COMPONENT %ls %ls X",root_name,rid);
    SETUP(ax_actual_pose(root,&rotor,actual));
    check(fabs(actual[3][1]*mm-10)<1e-6&&fabs(actual[2][2]+1)<1e-6,"flip around own axis keeps origin");
    DO(0,L"已完成固定",L"ASM_FIX %ls %ls",root_name,rid);
    SETUP(ax_actual_pose(root,&rotor,original));
    DO(0,L"解除固定并保持原位",L"ASM_UNFIX_COMPONENT %ls %ls",root_name,rid);
    SETUP(ax_actual_pose(root,&rotor,actual));
    check(ax_same_pose(original,actual,mm)&&!ax_packaged(&rotor),"unfix keeps the current pose");
    DO(0,L"依赖特征",L"ASM_DELETE_COMPONENT %ls %ls %ls CHECK",root_name,aid,base_name);
    DO(1,L"依赖特征",L"ASM_DELETE_COMPONENT %ls %ls %ls DELETE",root_name,aid,base_name);
    DO(0,L"已删除实例",L"ASM_DELETE_COMPONENT %ls %ls %ls DELETE",root_name,sid,part_name);
    check(ax_component_feature(root,sibling.id,&sibling)!=0&&!part_health(root),"only selected leaf deleted and assembly healthy");
    ProFeature removed_datum;
    SETUP(ux_named_feature(base,L"MG_TEMP",&removed_datum));
    DO(0,L"已完成平面偏距",L"ASM_OFFSET %ls %ls %ls PLANE:MG_TEMP PLANE:FRONT 10",root_name,rid,aid);
    SETUP(model_remove_feature((ProSolid)base,removed_datum.id));
    check(ux_named_feature(base,L"MG_TEMP",&removed_datum)!=0,"source datum actually removed in isolated fixture");
    DO(0,L"已修复参照",L"ASM_REPAIR_REF %ls %ls 1 BASE %ls PLANE:FRONT",root_name,rid,aid);
    SETUP(ax_actual_pose(root,&rotor,actual));
    check(fabs(actual[3][2]*mm-10)<0.001&&!part_health(root),"broken-reference fixture is healthy after repair");
    check(!ax_recovery_uncertain,"no unverified recovery");
done:
    trace("COMPONENT_CASES",cases);
    trace("COMPONENT_FAILURES",failures);
    trace("COMPONENT_DONE",1);
    ProEngineerEnd();
    return 0;
#undef SETUP
#undef DO
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH]=L"";
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLComponentProbe",component_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLComponentProbe`;");
}
