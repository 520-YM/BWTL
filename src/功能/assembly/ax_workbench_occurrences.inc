static int aw_contains_name(const wchar_t *name,const wchar_t *query){
    const size_t length=wcslen(query);
    for(const wchar_t *part=name;*part;part++)
        if(!_wcsnicmp(part,query,length))return 1;
    return 0;
}

static ProError aw_find(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxScan *scan=NULL;
    ProError error=ax_scan(assembly,0,&scan);
    if(error)return error;
    const wchar_t *filter=cmd->arg[2];
    int ordered=cmd->count==6;
    int axis=ordered?ax_axis_index(cmd->arg[3]+1):0;
    double sign=ordered&&cmd->arg[3][0]==L'-'?-1:1;
    int ranked[AX_MAX_OCCURRENCES],used=0,unavailable=0,matches=0;
    double positions[AX_MAX_OCCURRENCES];
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(wcscmp(filter,L"ALL")&&!aw_contains_name(item->name,filter))continue;
        matches++;
        if(!item->model||item->state!=PRO_FEAT_ACTIVE){unavailable++;continue;}
        if(!ordered){ranked[used++]=i;continue;}
        ProMatrix pose;
        if(ax_layout_measure(assembly,item,pose)||!isfinite(pose[3][axis])){unavailable++;continue;}
        double coordinate=pose[3][axis]*mm*sign;
        int position=used;
        while(position>0&&coordinate<positions[position-1]){
            ranked[position]=ranked[position-1];
            positions[position]=positions[position-1];
            position--;
        }
        ranked[position]=i;
        positions[position]=coordinate;
        used++;
    }
    int first=0,last=used;
    if(ordered){
        first=(int)as_arg(cmd,4)-1;
        last=first+1;
        if(first>=used||unavailable||scan->limited){
            part_text_add(out,L"排序定位不能核实：名称匹配 %d 个，有效位姿 %d 个，不可读取 %d 个。\n",matches,used,unavailable);
            error=PRO_TK_E_NOT_FOUND;goto done;
        }
        double tolerance=as_arg(cmd,5);
        if((first&&positions[first]-positions[first-1]<=tolerance)||
           (last<used&&positions[last]-positions[first]<=tolerance)){
            part_text_add(out,L"第 %d 位附近有实例坐标差不超过 %.8g mm，请加上型号、另一方向或明确实例路径。\n",first+1,tolerance);
            error=PRO_TK_BAD_CONTEXT;goto done;
        }
    }
    part_text_add(out,L"型号过滤 %ls，匹配 %d 个，可读活动实例 %d 个，不可读取 %d 个。\n",filter,matches,used,unavailable);
    for(int i=first;i<last&&i<first+60;i++){
        AxOccurrence *item=&scan->occurrences[ranked[i]];
        wchar_t label[256];
        if(!ax_path_label(item->ids,item->depth,label,_countof(label)))continue;
        ProMatrix pose;
        if(!ax_layout_measure(assembly,item,pose)){
            part_text_add(out,L"%ls %ls，原点 (%.8g, %.8g, %.8g) mm。\n",label,item->name,
                pose[3][0]*mm,pose[3][1]*mm,pose[3][2]*mm);
        }
    }
    if(last-first>60||scan->limited)part_text_add(out,L"结果明细达到显示或扫描上限。\n");
    if(!used)error=PRO_TK_E_NOT_FOUND;
done:
    free(scan);
    return error;
}

static ProError aw_occurrence(ProMdl assembly,const wchar_t *token,double mm,PartText *out){
    AxComponent selected={0},item={0};
    ProError error=ax_component_resolve(assembly,token,&selected);
    if(error)return error;
    const int depth=selected.path.table_num;
    for(int i=1;i<=depth;i++){
        wchar_t occurrence[256];
        ProMdlName name;
        if(!ax_path_label(selected.path.comp_id_table,i,occurrence,_countof(occurrence)))return PRO_TK_BAD_CONTEXT;
        if((error=ax_component_resolve(assembly,occurrence,&item))||(error=ProMdlMdlnameGet(item.model,name)))return error;
        part_text_add(out,L"第 %d 层 %ls %ls。\n",i,occurrence,name);
    }
    ProMatrix pose;
    if((error=ax_device_pose(&selected,pose)))return error;
    part_text_add(out,L"已定位 %ls，原点 (%.8g, %.8g, %.8g) mm。\n",token,pose[3][0]*mm,pose[3][1]*mm,pose[3][2]*mm);
    for(int i=0;i<3;i++)part_text_add(out,L"局部 %lc 轴在总装方向 (%.8g, %.8g, %.8g)。\n",L'X'+i,pose[i][0],pose[i][1],pose[i][2]);
    return PRO_TK_NO_ERROR;
}

static ProError aw_angle(ProMdl assembly,const wchar_t *id,const wchar_t *csys,
                        const wchar_t *local,const wchar_t *required,double *angle){
    AxComponent component={0};
    AxDeviceFrame frame;
    ProError error=ax_component_resolve(assembly,id,&component);
    if(!error)error=ax_device_frame(&component,csys,1,&frame);
    if(error)return error;
    const double sense=local[0]==required[0]?1:-1;
    const double dot=sense*frame.axis[ax_axis_index(local+1)][ax_axis_index(required+1)];
    *angle=acos(fmax(-1,fmin(1,dot)))*180/AS_PI;
    return PRO_TK_NO_ERROR;
}

static ProError aw_orientation(ProMdl assembly,const PartCommand *cmd,int kind,PartText *out){
    if(kind==AW_ORIENTATION){
        double angle;
        ProError error=aw_angle(assembly,cmd->arg[2],cmd->arg[3],cmd->arg[4],cmd->arg[5],&angle);
        if(error)return error;
        const double threshold=as_arg(cmd,6);
        part_text_add(out,L"%ls %ls 的 %ls 轴相对总装 %ls 偏角 %.8g°（最大 %.8g°），%ls。\n",
            cmd->arg[2],cmd->arg[3],cmd->arg[4],cmd->arg[5],angle,threshold,
            angle<1e-7+threshold?L"方向符合":angle>179.9999?L"方向相反":L"偏角超差");
        return PRO_TK_NO_ERROR;
    }

    const double tolerance=as_arg(cmd,3);
    int failed=0,unknown=0;
    for(int i=4;i<cmd->count;i+=3){
        double angle;
        ProError error=aw_angle(assembly,cmd->arg[i],cmd->arg[i+1],cmd->arg[i+2],cmd->arg[2],&angle);
        if(error){part_text_add(out,L"%ls 方向未核实（Creo %d）。\n",cmd->arg[i],error);unknown++;continue;}
        int pass=angle<1e-7+tolerance;
        failed+=!pass;
        part_text_add(out,L"%ls 相对总装送风方向 %ls 偏角 %.8g°，%ls。\n",
            cmd->arg[i],cmd->arg[2],angle,pass?L"符合":L"方向偏差");
    }
    part_text_add(out,L"风向不符合 %d 个，未核实 %d 个。\n",failed,unknown);
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
