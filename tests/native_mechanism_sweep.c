#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static int motion_probe(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    ProMdl assembly=NULL,part=NULL,base=NULL;
    ProMdlName label=L"",part_name=L"JOINT_MODEL",base_name=L"JOINT_BASE";
    const char *stage="joint fixture";
    ProError error;
    PartCommand parsed;
    wchar_t report[PART_TEXT_MAX]=L"";
    ModelPlan stock={0,20,15,5,0,1};
    int window;
    ProAsmcomp anchor={0};
#define STEP(call) do{if((error=(call))){trace("joint fixture " #call,error);goto done;}}while(0)
    STEP(quick_seed(part_name,&part));
    STEP(model_build_internal(&stock,part_name,&stage,part,NULL,NULL));
    if(part_command_parse(L"CREO_OP DATUM_AXIS JOINT_MODEL PIN PLANES PLANE:RIGHT PLANE:TOP",&parsed)!=1){error=-2;goto done;}
    STEP(foundation_execute(&parsed,part,report,_countof(report)));
    STEP(quick_seed(base_name,&base));
    STEP(model_build_internal(&stock,base_name,&stage,base,NULL,NULL));
    STEP(foundation_execute(&parsed,base,report,_countof(report)));
    model_background_mode=0;
    STEP(model_create_empty(3,label,&stage));
    STEP(ProMdlnameInit(label,PRO_MDL_ASSEMBLY,&assembly));
    STEP(ProMdlWindowGet(assembly,&window));STEP(ProWindowCurrentSet(window));
    STEP(add_component(assembly,base,0,&anchor));
    {
        ProAsmcomp block={0};
        ProMatrix obstacle={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,12,1}};
        STEP(ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)part,obstacle,&block));
        wchar_t test[512];
        swprintf_s(test,_countof(test),L"CREO_OP ASM_MOTION_SWEEP %ls ID:%d ID:%d AXIS:PIN SLIDE 0 20 6 0",label,anchor.id,block.id);
        command(assembly,test,0,L"首次采样干涉位置 8 mm");
        swprintf_s(test,_countof(test),L"CREO_OP ASM_DOOR_SWEEP %ls ID:%d ID:%d AXIS:PIN 90 5 1",label,anchor.id,block.id);
        command(assembly,test,0,L"干涉 0 个");
        swprintf_s(test,_countof(test),L"CREO_OP ASM_TRAY_SWEEP %ls ID:%d ID:%d AXIS:PIN 20 6 0",label,anchor.id,block.id);
        command(assembly,test,0,L"首次采样干涉位置 8 mm");
        swprintf_s(test,_countof(test),L"CREO_OP ASM_TOOL_SWEEP %ls ID:%d ID:%d AXIS:PIN -20 6 1",label,anchor.id,block.id);
        command(assembly,test,0,L"干涉 0 个");
        trace("MECHANISM_SWEEP_CASES",cases);
        trace("MECHANISM_SWEEP_FAILURES",failures);
        goto done;
    }
done:

    trace("JOINT_PROBE_DONE",error);
    ProEngineerEnd();return 0;
#undef STEP
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId id;
    ProError e=ProCmdActionAdd("BWTLJointProbe",motion_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return e?e:ProMacroLoad(L"~ Command `BWTLJointProbe`;");
}
