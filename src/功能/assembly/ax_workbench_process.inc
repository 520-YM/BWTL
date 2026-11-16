static ProError aw_field_checks(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxBomData data={0};
    ProError error=ax_bom_collect(assembly,&data,1);
    if(error){ax_bom_clear(&data);return error;}
    int reported=0,missing=0;
    for(int i=2;i<cmd->count;i++){
        int at=aw_bom_instance(&data,cmd->arg[i]);
        if(at<0||data.rows[at+1]<0){missing++;continue;}
        const AxOccurrence *item=&data.scan->occurrences[at];
        ProLine label,kind,torque,mark,position;
        aw_bom_value(assembly,&data,at,L"BWTL_STATION",label);
        aw_bom_value(assembly,&data,at,L"BWTL_KIND",kind);
        aw_bom_value(assembly,&data,at,L"BWTL_TORQUE",torque);
        aw_bom_value(assembly,&data,at,L"BWTL_CHECK",mark);
        aw_bom_value(assembly,&data,at,L"BWTL_POSITION",position);
        ProMatrix pose;
        if(ax_layout_measure(assembly,(AxOccurrence *)item,pose)){missing++;continue;}
        reported++;
        const wchar_t *role=!_wcsicmp(kind,L"FASTENER")?L"检查紧固到位、工具和防松标记":
            !_wcsicmp(kind,L"HARNESS")?L"检查端口、锁扣和走线":
            !_wcsicmp(kind,L"SEAL")?L"检查压合与接头":L"检查位置及安装方向";
        part_text_add(out,L"工序 %d %ls，%ls %ls：%ls。Creo 站位 (%.8g, %.8g, %.8g) mm，零件 +Z 方向 (%.6g, %.6g, %.6g)。\n",
            i-1,ax_bom_known(label),cmd->arg[i],item->name,role,pose[3][0]*mm,pose[3][1]*mm,pose[3][2]*mm,pose[2][0],pose[2][1],pose[2][2]);
        part_text_add(out,L"现场位置标注 %ls，规定扭矩 %ls，额外现场复核项 %ls（未标注需补充）。\n",
            ax_bom_known(position),ax_bom_known(torque),ax_bom_known(mark));
    }
    part_text_add(out,L"生成 %d 个站位复核点，%d 个实例无法核实。数值扭矩取已有工艺属性。\n",reported,missing);
    ax_bom_clear(&data);
    return missing?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static int aw_xml(FILE *file,const wchar_t *s){
    for(;*s;s++){
        if(*s==L'&'){if(fputws(L"&amp;",file)==WEOF)return 0;}
        else if(*s==L'<'){if(fputws(L"&lt;",file)==WEOF)return 0;}
        else if(*s==L'>'){if(fputws(L"&gt;",file)==WEOF)return 0;}
        else if(fputwc(*s,file)==WEOF)return 0;
    }
    return 1;
}

static ProError aw_disassembly_sheet(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    const int count=(cmd->count-4)/4;
    AxDeviceFrame frame;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[3],&frame);
    if(error)return error;
    typedef struct {AxComponent owner;ProOutline bounds;ProMdlName name;} Step;
    Step steps[AX_BATCH_LIMIT]={0};
    for(int i=0;i<count;i++){
        const int at=4+4*i;
        if((error=ax_component_resolve(assembly,cmd->arg[at],&steps[i].owner))||
           (error=ProMdlMdlnameGet(steps[i].owner.model,steps[i].name))||
           (error=aw_part_bounds(assembly,cmd->arg[at],&frame,mm,steps[i].bounds)))return error;
        for(int j=2;j<4;j++){
            const wchar_t *ref=cmd->arg[at+j];
            if(!wcscmp(ref,L"NONE"))continue;
            if(j==2){
                AxComponent fastener={0};
                if((error=ax_component_resolve(assembly,ref,&fastener)))return error;
            }else if((error=aw_verify_port(assembly,ref)))return error;
        }
    }
    DeliveryOutput output={0};
    if((error=dv_stage_begin(cmd->arg[2],&output)))return error;
    FILE *file=NULL;
    if((error=ax_bom_open(output.stage,L"service-removal.svg",&file)))return dv_stage_finish(&output,error,out);
    int ok=fwprintf(file,L"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 780 %d\">\n"
        L"<rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/>\n"
        L"<text x=\"22\" y=\"31\" font-size=\"18\" fill=\"#173247\">售后拆装图 · 实际组件局部包络示意</text>\n",52+132*count)>0;
    for(int i=0;ok&&i<count;i++){
        const int at=4+4*i,y=50+132*i;
        const double x_extent=(steps[i].bounds[1][0]-steps[i].bounds[0][0])*mm;
        const double y_extent=(steps[i].bounds[1][1]-steps[i].bounds[0][1])*mm;
        const double factor=70/fmax(1,fmax(x_extent,y_extent));
        const int direction=ax_axis_index(cmd->arg[at+1]+1);
        const int sign=cmd->arg[at+1][0]==L'+'?1:-1;
        const int ax=108,ay=y+59,bx=ax+(direction==0?sign*56:0),by=ay+(direction==1?-sign*56:direction==2?-sign*29:0);
        ok=fwprintf(file,L"<rect x=\"18\" y=\"%d\" width=\"744\" height=\"116\" rx=\"8\" fill=\"#eef3f5\"/>\n"
            L"<rect x=\"32\" y=\"%d\" width=\"%.5g\" height=\"%.5g\" fill=\"#8eb8ca\" stroke=\"#31566e\"/>\n"
            L"<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#ba4326\" stroke-width=\"3\"/>"
            L"<circle cx=\"%d\" cy=\"%d\" r=\"5\" fill=\"#ba4326\"/>\n"
            L"<text x=\"197\" y=\"%d\" font-size=\"15\">第 %d 步 · %ls ",
            y,y+17,fmax(2,x_extent*factor),fmax(2,y_extent*factor),ax,ay,bx,by,bx,by,y+32,i+1,cmd->arg[at])>0;
        if(ok)ok=aw_xml(file,steps[i].name);
        if(ok)ok=fwprintf(file,L"</text><text x=\"197\" y=\"%d\" font-size=\"13\">移出方向 %ls；紧固件 %ls；断开 %ls。</text>\n"
            L"<text x=\"197\" y=\"%d\" font-size=\"13\">模型外廓 XY %.8g × %.8g mm。箭头以指定基准为准。</text>\n",
            y+62,cmd->arg[at+1],cmd->arg[at+2],cmd->arg[at+3],y+86,x_extent,y_extent)>0;
    }
    if(ok)ok=fputws(L"</svg>\n",file)>=0;
    if(fclose(file)||!ok)error=PRO_TK_CANT_WRITE;
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已生成 %d 步拆装 SVG，包含已确认的实例、包络、移出方向和关联件：%ls\\service-removal.svg。\n",
        count,output.target);
    return error;
}

static ProError aw_order_impact(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxComponent active={0};
    ProMdlName active_name;
    ProMdlType type;
    ProError error=ax_component_resolve(assembly,cmd->arg[2],&active);
    if(!error)error=ProMdlMdlnameGet(active.model,active_name);
    if(!error)error=ProMdlTypeGet(active.model,&type);
    if(error)return error;
    AxBomModel changed={0};
    changed.model=active.model;
    changed.type=type;
    ax_bom_read(&changed);
    const wchar_t *code=changed.value[AB_CODE];
    int affected=0,unknown=0;
    for(int i=3;i<cmd->count;i++){
        ProMdl order=NULL;
        int loaded=0;
        AxBomData theirs={0};
        error=ax_load_subassembly(cmd->arg[i],&order,&loaded);
        if(!error&&order==assembly)error=PRO_TK_BAD_INPUTS;
        if(!error)error=ax_bom_collect(order,&theirs,1);
        if(error){unknown++;part_text_add(out,L"订单 %ls 无法完整读取。\n",cmd->arg[i]);}
        else{
            int hits=0;
            for(int j=0;j<theirs.scan->count;j++){
                int ix=theirs.rows[j+1];
                if(ix<0)continue;
                const AxBomModel *item=&theirs.models[ix];
                if(item->type!=type||(_wcsicmp(active_name,item->name)&&(!code[0]||_wcsicmp(code,item->value[AB_CODE]))))continue;
                hits++;
                wchar_t path[256];
                ProLine station;
                aw_bom_value(order,&theirs,j,L"BWTL_STATION",station);
                if(ax_bom_path(&theirs.scan->occurrences[j],path))
                    part_text_add(out,L"订单 %ls %ls %ls 当前版本 %ls，图号 %ls，工位 %ls。\n",
                        cmd->arg[i],path,item->name,ax_bom_known(item->value[AB_REV]),ax_bom_known(item->value[AB_DRAWING]),ax_bom_known(station));
            }
            affected+=hits!=0;
            unknown+=theirs.scan->unavailable!=0;
            if(!hits)part_text_add(out,L"订单 %ls 未找到活动同料号模型。\n",cmd->arg[i]);
        }
        ax_bom_clear(&theirs);
        if(loaded&&order){ProError erase=ProMdlErase(order);if(erase)return erase;}
    }
    part_text_add(out,L"变更目标 %ls %ls（当前版本 %ls），指定范围内涉及 %d 个订单，%d 个含未核实数据。\n",
        active_name,ax_bom_known(code),ax_bom_known(changed.value[AB_REV]),affected,unknown);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static ProError aw_pre_release(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    const wchar_t *profile=cmd->arg[2];
    static const wchar_t *const required[][4]={
        {L"SCREEN",L"SEAL",L"HARNESS",L"COOLING"},
        {L"SCREEN",L"HARNESS",L"POWER",NULL},
        {L"POWER",L"TERMINAL",L"GROUND",NULL}
    };
    const int type=!wcscmp(profile,L"OUTDOOR")?0:!wcscmp(profile,L"INDOOR")?1:2;
    AxBomData bom={0};
    ProError error=ax_bom_collect(assembly,&bom,1);
    if(error){ax_bom_clear(&bom);return error;}
    int actual[4]={0},metadata=0,unknown=bom.scan->unavailable,checked=0,failures=0;
    for(int i=0;i<bom.scan->count;i++){
        int ix=bom.rows[i+1];
        if(ix<0)continue;
        ProLine kind;
        aw_bom_value(assembly,&bom,i,L"BWTL_KIND",kind);
        const AxBomModel *model=&bom.models[ix];
        if(model->type==PRO_MDL_PART&&(!model->value[AB_CODE][0]||!model->value[AB_REV][0]))metadata++;
        for(int j=0;j<4&&required[type][j];j++)if(!_wcsicmp(kind,required[type][j]))actual[j]++;
    }
    for(int i=0;i<4&&required[type][i];i++){
        checked++;
        failures+=!actual[i];
        part_text_add(out,L"%ls 有标签实例 %d 个。%ls。\n",required[type][i],actual[i],actual[i]?L"存在":L"缺少或未标注");
    }
    const ProError health=part_health(assembly);
    part_text_add(out,L"原生再生状态 %ls，缺料号或版本 %d 件，装配不可读 %d 件。\n",
        health?L"需核对":L"通过",metadata,unknown);
    AxDeviceFrame frame;
    ProOutline bounds;
    if(!(error=aw_frame_world(assembly,L"ROOT",cmd->arg[3],&frame)))
        error=aw_part_bounds(assembly,L"ROOT",&frame,mm,bounds);
    if(!error)for(int i=0;i<3;i++){
        const double size=(bounds[1][i]-bounds[0][i])*mm,max=as_arg(cmd,4+i);
        int pass=ax_q_within(size,max);
        checked++;failures+=!pass;
        part_text_add(out,L"%lc 外廓 %.8g mm，限 %.8g mm：%ls。\n",L'X'+i,size,max,pass?L"符合":L"超限");
    }
    if(error){unknown++;part_text_add(out,L"当前外廓无法核实（Creo %d）。\n",error);}
    part_text_add(out,L"%ls 检查 %d 项，几何或类别不符 %d 项；资料缺项 %d，未核实 %d 项。正式工艺和现场验收按项目流程。\n",
        profile,checked,failures,metadata,unknown);
    ax_bom_clear(&bom);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
