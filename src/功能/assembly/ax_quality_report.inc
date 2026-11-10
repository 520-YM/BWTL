
#define AX_Q_REPORT_ROWS 24
typedef struct {
    int kind,checked,failed,unknown;
    wchar_t text[PART_TEXT_MAX];
} AxQualityEntry;

typedef struct {
    ProMdl assembly;
    ProWVerstamp root,children[AX_MAX_OCCURRENCES];
    ProMatrix poses[AX_MAX_OCCURRENCES];
    ProError pose_status[AX_MAX_OCCURRENCES];
    ProPath directory;
    AxScan *scan;
    AxQualityEntry entries[AX_Q_REPORT_ROWS];
    int count,dropped;
} AxQualityReport;

static AxQualityReport ax_q_report_cache;

static void ax_q_clear(AxQualityReport *cache){
    if(cache->root)ProVerstampFree(&cache->root);
    if(cache->scan)for(int i=0;i<cache->scan->count;i++)
        if(cache->children[i])ProVerstampFree(&cache->children[i]);
    free(cache->scan);
    memset(cache,0,sizeof(*cache));
}

static int ax_q_stamp_matches(ProMdl model,ProWVerstamp recorded){
    ProWVerstamp stamp=NULL;
    if(!recorded||ProMdlVerstampGet(model,&stamp))return 0;
    int equal=ProVerstampEqual(recorded,stamp);
    ProVerstampFree(&stamp);
    return equal;
}

static ProError ax_q_report_pose(ProMdl assembly,AxOccurrence *item,ProMatrix pose){
    ProIdTable ids={0};
    ProAsmcomppath path;
    memcpy(ids,item->ids,item->depth*sizeof(int));
    ProError error=ProAsmcomppathInit((ProSolid)assembly,ids,item->depth,&path);
    return error?error:ProAsmcomppathTrfGet(&path,PRO_B_TRUE,pose);
}

static int ax_q_report_current(ProMdl assembly,AxQualityReport *cache){
    if(cache->assembly!=assembly||!cache->scan||!ax_q_stamp_matches(assembly,cache->root))return 0;
    ProPath directory;
    if(ProDirectoryCurrentGet(directory)||_wcsicmp(directory,cache->directory))return 0;
    AxScan *now=NULL;
    if(ax_scan(assembly,0,&now))return 0;
    int equal=now->count==cache->scan->count&&now->limited==cache->scan->limited;
    for(int i=0;equal&&i<now->count;i++){
        AxOccurrence *a=&now->occurrences[i],*b=&cache->scan->occurrences[i];
        equal=a->model==b->model&&a->type==b->type&&a->state==b->state&&a->depth==b->depth&&
            !memcmp(a->ids,b->ids,a->depth*sizeof(int))&&(!a->model||ax_q_stamp_matches(a->model,cache->children[i]));
        if(equal&&a->model){
            ProMatrix pose;
            ProError status=ax_q_report_pose(assembly,a,pose);
            equal=status==cache->pose_status[i]&&(status||ax_same_pose(pose,cache->poses[i],1));
        }
    }
    free(now);
    return equal;
}

static void ax_q_remember(ProMdl assembly,int kind,const AxQuality *q){
    AxQualityReport *cache=&ax_q_report_cache;
    if(!ax_q_report_current(assembly,cache)){
        ax_q_clear(cache);
        if(ProDirectoryCurrentGet(cache->directory)||ProMdlVerstampGet(assembly,&cache->root)||
           ax_scan(assembly,0,&cache->scan)){ax_q_clear(cache);return;}
        for(int i=0;i<cache->scan->count;i++){
            ProMdl model=cache->scan->occurrences[i].model;
            if(model&&ProMdlVerstampGet(model,&cache->children[i])){ax_q_clear(cache);return;}
            if(model)cache->pose_status[i]=ax_q_report_pose(assembly,&cache->scan->occurrences[i],cache->poses[i]);
        }
        cache->assembly=assembly;
    }
    if(cache->count==AX_Q_REPORT_ROWS){
        memmove(cache->entries,cache->entries+1,sizeof(cache->entries)-sizeof(cache->entries[0]));
        cache->count--;cache->dropped++;
    }
    AxQualityEntry *entry=&cache->entries[cache->count++];
    entry->kind=kind;
    entry->checked=q->checked;
    entry->failed=q->failed;
    entry->unknown=q->unknown;
    if(wcsncpy_s(entry->text,_countof(entry->text)-32,q->out->text,_TRUNCATE))
        wcscat_s(entry->text,_countof(entry->text),L"\n（本条详单已截断）\n");
}

static const wchar_t *ax_q_name(int kind){
    static const wchar_t *const names[]={L"装配重生成诊断",L"分区域间隙",L"白名单干涉",L"紧固孔对位",
        L"面板齐平度",L"安装面共面性",L"装配公差叠加",L"重心与支撑范围",L"外形与安装空间"};
    return names[kind-AX_Q_REGEN];
}

static ProError ax_q_export(AxQualityReport *cache,const wchar_t *leaf,const wchar_t *summary,PartText *out){
    DeliveryOutput output={0};
    ProError error=dv_stage_begin(leaf,&output);
    if(error==PRO_TK_E_FOUND)part_text_add(out,L"报告目录 %ls 已存在，请使用新名称。\n",leaf);
    if(error)return error;
    ProPath path;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\report.txt",output.stage)<0)
        return dv_stage_finish(&output,PRO_TK_BAD_INPUTS,out);
    FILE *file=_wfopen(path,L"wt, ccs=UTF-8");
    if(!file)return dv_stage_finish(&output,PRO_TK_CANT_WRITE,out);
    if(fwprintf(file,L"%ls\n",summary)<0)error=PRO_TK_CANT_WRITE;
    for(int i=0;!error&&i<cache->count;i++){
        AxQualityEntry *entry=&cache->entries[i];
        if(fwprintf(file,L"\n%d %ls\n%ls",311+entry->kind-AX_Q_REGEN,ax_q_name(entry->kind),entry->text)<0)
            error=PRO_TK_CANT_WRITE;
    }
    if(fclose(file)&&!error)error=PRO_TK_CANT_WRITE;
    if(!error&&!dv_file_nonempty(path))error=PRO_TK_CANT_WRITE;
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已输出报告：%ls\\report.txt\n",output.target);
    return error;
}

static ProError ax_q_report(ProMdl assembly,const wchar_t *name,const wchar_t *leaf,PartText *out){
    AxQualityReport *cache=&ax_q_report_cache;
    if(!cache->count||!ax_q_report_current(assembly,cache)){
        part_text_add(out,L"%ls 暂无与当前装配、组件版本及工作目录一致的质量结果。请重新运行需要汇总的检查。\n",name);
        return PRO_TK_E_NOT_FOUND;
    }
    int checked=0,failed=0,unknown=0;
    unsigned int categories=0;
    for(int i=0;i<cache->count;i++){
        AxQualityEntry *entry=&cache->entries[i];
        checked+=entry->checked;failed+=entry->failed;unknown+=entry->unknown;
        categories|=1u<<(entry->kind-AX_Q_REGEN);
    }
    part_text_add(out,L"%ls 装配质量检查报告。已测 %d 项，不符合 %d 项，未核实 %d 项。\n",name,checked,failed,unknown);
    if(cache->dropped)part_text_add(out,L"仅保留最近 %d 次检查，之前 %d 次未收入。\n",cache->count,cache->dropped);
    for(int k=AX_Q_REGEN;k<AX_Q_REPORT;k++)if(!(categories&(1u<<(k-AX_Q_REGEN))))
        part_text_add(out,L"%d %ls：本报告无记录。\n",311+k-AX_Q_REGEN,ax_q_name(k));
    if(leaf)return ax_q_export(cache,leaf,out->text,out);
    for(int i=0;i<cache->count;i++){
        AxQualityEntry *entry=&cache->entries[i];
        if(wcslen(out->text)+wcslen(entry->text)+200>=out->size){
            part_text_add(out,L"\n其余 %d 次详单超出聊天容量，可按需导出报告文件。\n",cache->count-i);
            break;
        }
        part_text_add(out,L"\n%d %ls\n%ls",311+entry->kind-AX_Q_REGEN,ax_q_name(entry->kind),entry->text);
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_quality_execute(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxQuality q={out,0,0,0};
    ProError error=PRO_TK_NO_ERROR;
    switch(kind){
    case AX_Q_REGEN:error=ax_q_regen(assembly,cmd,&q);break;
    case AX_Q_GAPS:error=ax_q_gaps(assembly,cmd,mm,&q);break;
    case AX_Q_INTERF:error=ax_q_interference(assembly,cmd,mm,&q);break;
    case AX_Q_HOLES:error=ax_q_holes(assembly,cmd,mm,&q);break;
    case AX_Q_FLUSH:case AX_Q_COPLANE:error=ax_q_planes(assembly,cmd,kind,mm,&q);break;
    case AX_Q_STACK:error=ax_q_stack(cmd,&q);break;
    case AX_Q_SUPPORT:error=ax_q_support(assembly,cmd,mm,&q);break;
    case AX_Q_SPACE:error=ax_q_space(assembly,cmd,mm,&q);break;
    case AX_Q_REPORT:return ax_q_report(assembly,cmd->arg[1],cmd->count==3?cmd->arg[2]:NULL,out);
    default:return PRO_TK_BAD_INPUTS;
    }
    if(error)ax_q_missing(&q,ax_q_name(kind),error);
    if(out->truncated)q.unknown++;
    part_text_add(out,L"已测 %d 项，不符合 %d 项，未核实 %d 项。\n",q.checked,q.failed,q.unknown);
    ax_q_remember(assembly,kind,&q);
    return error;
}
