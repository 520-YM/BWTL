#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES
#include <ProSmtPunchQuilt.h>

static ProError finish_punch(ProMdl model,const wchar_t *name,FILE *log){
    FdTxn offset={0};
    FdRef reference={0};
    ProElement constraints=NULL,entry=NULL;
    ProError error=sh_begin(&offset,model,L"PUNCH_PL",PRO_FEAT_DATUM);
    if(!error)error=fd_ref_resolve(model,L"PLANE:FRONT",PRO_SURFACE,&reference);
    if(!error)error=model_element(offset.root,PRO_E_DTMPLN_CONSTRAINTS,0,0,&constraints);
    if(!error)error=model_element(constraints,PRO_E_DTMPLN_CONSTRAINT,0,0,&entry);
    if(!error)error=model_element(entry,PRO_E_DTMPLN_CONSTR_TYPE,1,PRO_DTMPLN_OFFS,NULL);
    if(!error)error=fd_ref_element(&offset,entry,PRO_E_DTMPLN_CONSTR_REF,&reference);
    if(!error)error=model_element(entry,PRO_E_DTMPLN_CONSTR_REF_OFFSET,2,-3,NULL);
    if(!error)error=fd_create(&offset,0);
    fd_ref_free(&reference);
    fd_cleanup(&offset,error);
    fwprintf(log,L"TOOL BASE PLANE=%d\n",error);
    if(!error)error=batch_step(model,log,L"CREO_OP SMT_SKETCH %ls TOOL_SK PLANE:PUNCH_PL PLANE:RIGHT CIRCLE 38 27 8",name);
    FdTxn tool={0};
    PartCommand cmd={0};
    wcscpy_s(cmd.arg[3],PART_TOKEN_MAX,L"TOOL_SK");
    wcscpy_s(cmd.arg[4],PART_TOKEN_MAX,L"11");
    wcscpy_s(cmd.arg[5],PART_TOKEN_MAX,L"NEG");
    if(!error)error=sh_begin(&tool,model,L"TOOL_RING",PRO_FEAT_DATUM_SURF);
    if(!error)error=sf_extrude(&tool,&cmd,SF_EXTRUDE);
    if(!error)error=fd_create(&tool,0);
    fwprintf(log,L"TOOL RING=%d\n",error);
    fd_cleanup(&tool,error);
    if(!error)error=sh_begin(&tool,model,L"TOOL_BOTTOM",PRO_FEAT_DATUM_SURF);
    if(!error)error=sf_extrude(&tool,&cmd,SF_FILL);
    if(!error)error=fd_create(&tool,0);
    fwprintf(log,L"TOOL CAP=%d\n",error);
    fd_cleanup(&tool,error);
    cmd.count=5;
    wcscpy_s(cmd.arg[3],PART_TOKEN_MAX,L"QUILT:TOOL_RING");
    wcscpy_s(cmd.arg[4],PART_TOKEN_MAX,L"QUILT:TOOL_BOTTOM");
    if(!error)error=sh_begin(&tool,model,L"TOOL",PRO_FEAT_DATUM_QUILT);
    if(!error)error=sf_merge(&tool,&cmd);
    if(!error)error=fd_create(&tool,0);
    fwprintf(log,L"TOOL MERGE=%d\n",error);
    fd_cleanup(&tool,error);
    return error;
}

typedef struct {ProMdl model;int count,id;double best;} FinishPunchQuilt;

static ProError finish_quilt_find(ProQuilt quilt,ProError status,ProAppData data){
    if(status)return status;
    FinishPunchQuilt *found=data;
    ProBoolean internal,backup;
    ProError error=ProQuiltIsInternal(quilt,&internal);
    if(!error)error=ProQuiltIsBackupgeometry(quilt,&backup);
    if(error||internal||backup)return error;
    SfArea area={0};
    error=ProQuiltSurfaceVisit(quilt,sf_surface_area,NULL,&area);
    if(error==PRO_TK_EMPTY||error==PRO_TK_E_NOT_FOUND||error==PRO_TK_OUT_OF_RANGE)return PRO_TK_NO_ERROR;
    if(error)return error;
    if(area.count>found->count||(area.count==found->count&&area.area>found->best)){
        found->count=area.count;
        found->best=area.area;
        error=ProQuiltIdGet(quilt,&found->id);
    }
    return error;
}

static ProError finish_quilt_stamp(ProMdl model,int side,FILE *log){
    FdTxn tx={0};
    FdRef quilt={0};
    ProElement fillet;
    ProError error=sh_begin(&tx,model,L"FORM_TOOL",PRO_FEAT_PUNCH_QUILT);
    FinishPunchQuilt found={.model=model};
    wchar_t token[32];
    if(!error)error=part_regenerate(model);
    if(!error)error=ProSolidQuiltVisit((ProSolid)model,finish_quilt_find,NULL,&found);
    fwprintf(log,L"TOOL QUILT ID %d SURFACES %d AREA %.8g\n",found.id,found.count,found.best);
    if(!error&&found.count<1)error=PRO_TK_INCOMPLETE;
    swprintf_s(token,_countof(token),L"QUILT:%d",found.id);
    if(!error)error=sf_ref(model,token,&quilt);
    fwprintf(log,L"PUNCH TOOL REFERENCE=%d ID=%d\n",error,quilt.item.id);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_FORM_PUNCH_QUILT,&quilt);
    if(!error)error=model_element(tx.root,PRO_E_FORM_PUNCH_SIDE,1,side,NULL);
    if(!error)error=model_element(tx.root,PRO_E_FORM_PUNCH_MATERIAL_SIDE,1,PRO_SMT_PUNCH_MAT_OUTSIDE,NULL);
    if(!error)error=model_element(tx.root,PRO_E_FORM_PUNCH_HIDE_QUILT,1,PRO_SMT_PUNCH_KEEP_ORIGINAL,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SMT_TRIM_FORM_SIDES,1,PRO_B_FALSE,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SMT_FILLET_INTERSECT,0,0,&fillet);
    if(!error)error=model_element(fillet,PRO_E_SMT_FILLET_RADIUS_USEFLAG,1,PRO_B_TRUE,NULL);
    if(!error)error=model_element(fillet,PRO_E_SMT_FILLET_RADIUS_SIDE,1,PRO_BEND_RAD_INSIDE,NULL);
    if(!error)error=model_element(fillet,PRO_E_SMT_FILLET_RADIUS_VALUE,2,2,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SMT_FILLET_QUILT,0,0,&fillet);
    if(!error)error=model_element(fillet,PRO_E_SMT_FILLET_RADIUS_USEFLAG,1,PRO_B_TRUE,NULL);
    if(!error)error=model_element(fillet,PRO_E_SMT_FILLET_RADIUS_SIDE,1,PRO_BEND_RAD_INSIDE,NULL);
    if(!error)error=model_element(fillet,PRO_E_SMT_FILLET_RADIUS_VALUE,2,2,NULL);
    if(!error)error=fd_create(&tx,0);
    fwprintf(log,L"PUNCH CREATE RESULT=%d\n",error);
    fwprintf(log,L"QUILT PUNCH SIDE=%d ERROR=%d\n",side,error);
    for(int i=0;tx.errors.error_list&&i<tx.errors.error_number;i++)fwprintf(log,L"ELEMENT %d ERROR %d\n",
        tx.errors.error_list[i].err_item_id,tx.errors.error_list[i].error);
    if(!error)error=part_regenerate(model);
    fd_ref_free(&quilt);
    fd_cleanup(&tx,error);
    return error;
}

static int finish_punch_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-punch.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    for(int i=0;i<2;i++){
        ProMdl source=NULL,result=NULL;
        ProMdlName name;
        ProError error=batch_sheet(&source,name,log);
        if(!error)error=finish_punch(source,name,log);
        if(!error)error=finish_quilt_stamp(source,i?PRO_SMT_SURF_FACE:PRO_SMT_SURF_OFFSET,log);
        if(!error)error=batch_reopen(source,&result,log);
        if(!error&&i==1)error=batch_edit(result,L"TOOL_RING",PRODIMTYPE_LINEAR,11,12,log);
        if(result)ProMdlErase(result);
        if(source)ProMdlErase(source);
        fwprintf(log,L"PUNCH CASE %d ERROR=%d PASS=%d\n",i,error,!error);
    }
    fclose(log);
    ProEngineerEnd();
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayPunch",finish_punch_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayPunch\x0060;");
}

