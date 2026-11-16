typedef struct {
    AxScan *tree;
    ProMatrix *poses;
    unsigned char *pose_valid;
} AwAssemblyState;


typedef struct {
    wchar_t path[PART_TOKEN_MAX],base[PART_TOKEN_MAX],own[PART_TOKEN_MAX];
    wchar_t prior_root[PART_TOKEN_MAX],prior_part[PART_TOKEN_MAX];
    int committed,prior_error;
} AwJobResult;
static AwJobResult aw_job[AX_BATCH_LIMIT];
static int aw_job_count;

static AwAssemblyState aw_baseline;
static ProMdl aw_baseline_model;
static ProMdlName aw_baseline_name;
static ProPath aw_baseline_directory;
static ProMdl aw_last_model;
static ProMdlName aw_last_name;
static ProPath aw_last_directory;
static wchar_t aw_last_result[512];


static void aw_state_free(AwAssemblyState *state){
    free(state->tree);
    free(state->poses);
    free(state->pose_valid);
    memset(state,0,sizeof(*state));
}

static ProError aw_state_capture(ProMdl assembly,AwAssemblyState *state){
    ProError error=ax_scan(assembly,0,&state->tree);
    if(error)return error;
    if(state->tree->limited||state->tree->unavailable){aw_state_free(state);return PRO_TK_INCOMPLETE;}
    const size_t capacity=(size_t)state->tree->count+1;
    state->poses=calloc(capacity,sizeof(*state->poses));
    state->pose_valid=calloc(capacity,1);
    if(!state->poses||!state->pose_valid){aw_state_free(state);return PRO_TK_OUT_OF_MEMORY;}
    for(int i=0;i<state->tree->count;i++){
        AxOccurrence *item=&state->tree->occurrences[i];
        if(item->state!=PRO_FEAT_ACTIVE)continue;
        error=ax_layout_measure(assembly,item,state->poses[i]);
        if(error){aw_state_free(state);return error;}
        state->pose_valid[i]=1;
    }
    return PRO_TK_NO_ERROR;
}

static ProError aw_state_verify(ProMdl assembly,const AwAssemblyState *before,int except,double mm,PartText *out){
    AwAssemblyState after={0};
    ProError error=aw_state_capture(assembly,&after);
    if(error)return error;
    if(before->tree->count!=after.tree->count)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<after.tree->count;i++){
        AxOccurrence *a=&before->tree->occurrences[i],*b=&after.tree->occurrences[i];
        if(!ax_layout_same_path(a,b)||a->model!=b->model||a->state!=b->state||
           _wcsicmp(a->name,b->name)||before->pose_valid[i]!=after.pose_valid[i]){error=PRO_TK_INCOMPLETE;break;}
        if(a->ids[0]!=except&&before->pose_valid[i]&&!ax_same_pose(before->poses[i],after.poses[i],mm)){
            wchar_t path[256];
            if(ax_path_label(a->ids,a->depth,path,_countof(path)))
                part_text_add(out,L"其他实例 %ls %ls 的位姿发生变化。\n",path,a->name);
            error=PRO_TK_BAD_CONTEXT;
        }
    }
    aw_state_free(&after);
    return error;
}

static int aw_aligned(ProMdl assembly,AxComponent *part,const wchar_t *base_name,const wchar_t *part_name,double mm){
    ProSelection base=NULL,own=NULL,actual_base=NULL,actual_own=NULL;
    ProAsmcompconstraint *rows=NULL;
    ProCsysdata a,b;
    ProAsmcomppath root;
    ProAsmcompConstrType type;
    ProDatumside side;
    ProMatrix pose;
    int count,match=0;
    if(ax_root_path(assembly,&root)||ax_read_constraints(&part->feature,&root,&rows,&count)||count!=1||
       ProAsmcompconstraintTypeGet(rows[0],&type)||type!=PRO_ASM_CSYS||
       ax_csys_selection(assembly,base_name,1,&base,&a)||ax_csys_selection(part->model,part_name,0,&own,&b)||
       ProAsmcompconstraintAsmreferenceGet(rows[0],&actual_base,&side)||
       ProAsmcompconstraintCompreferenceGet(rows[0],&actual_own,&side))goto done;
    match=ax_selection_equal(base,actual_base,0)&&ax_selection_equal(own,actual_own,part->feature.id)&&
        !ax_actual_pose(assembly,&part->feature,pose)&&ax_frames_aligned(&a,&b,pose,mm);
done:
    if(rows)ProAsmcompconstraintArrayFree(rows);
    if(base)ProSelectionFree(&base);
    if(own)ProSelectionFree(&own);
    return match;
}

static ProError aw_planned_alignment(ProMdl assembly,AxComponent *part,const wchar_t *base_name,const wchar_t *part_name,
                                     double mm,double max_move,double max_angle){
    AxComponent root={0};
    AxDeviceFrame base,own;
    ProMatrix start,wanted;
    ProError error=ax_ref_occurrence(assembly,L"ROOT",&root);
    if(!error)error=ax_device_frame(&root,base_name,0,&base);
    if(!error)error=ax_device_frame(part,part_name,0,&own);
    if(!error)error=ax_actual_pose(assembly,&part->feature,start);
    if(error)return error;
    ax_device_align(&own,&base,wanted);
    return ax_q_within(ax_layout_shift(start,mm,wanted,mm),max_move)&&
        ax_q_within(ax_layout_angle(start,wanted),max_angle)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError aw_alignment_station(ProMdl assembly,const PartCommand *cmd,int at,double mm,PartText *out,int *changed){
    *changed=0;
    AwJobResult *job=&aw_job[aw_job_count++];
    wcscpy_s(job->path,_countof(job->path),cmd->arg[at]);
    AxComponent target={0};
    const wchar_t *id=cmd->arg[at];
    ProError error=ax_hw_occurrence(assembly,id,mm,&target);
    if(!error)error=ax_ordinary_direct(&target.feature);
    if(!error)error=ax_leaf_without_children(&target.feature);
    if(!error)error=ax_check_default_set(&target.feature);
    if(error)return error;

    for(int i=1;i<=3;i+=2)if(aw_aligned(assembly,&target,cmd->arg[at+i],cmd->arg[at+i+1],mm)){
        part_text_add(out,L"%ls 先前已完成，实际定位约束和位姿已核对，跳过。\n",id);
        return PRO_TK_NO_ERROR;
    }
    if((error=ax_packaged(&target.feature))){
        part_text_add(out,L"%ls 已有其他定位，保留原约束。\n",id);
        return error;
    }

    AwAssemblyState before={0};
    error=aw_state_capture(assembly,&before);
    if(error)return error;
    PartCommand align={0};
    align.verb=PART_FOUNDATION;
    align.count=5;
    wcscpy_s(align.arg[0],PART_TOKEN_MAX,L"ASM_CSYS");
    wcscpy_s(align.arg[1],PART_TOKEN_MAX,cmd->arg[1]);
    wcscpy_s(align.arg[2],PART_TOKEN_MAX,id);
    for(int offset=1;offset<=3;offset+=2){
        const wchar_t *base=cmd->arg[at+offset],*part=cmd->arg[at+offset+1];
        error=aw_planned_alignment(assembly,&target,base,part,mm,as_arg(cmd,2),as_arg(cmd,3));
        if(!error){
            wcscpy_s(align.arg[3],PART_TOKEN_MAX,base);
            wcscpy_s(align.arg[4],PART_TOKEN_MAX,part);
            error=ax_constrain(assembly,&align,mm,out);
        }
        if(offset==1){
            wcscpy_s(job->prior_root,_countof(job->prior_root),base);
            wcscpy_s(job->prior_part,_countof(job->prior_part),part);
            job->prior_error=error;
        }
        ProError verify=aw_state_verify(assembly,&before,error?0:target.feature.id,mm,out);
        if(ax_recovery_uncertain||verify){
            if(!ax_recovery_uncertain)ax_uncertain(out);
            error=verify?verify:PRO_TK_INCOMPLETE;
            break;
        }
        if(!error){
            if(!aw_aligned(assembly,&target,base,part,mm)){
                ax_uncertain(out);error=PRO_TK_INCOMPLETE;break;
            }
            wcscpy_s(job->base,_countof(job->base),base);
            wcscpy_s(job->own,_countof(job->own),part);
            job->committed=1;
            *changed=1;
            part_text_add(out,L"%ls 使用第 %d 组参照完成。已核对新增约束、目标姿态和其余实例位置。\n",id,(offset+1)/2);
            break;
        }
        part_text_add(out,L"%ls 第 %d 组参照未通过（Creo %d）。已核对未留下变化。%ls\n",
            id,(offset+1)/2,error,offset==1?L"继续核对另一组参照。":L"");
    }
    aw_state_free(&before);
    return error;
}

static ProError aw_align_batch(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    memset(aw_job,0,sizeof(aw_job));
    aw_job_count=0;
    int written=0,already=0,failed=0,next=4;
    const int requested=(cmd->count-4)/5;
    for(;next<cmd->count;next+=5){
        int changed=0;
        ProError error=aw_alignment_station(assembly,cmd,next,mm,out,&changed);
        if(error){
            failed++;
            part_text_add(out,L"%ls 暂未定位（Creo %d）。\n",cmd->arg[next],error);
            if(ax_recovery_uncertain){next+=5;break;}
        }else if(changed)written++;
        else already++;
    }
    const int unattempted=(cmd->count-next)/5;
    aw_last_model=NULL;
    if(!ProMdlMdlnameGet(assembly,aw_last_name)&&!ProDirectoryCurrentGet(aw_last_directory)){
        aw_last_model=assembly;
        swprintf_s(aw_last_result,_countof(aw_last_result),
            L"装配结果：计划 %d 处，新完成 %d 处，复核已完成 %d 处，未完成 %d 处，未尝试 %d 处。"
            L" 当前成果留在 Creo 会话，未保存。\n",requested,written,already,failed,unattempted);
        part_text_add(out,L"%ls",aw_last_result);
    }
    if(failed&&!ax_recovery_uncertain)part_text_add(out,L"纠正未完成站位的参照后可继续。已核对完成的站位会自动跳过。\n");
    return ax_recovery_uncertain?PRO_TK_INCOMPLETE:written||already?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static int aw_same_workspace(ProMdl assembly,ProMdl expected,const wchar_t *name,const wchar_t *directory){
    ProMdlName current_name;
    ProPath current_directory;
    return assembly==expected&&!ProMdlMdlnameGet(assembly,current_name)&&!_wcsicmp(name,current_name)&&
        !ProDirectoryCurrentGet(current_directory)&&!_wcsicmp(directory,current_directory);
}

static ProError aw_inspect_checkpoint(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    if(!wcscmp(cmd->arg[2],L"CAPTURE")){
        AwAssemblyState next={0};
        ProError error=aw_state_capture(assembly,&next);
        if(error)return error;
        ProMdlName name;
        ProPath directory;
        if((error=ProMdlMdlnameGet(assembly,name))||(error=ProDirectoryCurrentGet(directory))){
            aw_state_free(&next);
            return error;
        }
        aw_state_free(&aw_baseline);
        aw_baseline=next;
        aw_baseline_model=assembly;
        wcscpy_s(aw_baseline_name,_countof(aw_baseline_name),name);
        wcscpy_s(aw_baseline_directory,_countof(aw_baseline_directory),directory);
        part_text_add(out,L"已记录当前装配 %d 个实例的路径、状态和实际位姿。\n",next.tree->count);
        return PRO_TK_NO_ERROR;
    }
    if(!aw_same_workspace(assembly,aw_baseline_model,aw_baseline_name,aw_baseline_directory)){
        part_text_add(out,L"当前模型和工作目录没有可对照的装配快照。\n");
        return PRO_TK_E_NOT_FOUND;
    }
    AwAssemblyState after={0};
    ProError error=aw_state_capture(assembly,&after);
    if(error)return error;
    int additions=0,removed=0,changed=0;
    for(int i=0;i<after.tree->count;i++){
        AxOccurrence *item=&after.tree->occurrences[i];
        wchar_t path[256];
        if(!ax_path_label(item->ids,item->depth,path,_countof(path)))continue;
        int match=0;
        while(match<aw_baseline.tree->count&&!ax_layout_same_path(&aw_baseline.tree->occurrences[match],item))match++;
        if(match==aw_baseline.tree->count){
            if(additions++<40)part_text_add(out,L"%ls 新增实例 %ls。\n",path,item->name);
            continue;
        }
        AxOccurrence *prior=&aw_baseline.tree->occurrences[match];
        const int poses_differ=aw_baseline.pose_valid[match]!=after.pose_valid[i]||
            (after.pose_valid[i]&&!ax_same_pose(aw_baseline.poses[match],after.poses[i],mm));
        if(poses_differ||prior->state!=item->state||prior->model!=item->model){
            if(changed++<40)part_text_add(out,L"%ls %ls，%ls，状态 %ls → %ls。\n",path,item->name,
                poses_differ?L"位姿有变化":L"位姿保持",ax_state_label(prior->state),ax_state_label(item->state));
        }
    }
    for(int i=0;i<aw_baseline.tree->count;i++){
        AxOccurrence *item=&aw_baseline.tree->occurrences[i];
        int match=0;
        while(match<after.tree->count&&!ax_layout_same_path(&after.tree->occurrences[match],item))match++;
        if(match==after.tree->count){
            wchar_t path[256];
            if(removed++<40&&ax_path_label(item->ids,item->depth,path,_countof(path)))
                part_text_add(out,L"%ls %ls 已移除。\n",path,item->name);
        }
    }
    ProError health=part_health(assembly);
    part_text_add(out,L"新增 %d、移除 %d、位姿或状态变化 %d。当前原生再生状态：%ls。\n",additions,removed,changed,health?L"需核对":L"已通过健康检查");
    aw_state_free(&after);
    return health;
}

static ProError aw_show_batch_result(ProMdl assembly,PartText *out){
    if(!aw_same_workspace(assembly,aw_last_model,aw_last_name,aw_last_directory)){
        part_text_add(out,L"当前模型和工作目录暂无批量定位结果。\n");
        return PRO_TK_E_NOT_FOUND;
    }
    part_text_add(out,L"%ls",aw_last_result);
    return PRO_TK_NO_ERROR;
}

