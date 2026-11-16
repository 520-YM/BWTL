static ProError aw_shared_model(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxComponent selected={0};
    ProMdlName name;
    ProMdlType type;
    ProError error=ax_component_resolve(assembly,cmd->arg[2],&selected);
    if(!error)error=ProMdlMdlnameGet(selected.model,name);
    if(!error)error=ProMdlTypeGet(selected.model,&type);
    if(error)return error;
    int known=0,found=0,unknown=0;
    for(int i=3;i<cmd->count;i++){
        ProMdl model=NULL;
        AxScan *scan=NULL;
        int loaded=0,hits=0;
        error=ax_load_subassembly(cmd->arg[i],&model,&loaded);
        if(!error&&model==assembly)error=PRO_TK_BAD_INPUTS;
        if(!error)error=ax_scan(model,0,&scan);
        if(error||scan->limited||scan->unavailable){
            part_text_add(out,L"%ls 未完整读取。\n",cmd->arg[i]);unknown++;
        }else{
            known++;
            for(int j=0;j<scan->count;j++){
                AxOccurrence *item=&scan->occurrences[j];
                if(item->state!=PRO_FEAT_ACTIVE||item->type!=type||_wcsicmp(item->name,name))continue;
                wchar_t path[256];
                if(hits++<30&&ax_bom_path(item,path))part_text_add(out,L"%ls %ls 使用同名模型 %ls。\n",cmd->arg[i],path,name);
            }
            found+=hits!=0;
            if(!hits)part_text_add(out,L"%ls 未检出同名活动实例。\n",cmd->arg[i]);
        }
        free(scan);
        if(loaded&&model){ProError erase=ProMdlErase(model);if(erase)return erase;}
    }
    part_text_add(out,L"%ls %ls 在指定 %d 个可读装配中有 %d 个共享，%d 个未核实。范围限所列订单装配。\n",
        cmd->arg[2],name,known,found,unknown);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static int aw_path_index(AxScan *scan,const AxOccurrence *item){
    for(int i=0;i<scan->count;i++){
        const AxOccurrence *candidate=&scan->occurrences[i];
        if(candidate->depth==item->depth&&!memcmp(candidate->ids,item->ids,item->depth*sizeof(item->ids[0])))return i;
    }
    return -1;
}

typedef struct {PartText *out;FILE *file;int total,ok;} AwRework;

static void aw_rework_row(AwRework *result,const wchar_t *action,const AxOccurrence *item,
                          const wchar_t *old,const wchar_t *current,const wchar_t *move){
    if(!result->ok)return;
    wchar_t path[256];
    if(!ax_path_label(item->ids,item->depth,path,_countof(path))){result->ok=0;return;}
    if(result->total++<100)part_text_add(result->out,L"%ls %ls：%ls → %ls。%ls\n",action,path,old,current,move);
    if(result->file){
        const wchar_t *fields[]={action,path,old,current,move};
        result->ok=ax_bom_csv(result->file,fields,_countof(fields));
    }
}

static ProError aw_rework(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    ProMdl prior=NULL;
    AxScan *current=NULL,*old=NULL;
    double prior_mm;
    int loaded=0;
    AwRework result={out,NULL,0,1};
    DeliveryOutput publish={0};
    ProError error=ax_load_subassembly(cmd->arg[2],&prior,&loaded);
    if(!error&&prior==assembly)error=PRO_TK_BAD_INPUTS;
    if(!error)error=qa_mm_per_unit(prior,&prior_mm);
    if(!error)error=ax_scan(assembly,1,&current);
    if(!error)error=ax_scan(prior,1,&old);
    if(error)goto done;
    if(cmd->count==6){
        error=dv_stage_begin(cmd->arg[5],&publish);
        if(error)goto done;
        error=ax_bom_open(publish.stage,L"assembly-rework.csv",&result.file);
        if(error)goto done;
        result.ok=fwprintf(result.file,L"工序,实例路径,旧模型,新模型,位移及说明\n")>0;
    }
    int added=0,removed=0,replaced=0,moved=0,unknown=0;
    for(int i=0;i<current->count;i++){
        AxOccurrence *a=&current->occurrences[i];
        if(a->type!=PRO_MDL_PART||a->state!=PRO_FEAT_ACTIVE)continue;
        int match=aw_path_index(old,a);
        AxOccurrence *b=match<0?NULL:&old->occurrences[match];
        if(!b||b->type!=PRO_MDL_PART||b->state!=PRO_FEAT_ACTIVE){
            aw_rework_row(&result,L"装入",a,L"—",a->name,L"在新装配路径安装。");added++;continue;
        }
        if(_wcsicmp(a->name,b->name)){
            aw_rework_row(&result,L"更换",a,b->name,a->name,L"新旧版本同一实例路径。");replaced++;
        }
        ProMatrix p,q;
        if(ax_layout_measure(assembly,a,p)||ax_layout_measure(prior,b,q)){unknown++;continue;}
        const double rest=ax_layout_shift(p,mm,q,prior_mm),rotation=ax_layout_angle(p,q);
        if(rest<=as_arg(cmd,3)&&rotation<=as_arg(cmd,4))continue;
        wchar_t movement[256];
        swprintf_s(movement,_countof(movement),L"各自总装坐标系原点差 (%+.8g, %+.8g, %+.8g) mm，转角 %.8g°。",
            p[3][0]*mm-q[3][0]*prior_mm,p[3][1]*mm-q[3][1]*prior_mm,p[3][2]*mm-q[3][2]*prior_mm,rotation);
        aw_rework_row(&result,L"移位",a,b->name,a->name,movement);moved++;
    }
    for(int i=0;i<old->count;i++){
        AxOccurrence *a=&old->occurrences[i];
        if(a->type!=PRO_MDL_PART||a->state!=PRO_FEAT_ACTIVE)continue;
        int match=aw_path_index(current,a);
        if(match>=0&&current->occurrences[match].type==PRO_MDL_PART&&current->occurrences[match].state==PRO_FEAT_ACTIVE)continue;
        aw_rework_row(&result,L"拆除",a,a->name,L"—",L"新装配相同位置无活动零件。");removed++;
    }
    part_text_add(out,L"按同源实例路径比较：装入 %d、拆除 %d、更换 %d、移位 %d、位姿未核实 %d。对比时两版应使用同一个坐标基准。\n",
        added,removed,replaced,moved,unknown);
    if(!result.ok)error=PRO_TK_CANT_WRITE;
done:
    if(result.file&&fclose(result.file)&&!error)error=PRO_TK_CANT_WRITE;
    if(publish.stage[0]){
        error=dv_stage_finish(&publish,error,out);
        if(!error)part_text_add(out,L"已输出 %ls\\assembly-rework.csv。\n",publish.target);
    }
    free(current);free(old);
    if(loaded&&prior){ProError erase=ProMdlErase(prior);if(!error)error=erase;}
    return error;
}
