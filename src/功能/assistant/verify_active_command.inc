static void verify_sheetmetal_demo(ProMdl model,const wchar_t *name,int pinned){
    static wchar_t result[PART_TEXT_MAX],fallback[PART_TEXT_MAX];
    wchar_t protocol[512];
    PartCommand command={0};
    PartSnapshot snapshot={0};
    ProError error=PRO_TK_BAD_CONTEXT;
    result[0]=0;
    if(!pinned){
        smart_step(NULL,L"核对钣金样件窗口",error,L"独立钣金样件已创建，活动窗口或工作目录未能核对，尚未加工。",0,1,1);
        return;
    }
    swprintf_s(protocol,_countof(protocol),
        L"CREO_OP CASE_TRAY %ls DEMO_TRAY PLANE:FRONT PLANE:RIGHT 200 150 2 30 2 8 POS",name);
    if(part_command_parse(protocol,&command)==1&&!(error=part_snapshot_take_mode(model,&snapshot,0)))
        error=part_execute(&command,&snapshot,result,_countof(result));
    part_snapshot_clear(&snapshot);
    if(!error){
        wchar_t receipt[800];
        swprintf_s(receipt,_countof(receipt),L"已建好钣金托盘 %ls.prt。底板 200 × 150 mm、板厚 2 mm，四边 30 mm 高翻边，内弯 R2，两端各退让 8 mm。已核对原生板壁、实体和重生成，几何未自动保存。",name);
        smart_step(&command,L"钣金托盘验证",0,receipt,1,1,0);
        return;
    }
    trace("Sheetmetal demo tray result",error);
    int bodies;
    if(!cm_recovery_uncertain&&!sh_recovery_uncertain&&!part_health(model)&&
       !sd_body_count(model,&bodies)&&bodies==0&&
       smart_binding_set(model,model,request_snapshot.directory)){
        swprintf_s(protocol,_countof(protocol),
            L"CREO_OP CASE_U %ls DEMO_U PLANE:FRONT PLANE:RIGHT 200 146 2 30 2 POS",name);
        if(part_command_parse(protocol,&command)==1&&!(error=part_snapshot_take_mode(model,&snapshot,0))){
            error=part_execute(&command,&snapshot,fallback,_countof(fallback));

            if(!error){
                wchar_t receipt[800];
                swprintf_s(receipt,_countof(receipt),L"四边托盘尝试失败并核对了恢复。已自动改用两边翻边，建成钣金样件 %ls.prt。已核对实体和重生成，几何未自动保存。",name);
                part_snapshot_clear(&snapshot);
                smart_step(&command,L"钣金替代样件",0,receipt,1,1,0);
                return;
            }
            if(fallback[0])wcsncpy_s(result,_countof(result),fallback,_TRUNCATE);
        }
    }
    part_snapshot_clear(&snapshot);
    smart_step(NULL,L"钣金样件验证",error,result[0]?result:L"未生成可核对的钣金实体。请查看执行明细。",0,1,
        cm_recovery_uncertain||sh_recovery_uncertain);
}

static int verify_active_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active;ProMdlName name;wchar_t message[256];
    ProError error;
    if(!smart_active)return 0;
    error=ProMdlCurrentGet(&active);trace("Verify active model",error);
    if(!error)error=ProMdlMdlnameGet(active,name);
    if(!error && !_wcsicmp(name,expected_active_model)){
        smart_target_model=active;
        swprintf_s(message,256,L"当前活动模型：%ls。",name);
        int pinned=smart_binding_set(active,active,request_snapshot.directory);
        if(assembly_default_after_create(active,name,pinned))return 0;
        if(assembly_child_after_create(active,name,pinned))return 0;
        if(smart_sheetmetal_demo){verify_sheetmetal_demo(active,name,pinned);return 0;}
        const wchar_t *receipt=smart_new_model_result[0]?smart_new_model_result:message;
        if(pinned)chat_task_record_bound(active,active,name,request_snapshot.directory,L"新建模型",0,receipt,0,smart_steps+1);
        smart_step(NULL,L"新建模型",0,receipt,1,0,!pinned);
    }else{
        memset(&default_assembly_pending,0,sizeof(default_assembly_pending));
        assembly_child_pending=0;
        trace("Active model mismatch",error?error:-1);
        smart_step(NULL,L"核对新模型活动窗口",error?error:PRO_TK_BAD_CONTEXT,L"新模型已创建，活动窗口未能核对。",0,1,1);

    }
    return 0;
}
