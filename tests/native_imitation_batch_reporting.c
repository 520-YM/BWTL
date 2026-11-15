#define user_initialize imitation_batch_reporting_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProError batch_top_face(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    ProModelitem *item=data;
    ProModelitem candidate;
    ProVector origin,normal;
    ProError error=ProSurfaceToGeomitem((ProSolid)item->owner,surface,&candidate);
    if(error)return error;
    if(!fd_plane_data(&candidate,origin,normal)&&fabs(origin[2]-20)<0.0002&&normal[2]>0.999999){
        *item=candidate;
        return PRO_TK_E_FOUND;
    }
    return PRO_TK_NO_ERROR;
}

static ProError batch_feature_failure(FILE *log){
    ModelPlan plan={0,100,80,20,0,1};
    ProMdlName name=L"";
    ProMdl model=NULL;
    const char *stage=NULL;
    model_background_mode=1;
    ProError error=model_create(&plan,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
    wchar_t text[9000]=L"";
    PartText out={text,_countof(text),0};
    PartCommand command={0};
    ProPath resources,working,original,staged;
    if(!error){
        error=ProToolkitApplTextPathGet(resources);
        if(!error)error=ProDirectoryCurrentGet(working);
        if(!error){
            swprintf_s(original,_countof(original),L"%ls\\udf\\bwtl_boundary.gph.1",resources);
            swprintf_s(staged,_countof(staged),L"%ls\\SOURCE_TEMPLATE.gph",working);
            if(!CopyFileW(original,staged,TRUE))error=PRO_TK_CANT_WRITE;
        }
    }
    const wchar_t *setup[]={
        L"CREO_OP REUSE_UDF_SAVE %ls MY_TEMPLATE SOURCE_TEMPLATE",
        L"CREO_OP SKETCH_CREATE %ls UDF_FIRST PLANE:FRONT PLANE:RIGHT LINE 0 0 80 0",
        L"CREO_OP DATUM_PLANE %ls UDF_Z30 PLANE:FRONT 30",
        L"CREO_OP SKETCH_CREATE %ls UDF_SECOND PLANE:UDF_Z30 PLANE:RIGHT LINE 0 0 80 0",
        L"CREO_OP REUSE_UDF_PLACE %ls BATCH_UDF MY_TEMPLATE CURVE:UDF_FIRST CURVE:UDF_SECOND"
    };
    for(int i=0;!error&&i<_countof(setup);i++){
        wchar_t protocol[512];
        swprintf_s(protocol,_countof(protocol),setup[i],name);
        error=part_command_parse(protocol,&command)==1?
            foundation_execute(&command,model,text,_countof(text)):PRO_TK_BAD_INPUTS;
    }
    fwprintf(log,L"NEGATIVE CASE SETUP=%d\n%ls\n",error,text);
    ImitationOutcome outcome={0};
    ProPath delivered=L"";
    text[0]=0;
    ProError rejection=error?error:imitation_run_mode_impl(model,delivered,0,1,&outcome,&out);
    if(!error&&!rejection)error=PRO_TK_INCOMPLETE;
    if(!error&&(!outcome.replay.step||!outcome.replay.feature||delivered[0]))error=PRO_TK_INCOMPLETE;
    text[0]=0;
    out=(PartText){text,_countof(text),0};
    if(!error)imitation_batch_failure(L"测试源件",L"",model,&outcome,rejection,&out);
    if(!error&&(!wcsstr(text,L"BATCH_UDF")||!wcsstr(text,L"源特征")))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"NAMED FEATURE FAILURE=%d\n%ls\n",error,text);
    return error;
}

static int batch_reporting_case(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"imitation-batch-reporting.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProPath folder;
    wchar_t files[8192]={0},report[16000]=L"";
    PartText out={report,_countof(report),0};
    wchar_t mode[8];
    const int preview=GetEnvironmentVariableW(L"BWTL_BATCH_PREVIEW_ONLY",mode,_countof(mode))!=0;
    const wchar_t *leaves[]={L"missing_bwtl_fixture.prt.1",L"m26z0181011-v01.prt.93",L"222.prt.2",
                            L"m26z0181011-v01.prt.93"};
    ProError error=GetEnvironmentVariableW(L"BWTL_BATCH_TEST_INPUTS",folder,_countof(folder))?
        PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    if(!error){
        wcscpy_s(files,_countof(files),folder);
        wchar_t *next=files+wcslen(files)+1;
        for(int i=0;i<(preview?4:3);i++){
            wcscpy_s(next,_countof(files)-(next-files),leaves[i]);
            next+=wcslen(next)+1;
        }
        const ProError actual=preview?imitation_batch_preview_files(files,&out):imitation_batch_files(files,&out);
        if(actual!=PRO_TK_INCOMPLETE||imitation_batch_count!=(preview?0:1))
            error=PRO_TK_INCOMPLETE;
    }
    if(preview){
        if(!error&&(!wcsstr(report,L"外部参照或族表预检，2 个文件")||
           !wcsstr(report,L"M26Z0100000-V01.asm，2 个文件")||
           !wcsstr(report,L"M26Z0100000-V01_SKEL.prt，2 个文件")||
           !wcsstr(report,L"222.prt.2，通过预检")||
           !wcsstr(report,L"批量预检 1 / 4 个可尝试重建")))error=PRO_TK_INCOMPLETE;
        fwprintf(log,L"BATCH PREVIEW=%d\n%ls\nPASS=%d ERROR=%d\n",error,report,!error,error);
        fclose(log);
        ProEngineerEnd();
        return 0;
    }
    if(!error&&(!wcsstr(report,L"源文件缺失")||!wcsstr(report,L"外部参照或族表预检")||
       !wcsstr(report,L"M26Z0100000-V01.asm")||!wcsstr(report,L"M26Z0100000-V01_SKEL.prt")||
       !wcsstr(report,L"222.prt.2 →")||!wcsstr(report,L"批量通过 1 / 3 个")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"MIXED BATCH=%d\n%ls\n",error,report);
    if(!error)error=batch_feature_failure(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

#ifndef BWTL_BATCH_REPORTING_HELPER
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=imitation_batch_reporting_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLBatchReportingCase",batch_reporting_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLBatchReportingCase\x0060;");
}
#endif

