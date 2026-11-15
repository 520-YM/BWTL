#define user_initialize replay_acceptance_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

typedef struct {ProDimension dim;int count;} ReplaySlot;

static ProError replay_slot_dimension(ProDimension *dim,ProError status,ProAppData data){
    if(status)return status;
    ReplaySlot *slot=data;
    ProDimensiontype type;
    double value;
    if(!ProDimensionTypeGet(dim,&type)&&type==PRODIMTYPE_DIAMETER&&
       !ProDimensionValueGet(dim,&value)&&fabs(value-7)<0.0001){
        slot->dim=*dim;slot->count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError replay_accept_slot(ProMdl source,ProMdl result,PartText *out){
    ReplayOrder original={0},fresh={0};
    ProError error=ProSolidFeatVisit((ProSolid)source,replay_read_order,NULL,&original);
    if(!error)error=ProSolidFeatVisit((ProSolid)result,replay_read_order,NULL,&fresh);
    if(error)return error;
    if(original.count!=6||fresh.count!=6)return PRO_TK_INCOMPLETE;
    for(int i=0;i<6;i++)if(original.step[i].id==fresh.step[i].id)return PRO_TK_INCOMPLETE;
    ProFeature last;
    ReplaySlot slot={0};
    ImitationShapeMetrics base;
    ProMassProperty before,after;
    error=ProFeatureInit((ProSolid)result,fresh.step[5].id,&last);
    if(!error)error=ProFeatureDimensionVisit(&last,replay_slot_dimension,NULL,&slot);
    if(!error&&slot.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_shape_read(source,&base);
    if(!error)error=qa_geometry_mass(result,&before);
    if(error)return error;
    error=ProDimensionValueSet(&slot.dim,7.5);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&after);
    if(!error&&(before.volume-after.volume<5||before.volume-after.volume>80))error=PRO_TK_INCOMPLETE;
    double actual=0;
    if(!error)error=ProDimensionValueGet(&slot.dim,&actual);
    if(!error&&fabs(actual-7.5)>0.0001)error=PRO_TK_INCOMPLETE;
    ProError restore=ProDimensionValueSet(&slot.dim,7);
    if(!restore)restore=part_regenerate(result);
    if(!restore)restore=imitation_shape_verify_mode(source,result,&base,0,out);
    if(error||restore)return error?error:restore;
    part_text_add(out,L"末道拉伸切除的内嵌槽宽从 7 改到 %.8g mm，实际去料 %.8g mm³，恢复 7 mm 后与原件一致。\n",
        actual,before.volume-after.volume);
    return PRO_TK_NO_ERROR;
}

static int replay_acceptance(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-acceptance.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProPath path,output;
    ProMdl source=NULL,result=NULL;
    wchar_t buffer[10000]=L"";
    PartText out={buffer,_countof(buffer),0};
    model_background_mode=1;
    ProError error=GetEnvironmentVariableW(L"BWTL_REPLAY_INPUT",path,_countof(path))?
        ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&source):PRO_TK_E_NOT_FOUND;
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"BRACKET=%d\n%ls\n",error,buffer);
    fflush(log);
    buffer[0]=0;
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=replay_accept_slot(source,result,&out);
    fwprintf(log,L"SLOT_EDIT_RESTORE=%d\n%ls\n",error,buffer);
    fflush(log);
    if(result)ProMdlErase(result);
    buffer[0]=0;
    ModelPlan cylinder={1,41,41,23,0,1};
    ProMdl solid=NULL;
    ProMdlName name;
    const char *stage="solid replay fixture";
    ProError solid_error=model_create(&cylinder,name,&stage);
    if(!solid_error)solid_error=ProMdlnameInit(name,PRO_MDL_PART,&solid);
    if(!solid_error)solid_error=imitation_run(solid,output,0,&out);
    fwprintf(log,L"SOLID=%d\n%ls\n",solid_error,buffer);
    buffer[0]=0;
    ProMdl multi=NULL;
    ProError flat_error=GetEnvironmentVariableW(L"BWTL_REPLAY_FLAT",path,_countof(path))?
        ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&multi):PRO_TK_E_NOT_FOUND;
    if(!flat_error)flat_error=imitation_run(multi,output,0,&out);
    fwprintf(log,L"TWO_EDGE_FLAT_WALL=%d\n%ls\n",flat_error,buffer);
    fwprintf(log,L"PASS=%d\n",!error&&!solid_error&&!flat_error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=replay_acceptance_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayAcceptance",replay_acceptance,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLReplayAcceptance`;");
}
