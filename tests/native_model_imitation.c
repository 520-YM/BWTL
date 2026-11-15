#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
#include "../src/功能/runtime_paths/runtime_isolation_verify.inc"
#include "../src/功能/model_imitation/imitation_run.inc"

static ProError imitation_variant(ProMdl source,ProPath output,PartText *report){
    ImitationGeometry m={0};
    ProError error=imitation_measure(source,&m);
    if(error)return error;
    m.origin[0]=7;m.origin[1]=-12;m.origin[2]=3;
    m.width=104;m.half_flat=65;m.ear_half=14;m.ear_end=79;
    m.ear_tangent=m.half_flat+2*m.radius+m.thickness;
    m.window[0][0]=-31;m.window[1][0]=33;
    m.hole_center[0][0]=-1.5;m.hole_center[1][0]=1.5;
    const char *stage="new variant";
    ProMdl model;
    ProMdlName name;
    wchar_t face[32];
    model_background_mode=1;
    error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
    if(!error)error=imitation_geometry_build(model,name,&m,report);
    if(!error)error=imitation_find_plane(model,2,m.origin[2]+m.thickness,1,face);
    if(!error)error=imitation_operation(model,report,0,L"CREO_OP SMT_CONVERT %ls VARIANT_SHEET %ls %.12g",name,face,m.thickness);
    if(!error)error=imitation_run(model,output,0,report);
    return error;
}

static ProError imitation_reopen(ProMdl original,const wchar_t *file,PartText *report){
    ProMdl reopened;
    ImitationGeometry m={0};
    ProError error=imitation_measure(original,&m);
    if(!error)error=ProMdlFiletypeLoad((wchar_t *)file,PRO_MDLFILE_PART,PRO_B_FALSE,&reopened);
    if(!error)error=part_regenerate(reopened);
    if(!error)error=part_health(reopened);
    if(!error)error=imitation_verify_shape(original,reopened,&m,report);
    if(!error)error=imitation_verify_bends(reopened,&m,report);
    const wchar_t *tokens[]={L"PLANE:DTM1",L"PLANE:DTM2"};
    for(int i=0;!error&&i<2;i++){
        ProVector a,b,c,d;
        error=imitation_plane(original,tokens[i],a,b);
        if(!error)error=imitation_plane(reopened,tokens[i],c,d);
        if(!error){
            ProVector delta;
            ad_sub(a,c,delta);
            if(!imitation_near(ad_dot(delta,b),0)||fabs(ad_dot(b,d))<0.999999)error=PRO_TK_GENERAL_ERROR;
        }
    }
    ProCsysdata a,b;
    if(!error)error=in_box_frame(original,L"CSYS:MWEX0181005_4",&a);
    if(!error)error=in_box_frame(reopened,L"CSYS:MWEX0181005_4",&b);
    if(!error&&(cm_distance(a.origin,b.origin)>0.0001||ad_dot(a.x_vector,b.x_vector)<0.999999||
       ad_dot(a.y_vector,b.y_vector)<0.999999||ad_dot(a.z_vector,b.z_vector)<0.999999))error=PRO_TK_GENERAL_ERROR;
    if(!error)part_text_add(report,L"REOPEN_PASS: 原生重生成、基准面和坐标系一致。\n");
    return error;
}

static int imitation_test(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    ProPath original,output;
    ProMdl source;
    FILE *file=_wfopen(L"imitation-result.txt",L"wt, ccs=UTF-8");
    if(!file){ProEngineerEnd();return 0;}
    ProError error=PRO_TK_BAD_CONTEXT;
    if(GetEnvironmentVariableW(L"BWTL_IMITATION_TEST_INPUT",original,_countof(original))){
        error=ProMdlFiletypeLoad(original,PRO_MDLFILE_PART,PRO_B_FALSE,&source);
        wchar_t report[8000]=L"";
        PartText text={report,_countof(report),0};
        ProPath reopen;
        if(!error&&GetEnvironmentVariableW(L"BWTL_IMITATION_TEST_OUTPUT",reopen,_countof(reopen))){
            error=imitation_reopen(source,reopen,&text);
            fwprintf(file,L"%ls\n",report);
        }else if(!error){
            ULONGLONG start=GetTickCount64();
            error=imitation_run(source,output,0,&text);
            fwprintf(file,L"%ls\n",report);
            if(!error)fwprintf(file,L"FILE=%ls\nELAPSED_MS=%llu\n",output,GetTickCount64()-start);
            ProMdl rejected;
            ProMdlName name;
            const char *stage="blank rejection";
            if(!error)error=model_create_empty(1,name,&stage);
            if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&rejected);
            report[0]=0;text.truncated=0;
            if(!error){
                ProError reject=imitation_run(rejected,output,0,&text);
                if(!reject||output[0])error=PRO_TK_GENERAL_ERROR;
                else fwprintf(file,L"BLANK_REJECTED=%d\n",reject);
            }
            report[0]=0;text.truncated=0;
            if(!error)error=imitation_variant(source,output,&text);
            fwprintf(file,L"%ls\n",report);
            if(!error)fwprintf(file,L"VARIANT_FILE=%ls\n",output);
        }
    }
    fwprintf(file,L"IMITATION_DONE=%d\n",error);
    fclose(file);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId command;
    if(!runtime_isolation_verify())return PRO_TK_BAD_CONTEXT;
    ProError error=ProCmdActionAdd("BWTLImitationTest",imitation_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLImitationTest`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
