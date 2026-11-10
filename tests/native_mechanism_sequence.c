#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProMdl joint_fixture;
static ProMdlName label,children[3]={L"JOINT_MODEL",L"JOINT_SLIDE",L"JOINT_CYLINDER"};
static int joint_phase,joint_ids[3],anchor_ids[3];

static int production_joint(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    wchar_t test[512];
    int reject=0;
    const wchar_t *expected=L"核对 Creo 实际姿态";
    if(joint_phase<3){
        const wchar_t *verbs[]={L"ASM_JOINT_PIN",L"ASM_JOINT_SLIDER",L"ASM_JOINT_CYLINDER"};
        swprintf_s(test,_countof(test),L"CREO_OP %ls %ls ID:%d %ls AXIS:PIN AXIS:PIN PLANE:RIGHT PLANE:RIGHT PLANE:FRONT PLANE:FRONT",
            verbs[joint_phase],label,anchor_ids[joint_phase],children[joint_phase]);
    }else if(joint_phase==3)
        swprintf_s(test,_countof(test),L"CREO_OP ASM_ROTATION_LIMIT %ls ID:%d 0 80",label,joint_ids[0]);
    else if(joint_phase==4)
        swprintf_s(test,_countof(test),L"CREO_OP ASM_TRAVEL_LIMIT %ls ID:%d 0 20",label,joint_ids[1]);
    else if(joint_phase==5){
        swprintf_s(test,_countof(test),L"CREO_OP ASM_MOTION_PRESET %ls ID:%d SERVICE",label,joint_ids[0]);
        expected=L"旋转当前位置 40 度";
    }else if(joint_phase==6){
        swprintf_s(test,_countof(test),L"CREO_OP ASM_MOTION_PRESET %ls ID:%d OPEN",label,joint_ids[1]);
        expected=L"滑动当前位置 20 mm";
    }else if(joint_phase==7)
        swprintf_s(test,_countof(test),L"CREO_OP ASM_ROTATION_LIMIT %ls ID:%d -20 60",label,joint_ids[2]);
    else if(joint_phase==8)
        swprintf_s(test,_countof(test),L"CREO_OP ASM_TRAVEL_LIMIT %ls ID:%d 0 10",label,joint_ids[2]);
    else if(joint_phase==9){
        swprintf_s(test,_countof(test),L"CREO_OP ASM_MOTION_PRESET %ls ID:%d OPEN",label,joint_ids[2]);
        expected=L"旋转当前位置 60 度，限位 -20 至 60 度。\n滑动当前位置 10 mm";
    }else if(joint_phase==10){
        swprintf_s(test,_countof(test),L"CREO_OP ASM_ROTATION_LIMIT %ls ID:%d 0 90",label,joint_ids[1]);
        reject=1;expected=L"没有此运动轴";
    }else if(joint_phase==11){
        swprintf_s(test,_countof(test),L"CREO_OP ASM_TRAVEL_LIMIT %ls ID:%d 0 50",label,joint_ids[0]);
        reject=1;expected=L"没有此运动轴";
    }else{
        swprintf_s(test,_countof(test),L"CREO_OP ASM_MOTION_PRESET %ls ID:%d CLOSED",label,joint_ids[2]);
        expected=L"旋转当前位置 -20 度，限位 -20 至 60 度。\n滑动当前位置 0 mm";
    }
    trace("mechanism production phase",joint_phase);
    ProError error=command(joint_fixture,test,reject,expected);
    if(reject&&error)error=PRO_TK_NO_ERROR;
    AxScan *list=NULL;
    if(!error&&joint_phase<3)error=ax_scan(joint_fixture,1,&list);
    if(!error&&list){
        int last=0;
        for(int i=0;i<list->count;i++)
            if(list->occurrences[i].depth==1&&list->occurrences[i].ids[0]>last)last=list->occurrences[i].ids[0];
        joint_ids[joint_phase]=last;
    }
    free(list);
    if(error||++joint_phase>=13){
        trace("MECHANISM_PRODUCTION_CASES",cases);
        trace("MECHANISM_PRODUCTION_FAILURES",failures);
        ProEngineerEnd();
    }else ProMacroLoad(L"~ Command `BWTLJointProduction`;");
    return 0;
}
static int motion_probe(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    ProMdl part=NULL,base=NULL;
    ProMdlName part_name=L"JOINT_MODEL",base_name=L"JOINT_BASE";
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
    for(int i=1;i<3;i++){
        STEP(quick_seed(children[i],&part));
        STEP(model_build_internal(&stock,children[i],&stage,part,NULL,NULL));
        STEP(foundation_execute(&parsed,part,report,_countof(report)));
    }
    STEP(quick_seed(base_name,&base));
    STEP(model_build_internal(&stock,base_name,&stage,base,NULL,NULL));
    STEP(foundation_execute(&parsed,base,report,_countof(report)));
    model_background_mode=0;
    STEP(model_create_empty(3,label,&stage));
    STEP(ProMdlnameInit(label,PRO_MDL_ASSEMBLY,&joint_fixture));
    STEP(ProMdlWindowGet(joint_fixture,&window));STEP(ProWindowCurrentSet(window));
    STEP(add_component(joint_fixture,base,0,&anchor));


    anchor_ids[0]=anchor.id;
    STEP(add_component(joint_fixture,base,80,&anchor));anchor_ids[1]=anchor.id;
    STEP(add_component(joint_fixture,base,160,&anchor));anchor_ids[2]=anchor.id;
    for(int i=0;i<3;i++){
        wchar_t fix[256];
        swprintf_s(fix,_countof(fix),L"CREO_OP ASM_FIX %ls ID:%d",label,anchor_ids[i]);
        STEP(command(joint_fixture,fix,0,NULL));
    }
    STEP(ProMacroLoad(L"~ Command `BWTLJointProduction`;"));
    return 0;
done:
    trace("MECHANISM_PRODUCTION_SETUP_ERROR",error);
    ProEngineerEnd();return 0;
#undef STEP
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId id;
    ProError e=ProCmdActionAdd("BWTLJointProbe",motion_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!e)e=ProCmdActionAdd("BWTLJointProduction",production_joint,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return e?e:ProMacroLoad(L"~ Command `BWTLJointProbe`;");
}