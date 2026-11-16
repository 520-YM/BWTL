static int aw_bom_instance(const AxBomData *data,const wchar_t *token){
    int ids[AX_PATH_DEPTH],n=ax_path_parse(token,ids);
    if(!n)return -1;
    for(int i=0;i<data->scan->count;i++){
        AxOccurrence *item=&data->scan->occurrences[i];
        if(item->depth==n&&!memcmp(item->ids,ids,n*sizeof(ids[0])))return i;
    }
    return -1;
}

static void aw_bom_value(ProMdl root,const AxBomData *data,int i,const wchar_t *field,ProLine result){
    const wchar_t *const fields[]={field};
    ProLine fallback=L"";
    const int row=data->rows[i+1];
    if(row>=0)ax_parameter_text(data->models[row].model,fields,1,fallback);
    ax_bom_occurrence_value(root,&data->scan->occurrences[i],field,fallback,result);
}

static int aw_bom_in_scope(const AxOccurrence *item,const AxOccurrence *parent){
    return !parent||(item->depth>parent->depth&&!memcmp(item->ids,parent->ids,parent->depth*sizeof(item->ids[0])));
}

static const wchar_t *aw_bom_code(const AxBomModel *model){
    return model->value[AB_CODE][0]?model->value[AB_CODE]:model->name;
}

static ProError aw_slots(ProMdl root,const AxBomData *data,PartText *out){
    ProLine *slots=calloc((size_t)data->scan->count+1,sizeof(*slots));
    if(!slots)return PRO_TK_OUT_OF_MEMORY;
    int defined=0,conflicts=0;
    for(int i=0;i<data->scan->count;i++){
        if(data->rows[i+1]<0)continue;
        aw_bom_value(root,data,i,L"BWTL_SLOT",slots[i]);
        if(!slots[i][0])continue;
        defined++;
        AxOccurrence *a=&data->scan->occurrences[i];
        for(int j=0;j<i;j++){
            if(!slots[j][0]||_wcsicmp(slots[i],slots[j]))continue;
            AxOccurrence *b=&data->scan->occurrences[j];
            if(a->depth!=b->depth||memcmp(a->ids,b->ids,(a->depth-1)*sizeof(a->ids[0])))continue;
            wchar_t x[256],y[256];
            if(conflicts++<40&&ax_bom_path(a,x)&&ax_bom_path(b,y))
                part_text_add(out,L"同一父装配的站位 %ls 重复占用：%ls %ls ↔ %ls %ls。\n",slots[i],x,a->name,y,b->name);
        }
    }
    free(slots);
    part_text_add(out,L"有站位标签的活动实例 %d 个，冲突 %d 对，不可读取 %d 个。站位名按同一父装配核对。\n",
        defined,conflicts,data->scan->unavailable);
    return defined&&!data->scan->unavailable?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError aw_accessories(ProMdl root,const AxBomData *data,const PartCommand *cmd,int kind,PartText *out){
    const int terminals=kind==AW_TERMINALS;
    const int parent_index=!wcscmp(cmd->arg[2],L"ROOT")?-1:aw_bom_instance(data,cmd->arg[2]);
    if(parent_index<0&&wcscmp(cmd->arg[2],L"ROOT"))return PRO_TK_E_NOT_FOUND;
    const AxOccurrence *parent=parent_index>=0?&data->scan->occurrences[parent_index]:NULL;
    if(parent&&parent->type!=PRO_MDL_ASSEMBLY)return PRO_TK_BAD_CONTEXT;
    int bad=0,no_trigger=0,untagged=0;
    const int stride=terminals?2:3;
    for(int i=3;i<cmd->count;i+=stride){
        int triggered=0,found=0,unclear=0;
        const wchar_t *needed=terminals?cmd->arg[i]:cmd->arg[i+1];
        for(int j=0;j<data->scan->count;j++){
            const AxOccurrence *item=&data->scan->occurrences[j];
            if(ax_suppressed(item->state)||!aw_bom_in_scope(item,parent))continue;
            const int row=data->rows[j+1];
            if(row<0){unclear++;continue;}
            const AxBomModel *model=&data->models[row];
            if(terminals){
                ProLine category;
                aw_bom_value(root,data,j,L"BWTL_KIND",category);
                if(!category[0])untagged++;
                else if(!_wcsicmp(category,needed))found++;
            }else{
                if(!_wcsicmp(aw_bom_code(model),cmd->arg[i]))triggered++;
                if(!_wcsicmp(aw_bom_code(model),needed))found++;
            }
        }
        if(!terminals&&!triggered){
            part_text_add(out,L"范围 %ls 没有触发料号 %ls 的活动实例。%ls。\n",cmd->arg[2],cmd->arg[i],unclear?L"另有不可读实例":L"不适用");
            no_trigger++;continue;
        }
        const int expected=terminals?(int)as_arg(cmd,i+1):triggered*(int)as_arg(cmd,i+2);
        int pass=terminals?found==expected:found>=expected;
        bad+=!pass;
        part_text_add(out,L"%ls %ls 实际 %d 个，%ls %d 个。%ls。%ls\n",
            cmd->arg[2],needed,found,terminals?L"指定数量":L"最低需要",expected,pass?L"符合":L"数量不符",
            unclear?L"范围内另有不可读实例。":L"");
    }
    if(terminals&&untagged)part_text_add(out,L"范围内有未标注 BWTL_KIND 的件，未纳入附件分类。\n");
    part_text_add(out,L"规则不符 %d 条，未触发 %d 条。\n",bad,no_trigger);
    return PRO_TK_NO_ERROR;
}

static ProError aw_torque_audit(ProMdl root,const AxBomData *data,const PartCommand *cmd,PartText *out){
    int bad=0,unknown=0;
    for(int i=2;i<cmd->count;i+=5){
        int index=aw_bom_instance(data,cmd->arg[i]);
        if(index<0||data->rows[index+1]<0){
            unknown++;part_text_add(out,L"%ls 组件无法读取。\n",cmd->arg[i]);continue;
        }
        ProLine torque,locking,mark;
        aw_bom_value(root,data,index,L"BWTL_TORQUE",torque);
        aw_bom_value(root,data,index,L"BWTL_LOCKING",locking);
        aw_bom_value(root,data,index,L"BWTL_TORQUE_MARK",mark);
        double actual=0;
        if(!torque[0]||!locking[0]||!mark[0]||!qa_number(torque,0,1e5)||!part_number(torque,&actual)){
            unknown++;part_text_add(out,L"%ls 缺少数值扭矩、防松方式或标记数据。已读：%ls / %ls / %ls。\n",
                cmd->arg[i],ax_bom_known(torque),ax_bom_known(locking),ax_bom_known(mark));continue;
        }
        const double expected=as_arg(cmd,i+1),tol=as_arg(cmd,i+2);
        int pass=ax_q_within(fabs(actual-expected),tol)&&!_wcsicmp(locking,cmd->arg[i+3])&&!_wcsicmp(mark,cmd->arg[i+4]);
        bad+=!pass;
        part_text_add(out,L"%ls 规定扭矩 %.8g ± %.8g N·m，模型 %.8g N·m，防松 %ls，标记 %ls。%ls。\n",
            cmd->arg[i],expected,tol,actual,locking,mark,pass?L"工艺标注符合":L"工艺标注不符");
    }
    part_text_add(out,L"工艺标注不符 %d 个，未核实 %d 个。执行扭矩由现场量测确认。\n",bad,unknown);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static ProError aw_temporary(ProMdl root,const AxBomData *data,PartText *out){
    int tagged=0,issues=0,unknown=0;
    for(int i=0;i<data->scan->count;i++){
        AxOccurrence *item=&data->scan->occurrences[i];
        if(ax_suppressed(item->state))continue;
        if(data->rows[i+1]<0){unknown++;continue;}
        ProLine state,note;
        aw_bom_value(root,data,i,L"BWTL_BUILD_STATUS",state);
        if(!state[0]){unknown++;continue;}
        tagged++;
        if(_wcsicmp(state,L"TEMP")&&_wcsicmp(state,L"SUBSTITUTE")&&_wcsicmp(state,L"PENDING")&&
           wcscmp(state,L"临时")&&wcscmp(state,L"代用")&&wcscmp(state,L"待采购"))continue;
        aw_bom_value(root,data,i,L"BWTL_BUILD_NOTE",note);
        wchar_t path[256];
        if(issues++<100&&ax_bom_path(item,path))
            part_text_add(out,L"%ls %ls：%ls。备注 %ls。\n",path,item->name,state,ax_bom_known(note));
    }
    part_text_add(out,L"标注 %d 个，临时/代用/待采购 %d 个，未标注或不可读取 %d 个。\n",tagged,issues,unknown);
    return tagged?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError aw_design_vs_shop(const AxBomData *data,const PartCommand *cmd,PartText *out){
    int compared=0,mismatch=0,unlisted=0,unidentified=0;
    for(int at=2;at<cmd->count;at+=2){
        int actual=0;
        for(int i=1;i<data->count;i++){
            const AxBomModel *row=&data->models[i];
            if(row->type==PRO_MDL_PART&&row->value[AB_CODE][0]&&!_wcsicmp(row->value[AB_CODE],cmd->arg[at]))actual+=row->qty;
        }
        const int expected=(int)as_arg(cmd,at+1);
        compared++;
        mismatch+=actual!=expected;
        part_text_add(out,L"%ls 设计用量 %d，生产表 %d，差值 %+d。\n",cmd->arg[at],actual,expected,actual-expected);
    }
    for(int i=1;i<data->count;i++){
        const AxBomModel *row=&data->models[i];
        if(row->type!=PRO_MDL_PART)continue;
        if(!row->value[AB_CODE][0]){unidentified+=row->qty;continue;}
        int at=2;
        while(at<cmd->count&&_wcsicmp(row->value[AB_CODE],cmd->arg[at]))at+=2;
        if(at==cmd->count&&unlisted++<40)
            part_text_add(out,L"%ls（%ls）设计用量 %d，未纳入生产表。\n",row->name,row->value[AB_CODE],row->qty);
    }
    part_text_add(out,L"按同一料号核对 %d 项，数量差 %d 项，未列入生产表的型号 %d 种，无料号 %d 件，不可读 %d 个实例。\n",
        compared,mismatch,unlisted,unidentified,data->scan->unavailable);
    return PRO_TK_NO_ERROR;
}

static ProError aw_issue_list(ProMdl root,const AxBomData *data,const PartCommand *cmd,PartText *out){
    ProLine *station=calloc((size_t)data->scan->count+1,sizeof(ProLine));
    if(!station)return PRO_TK_OUT_OF_MEMORY;
    int unknown=0,matched=0;
    for(int i=0;i<data->scan->count;i++){
        int row=data->rows[i+1];
        if(row<0){
            if(!ax_suppressed(data->scan->occurrences[i].state))unknown++;
            continue;
        }
        if(data->models[row].type!=PRO_MDL_PART)continue;
        aw_bom_value(root,data,i,L"BWTL_STATION",station[i]);
        if(!station[i][0])unknown++;
        else if(!wcscmp(cmd->arg[2],L"ALL")||!_wcsicmp(cmd->arg[2],station[i]))matched++;
    }
    DeliveryOutput output={0};
    ProError error=PRO_TK_NO_ERROR;
    FILE *file=NULL;
    if(cmd->count==4){
        if((error=dv_stage_begin(cmd->arg[3],&output))){free(station);return error;}
        error=ax_bom_open(output.stage,L"workstation-picking.csv",&file);
        if(error){free(station);return dv_stage_finish(&output,error,out);}
    }
    unsigned char used[AX_MAX_OCCURRENCES]={0};
    int rows=0;
    int ok=!file||fwprintf(file,L"工位,模型,料号,规格,数量\n")>0;
    for(int i=0;ok&&i<data->scan->count;i++){
        const int index=data->rows[i+1];
        if(index<0||used[i]||!station[i][0]||(wcscmp(cmd->arg[2],L"ALL")&&_wcsicmp(cmd->arg[2],station[i])))continue;
        const AxBomModel *row=&data->models[index];
        int quantity=1;
        for(int j=i+1;j<data->scan->count;j++)
            if(data->rows[j+1]==index&&!_wcsicmp(station[i],station[j])){used[j]=1;quantity++;}
        wchar_t count[24];
        swprintf_s(count,_countof(count),L"%d",quantity);
        if(file){
            const wchar_t *fields[]={station[i],row->name,row->value[AB_CODE],row->value[AB_SPEC],count};
            ok=ax_bom_csv(file,fields,_countof(fields));
        }
        if(rows++<45)part_text_add(out,L"%ls：%ls %ls，%d 件。\n",station[i],row->name,row->value[AB_CODE],quantity);
    }
    free(station);
    if(file){
        if(fclose(file)||!ok)error=PRO_TK_CANT_WRITE;
        error=dv_stage_finish(&output,error,out);
        if(!error)part_text_add(out,L"已输出 %ls\\workstation-picking.csv。\n",output.target);
    }
    part_text_add(out,L"工位匹配 %d 个零件实例，汇总 %d 行，未标注工位或不可读 %d 个。\n",matched,rows,unknown);
    return error;
}

static ProError aw_production(ProMdl root,const PartCommand *cmd,int kind,PartText *out){
    AxBomData data={0};
    ProError error=ax_bom_collect(root,&data,1);
    if(!error)switch(kind){
    case AW_SLOT_RULE:error=aw_slots(root,&data,out);break;
    case AW_REQUIRED:case AW_TERMINALS:error=aw_accessories(root,&data,cmd,kind,out);break;
    case AW_TORQUE:error=aw_torque_audit(root,&data,cmd,out);break;
    case AW_TEMPORARY:error=aw_temporary(root,&data,out);break;
    case AW_PRODUCTION_BOM:error=aw_design_vs_shop(&data,cmd,out);break;
    case AW_ISSUE_LIST:error=aw_issue_list(root,&data,cmd,out);break;
    default:error=PRO_TK_BAD_INPUTS;break;
    }
    ax_bom_clear(&data);
    return error;
}
