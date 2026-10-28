#ifndef CREO_PART_SELFCHECK_H
#define CREO_PART_SELFCHECK_H
/* This self-check runs inside the already loaded, trusted native assistant,
   in its Creo command callback. It touches only a fresh private test part,
   never displays a window and never saves the test part. */
static int part_selfcheck_passed=0;
static ProError part_selfcheck(wchar_t *message,size_t capacity){
    static PartSnapshot test={0};
    static wchar_t result[PART_TEXT_MAX];
    ProMdl before=NULL,after=NULL,model=NULL;ProMdlName name=L"";
    ProModelitem item;ProParameter param;ProParamvalue value={0};
    ModelPlan plan={0,80,60,2,0,1};PartCommand cmd={0};const char *stage="start";
    int i,chosen=-1,prior_mode=model_background_mode;double scale,old,new_value;
    ProError e=PRO_TK_GENERAL_ERROR,erase_error;wchar_t protocol[512];FILE *log;
    log=runtime_open(L"part-selfcheck.log",L"wt, ccs=UTF-8");
#define SELF_CHECK(label,call) do{stage=label;e=(call);if(log){fwprintf(log,L"%hs: %d\n",stage,e);fflush(log);}if(e)goto cleanup;}while(0)
    SELF_CHECK("Read current model before private check",ProMdlCurrentGet(&before));
    model_background_mode=1;
    SELF_CHECK("Copy invisible empty template for in-place check",model_create_empty(1,name,&stage));
    SELF_CHECK("Resolve private part",ProMdlnameInit(name,PRO_MDL_PART,&model));
    {int restored=0;ProFeature feature={0};
        SELF_CHECK("Extrude inside the existing private empty part",model_build_internal(&plan,name,&stage,model,&restored,&feature));
    }
    SELF_CHECK("Read real private model data",part_snapshot_take(model,&test));
    scale=test.mm_per_native;
    if(!test.has_volume||!part_close(test.volume,9600/(scale*scale*scale))||test.features<5||test.dimensions<3){stage="Verify initial part geometry and dimensions";e=PRO_TK_GENERAL_ERROR;goto cleanup;}
    for(i=0;i<test.dimensions;i++)if(!test.dimension[i].related&&!test.dimension[i].negative&&part_close(test.dimension[i].value,80/scale)){chosen=i;break;}
    if(chosen<0){stage="Locate editable width";e=PRO_TK_E_NOT_FOUND;goto cleanup;}
    old=test.dimension[chosen].value;new_value=100/scale;
    swprintf_s(protocol,512,L"CREO_OP DIMS %ls %d %ls %.17g %.17g",name,test.dimension[chosen].feature,test.dimension[chosen].symbol,old,new_value);
    if(part_command_parse(protocol,&cmd)!=1){stage="Parse self-check dimension command";e=PRO_TK_BAD_INPUTS;goto cleanup;}
    SELF_CHECK("Change width through actual production backend",part_change_dimensions(&cmd,&test,model,result,PART_TEXT_MAX));
    if(!part_change_dimensions(&cmd,&test,model,result,PART_TEXT_MAX)){stage="Stale old value was not rejected";e=PRO_TK_GENERAL_ERROR;goto cleanup;}
    SELF_CHECK("Read back changed solid",part_snapshot_take(model,&test));
    if(!test.has_volume||!part_close(test.volume,12000/(scale*scale*scale))){stage="Check changed volume";e=PRO_TK_GENERAL_ERROR;goto cleanup;}
    chosen=-1;
    for(i=0;i<test.dimensions;i++)if(!test.dimension[i].related&&!test.dimension[i].negative&&part_close(test.dimension[i].value,100/scale)){chosen=i;break;}
    if(chosen<0){stage="Locate changed private width";e=PRO_TK_E_NOT_FOUND;goto cleanup;}
    swprintf_s(protocol,512,L"CREO_OP DIMS %ls %d %ls %.17g 0",name,test.dimension[chosen].feature,test.dimension[chosen].symbol,100/scale);
    if(part_command_parse(protocol,&cmd)!=1){stage="Parse invalid geometry check";e=PRO_TK_BAD_INPUTS;goto cleanup;}
    if(!part_change_dimensions(&cmd,&test,model,result,PART_TEXT_MAX)){stage="Degenerate private solid was incorrectly accepted";e=PRO_TK_GENERAL_ERROR;goto cleanup;}
    SELF_CHECK("Read private part after rejected degenerate edit",part_snapshot_take(model,&test));
    if(!test.has_volume||!part_close(test.volume,12000/(scale*scale*scale))||part_health(model)){stage="Verify failed-edit restoration";e=PRO_TK_GENERAL_ERROR;goto cleanup;}
    SELF_CHECK("Resolve private model parameter owner",ProMdlToModelitem(model,&item));
    value.type=PRO_PARAM_STRING;wcscpy_s(value.value.s_val,PRO_LINE_SIZE,L"测试原值");
    SELF_CHECK("Create private Chinese parameter",ProParameterWithUnitsCreate(&item,L"CODEX_PRIVATE_CHECK",&value,NULL,&param));
    SELF_CHECK("Capture private parameter",part_snapshot_take(model,&test));
    swprintf_s(protocol,512,L"CREO_OP PARAM %ls CODEX_PRIVATE_CHECK STRING \"测试原值\" \"测试新值\"",name);
    if(part_command_parse(protocol,&cmd)!=1){stage="Parse parameter self-check";e=PRO_TK_BAD_INPUTS;goto cleanup;}
    SELF_CHECK("Change private parameter through production backend",part_change_parameter(&cmd,&test,model,result,PART_TEXT_MAX));
    if(!part_change_parameter(&cmd,&test,model,result,PART_TEXT_MAX)){stage="Stale parameter was not rejected";e=PRO_TK_GENERAL_ERROR;goto cleanup;}
    SELF_CHECK("Read private Chinese parameter",ProParameterValueWithUnitsGet(&param,&value,NULL));
    if(wcscmp(value.value.s_val,L"测试新值")){stage="Chinese parameter readback mismatch";e=PRO_TK_GENERAL_ERROR;goto cleanup;}
    SELF_CHECK("Read current model after private check",ProMdlCurrentGet(&after));
    if(before!=after){stage="Private check changed active model";e=PRO_TK_BAD_CONTEXT;goto cleanup;}
    stage="Core dimension and Chinese parameter self-check passed";e=0;
cleanup:
    model_background_mode=prior_mode;part_snapshot_clear(&test);
    if(model){erase_error=ProMdlErase(model);if(log)fwprintf(log,L"Erase unsaved private model: %d\n",erase_error);}
    if(log){fwprintf(log,L"FINAL: %hs = %d\n",stage,e);fclose(log);}
    if(!e){part_selfcheck_passed=1;wcscpy_s(message,capacity,L"本次运行的基础写入自检通过：真实模板零件创建、尺寸修改与体积核验、退化尺寸拒绝/恢复、中文参数修改、旧值失配拒绝及活动模型保持。孔、圆角、钣金壁等后续功能不在此验证范围内。");}
    else swprintf_s(message,capacity,L"基础写入自检未通过，阶段%hs，错误码%d。当前零件的写入指令未执行；请查看 part-selfcheck.log。",stage,e);
    return e;
#undef SELF_CHECK
}
#endif
