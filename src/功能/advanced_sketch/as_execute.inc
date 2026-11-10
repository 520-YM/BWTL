static ProError as_capture_strong(ProMdl model,const wchar_t *name,FdDimValue values[256],int *count){
    FdSketch probe={0};
    ProError e=fd_sketch_open(model,name,&probe,0);
    int i,total=0;
    *count=0;
    if(!e)e=fd_dimensions_capture(probe.section,values,&total);
    fd_sketch_free(&probe);
    if(!e)for(i=0;i<total;i++){
        ProError strength;
        if((e=fd_sketch_open(model,name,&probe,0)))break;
        strength=ProSecdimStrengthen(probe.section,values[i].id);
        fd_sketch_free(&probe);
        if(strength==PRO_TK_E_FOUND)values[(*count)++]=values[i];
        else if(strength&&strength!=PRO_TK_INVALID_TYPE){e=strength;break;}
    }
    fd_sketch_free(&probe);
    return e;
}

static int as_same_dimensions(ProSection section,const FdDimValue *expected,int count){
    int i;
    for(i=0;i<count;i++){
        double value;
        ProSecdimType type;
        if(ProSecdimValueGet(section,expected[i].id,&value)||ProSecdimTypeGet(section,expected[i].id,&type)||
           !part_close(value,expected[i].value)||type!=expected[i].type)return 0;
    }
    return 1;
}

static ProError as_execute(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    const wchar_t *labels[]={L"",L"等距偏移",L"草绘镜像",L"修剪或延伸",L"草绘转角",L"整条边链关联投影",L"轮廓检查",L"小间隙修复"};
    const int kind=as_edit_kind(cmd->arg[0]);
    const wchar_t *name=cmd->arg[2];
    FdSketch s={0},check={0};
    AsSnapshot original={0},expected={0},actual={0};
    AsAudit audit;
    AsEdit edit={0};
    FdDimValue strong[256];
    ProErrorlist errors={0};
    PartText details={report,capacity,0};
    ProError e;
    const char *stage="open advanced sketch";
    double scale=1,tol;
    int i,strong_count=0,committed=0,restored=1;
#define AS_STEP(label,call) do{stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    AS_STEP("open advanced sketch",fd_sketch_open(model,name,&s,kind!=AS_CHECK));
    AS_STEP("resolve sketch units",as_model_scale(model,&scale));
    tol=0.001*scale;
    if(kind==AS_CHECK||kind==AS_REPAIR){
        if(kind==AS_CHECK)part_text_add(&details,L"草绘 %ls 轮廓检查\n",name);
        AS_STEP("inspect gaps, repeats and crossings",as_audit(s.section,scale,&audit,kind==AS_CHECK?&details:NULL));
        if(kind==AS_CHECK){
            part_text_add(&details,L"%ls。轮廓图元 %d，断点 %d，分叉端点 %d，重复或重叠 %d，自交点 %d，未分类图元 %d。",as_closed(&audit)?L"闭合检查通过":L"轮廓需要检查",audit.curves,audit.gaps,audit.branches,audit.duplicates,audit.crossings,audit.unsupported);
            goto done;
        }
    }
    AS_STEP("capture original sketch geometry",as_snapshot(s.original,&original));
    AS_STEP("capture existing strong dimensions",as_capture_strong(model,name,strong,&strong_count));
    AS_STEP("prepare copied section for editing",ProSectionIntentManagerModeSet(s.section,PRO_B_FALSE));
    switch(kind){
    case AS_OFFSET:AS_STEP("create exact offset sketch entities",as_offset(s.section,cmd,scale,&edit));break;
    case AS_MIRROR:AS_STEP("reflect selected sketch entities",as_mirror(s.section,cmd,scale,&edit));break;
    case AS_TRIM:AS_STEP("trim or extend selected line",as_trim(s.section,cmd,scale,&edit));break;
    case AS_CORNER:AS_STEP("create round or chamfer at shared corner",as_corner(s.section,cmd,scale,&edit));break;
    case AS_CHAIN:AS_STEP("create connected native projections",as_chain(model,&s,cmd,scale,&edit));break;
    case AS_REPAIR:AS_STEP("pair uniquely matching straight-line endpoints",as_repair(s.section,&audit,as_arg(cmd,3)*scale,scale,&edit));break;
    default:e=PRO_TK_BAD_INPUTS;goto done;
    }
    AS_STEP("capture expected geometry on working copy",as_snapshot(s.section,&expected));
    AS_STEP("solve advanced sketch on working copy",fd_section_solve(s.section,scale));
    AS_STEP("read solved working copy",as_snapshot(s.section,&actual));
    if(!as_snapshot_match(&expected,&actual,NULL,tol)||!as_snapshot_match(&original,&actual,&edit,tol)||
       !as_same_dimensions(s.section,strong,strong_count)){
        stage="verify solver preserved the requested geometry and strong dimensions";e=PRO_TK_GENERAL_ERROR;goto done;
    }
    if(kind==AS_REPAIR){
        AS_STEP("verify repaired loop closes on working copy",as_audit(s.section,scale,&audit,NULL));
        if(!as_closed(&audit)){e=PRO_TK_BAD_CONTEXT;goto done;}
    }
    AS_STEP("mark copied sketch geometry for redefine",sd_mark_section(&s));
    s.options[0]=PRO_FEAT_CR_NO_OPTS;committed=1;
    AS_STEP("commit advanced sketch",ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors));
    AS_STEP("regenerate advanced sketch and dependents",part_regenerate(model));
    AS_STEP("read committed advanced sketch",fd_sketch_open(model,name,&check,0));
    AS_STEP("capture committed advanced sketch",as_snapshot(check.section,&actual));
    if(!as_snapshot_match(&expected,&actual,NULL,tol)||!as_same_dimensions(check.section,strong,strong_count)){
        stage="verify committed geometry and strong dimensions";e=PRO_TK_GENERAL_ERROR;goto done;
    }
    if(kind==AS_CHAIN)AS_STEP("verify every native projection association",as_verify_chain(model,&check,&edit,&actual,tol));
    if(kind==AS_REPAIR){
        AS_STEP("verify repaired loop closes in model",as_audit(check.section,scale,&audit,NULL));
        if(!as_closed(&audit)){e=PRO_TK_BAD_CONTEXT;goto done;}
    }
    if(kind==AS_REPAIR)swprintf_s(report,capacity,L"已修复草绘 %ls 的小间隙，调整 %d 条直线，实际闭合及重生成检查通过。未保存。",name,edit.changed_count);
    else if(kind==AS_TRIM)swprintf_s(report,capacity,L"已在草绘 %ls 完成指定直线端点的%ls，实际端点与重生成检查通过。未保存。",name,!wcscmp(cmd->arg[3],L"TRIM")?L"修剪":L"延伸");
    else swprintf_s(report,capacity,L"草绘 %ls 已完成%ls，新增 %d 个图元，原生读回和重生成通过。未保存。",name,labels[kind],edit.count);
done:
    if(e&&committed){
        ProErrorlist restore_errors={0};
        fd_sketch_free(&check);
        restored=!ProFeatureWithoptionsRedefine(NULL,&s.feature,s.backup,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&restore_errors)&&
            !part_regenerate(model)&&!fd_sketch_open(model,name,&check,0)&&!as_snapshot(check.section,&actual)&&
            as_snapshot_match(&original,&actual,NULL,tol)&&as_same_dimensions(check.section,strong,strong_count);
    }
    for(i=0;i<edit.ref_count;i++)fd_ref_free(&edit.references[i]);
    fd_sketch_free(&check);fd_sketch_free(&s);
    if(e)swprintf_s(report,capacity,L"草绘%ls未完成（%hs，错误码%d）。%ls。",labels[kind],stage,e,
        committed?(restored?L"已恢复并核对原草绘":L"恢复状态未能核实，请检查模型"):L"未提交模型修改");
    return e;
#undef AS_STEP
}
