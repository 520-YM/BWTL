typedef struct {
    const wchar_t *sku,*grade,*method;
    int found;
    double hole_min,hole_max,grip_min,grip_max,edge_min;
    wchar_t revision[32];
} SmyFastener;

static int smy_fastener_row(wchar_t **fields,void *context){
    SmyFastener *item=context;
    if(!dv_safe_leaf(fields[0])||!dv_safe_leaf(fields[1])||
       (wcscmp(fields[2],L"PRESS")&&wcscmp(fields[2],L"RIVET")&&wcscmp(fields[2],L"STUD"))||!dv_safe_leaf(fields[8]))return 0;
    double v[5];
    for(int i=0;i<5;i++){
        if(!smx_number(fields[3+i],0.001,1000))return 0;
        v[i]=wcstod(fields[3+i],NULL);
    }
    if(v[0]>v[1]||v[2]>v[3])return 0;
    if(_wcsicmp(fields[0],item->sku)||_wcsicmp(fields[1],item->grade)||wcscmp(fields[2],item->method))return 1;
    if(item->found)return 0;
    item->found=1;
    item->hole_min=v[0];item->hole_max=v[1];item->grip_min=v[2];item->grip_max=v[3];item->edge_min=v[4];
    wcscpy_s(item->revision,_countof(item->revision),fields[8]);
    return 1;
}

static ProError smy_fastener_check(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    SmyFastener item={cmd->arg[2],cmd->arg[3],kind==SMY_PRESS_CHECK?L"PRESS":L"RIVET"};
    ProError error=smx_table(L"BWTL_SMT_FASTENERS.tsv",9,smy_fastener_row,&item,out);
    if(!error&&!item.found){
        part_text_add(out,L"公司紧固件表没有匹配型号、材料和工艺的记录。\n");
        error=PRO_TK_E_NOT_FOUND;
    }
    double mm=1,thickness;
    if(!error)error=fab_thickness(model,&mm,&thickness);
    FdTxn tx={0};
    tx.model=model;tx.scale=1/mm;
    ShopMouth hole={0};
    if(!error){
        error=mp_mouth(&tx,cmd->arg[4],cmd->arg[5],&hole);
        if(error)part_text_add(out,L"指定圆柱面未与安装面完整孔口配对（Creo %d）。\n",error);
    }
    double clearance;
    int edge;
    if(!error)error=smy_form_clearance_value(&tx,&hole.frame,0,0,0,&clearance,&edge);
    if(!error&&!isfinite(clearance))error=PRO_TK_BAD_CONTEXT;
    double local=0;
    if(!error){
        double rim=fmin(0.2,(clearance-hole.radius)/2);
        error=rim>0.0002?mp_read_stock(&tx,&hole.frame,as_point(hole.radius+rim,0),0,&local,1):PRO_TK_BAD_CONTEXT;
        if(error)part_text_add(out,L"孔口周边未取得可靠的本地厚度射线（Creo %d）。\n",error);
    }
    if(!error&&fabs(local-thickness)>0.005)error=PRO_TK_BAD_CONTEXT;
    double extra=0;
    if(!error&&cmd->count==7){
        ProMdl second=NULL;
        int loaded=0;
        double other_mm;
        error=ax_load_part(cmd->arg[6],&second,&loaded);
        if(!error)error=fab_thickness(second,&other_mm,&extra);
        if(loaded&&second){
            ProError erased=ProMdlErase(second);
            if(!error)error=erased;
        }
    }
    if(error)return error;
    double diameter=2*hole.radius,grip=local+extra,net=clearance-hole.radius;
    int diameter_ok=diameter>=item.hole_min-1e-7&&diameter<=item.hole_max+1e-7;
    int grip_ok=grip>=item.grip_min-1e-7&&grip<=item.grip_max+1e-7;
    int edge_ok=net>=item.edge_min-1e-7;
    part_text_add(out,L"%ls / %ls（规则 %ls），安装面 %ls，孔壁 %ls。\n",item.sku,item.grade,item.revision,cmd->arg[4],cmd->arg[5]);
    part_text_add(out,L"孔径 %.8g mm，要求 %.8g–%.8g mm；实测板厚合计 %.8g mm，要求 %.8g–%.8g mm；到 EDGE:%d 的孔边净距 %.8g mm，下限 %.8g mm。\n",
        diameter,item.hole_min,item.hole_max,grip,item.grip_min,item.grip_max,edge,net,item.edge_min);
    part_text_add(out,L"孔径 %ls，夹持厚度 %ls，孔边净距 %ls。\n",diameter_ok?L"符合":L"超范围",grip_ok?L"符合":L"超范围",edge_ok?L"符合":L"不足");
    if(extra)part_text_add(out,L"第二件只计入实际板厚；两件贴合和孔对位需要对应装配几何确认。\n");
    return PRO_TK_NO_ERROR;
}

typedef struct {
    ProVector first,axis;
    double mm,length,start,segment,pitch;
    int count;
} SmyWeldSpan;

static ProError smy_weld_span(ProMdl model,const wchar_t *ref,int continuous,double start,double end,double segment,double pitch,SmyWeldSpan *span){
    ProEdge edge;
    ProEnttype shape;
    ProVector last,d1,d2,d3;
    ProError error=qa_mm_per_unit(model,&span->mm);
    if(!error)error=in_edge(model,ref,&edge);
    if(!error)error=ProEdgeTypeGet(edge,&shape);
    if(!error&&shape!=PRO_ENT_LINE)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProEdgeXyzdataEval(edge,0,span->first,d1,d2,d3);
    if(!error)error=ProEdgeXyzdataEval(edge,1,last,d1,d2,d3);
    if(error)return error;
    ad_sub(last,span->first,span->axis);
    span->length=cm_distance(span->first,last)*span->mm;
    if(!ad_unit(span->axis)||start+end>=span->length)return PRO_TK_BAD_CONTEXT;
    double available=span->length-start-end;
    if(continuous){
        if(segment||pitch)return PRO_TK_BAD_INPUTS;
        segment=available;
        pitch=available;
    }else if(segment<=0||pitch<segment||segment>available)return PRO_TK_BAD_INPUTS;
    double count=1+floor((available-segment+1e-8)/pitch);
    if(count>512)return PRO_TK_OUT_OF_RANGE;
    span->count=(int)count;span->start=start;span->segment=segment;span->pitch=pitch;
    return PRO_TK_NO_ERROR;
}

static ProError smy_weld_schedule(ProMdl model,const PartCommand *cmd,PartText *out){
    SmyWeldSpan span={0};
    ProError error=smy_weld_span(model,cmd->arg[3],!wcscmp(cmd->arg[4],L"CONT"),as_arg(cmd,5),as_arg(cmd,6),
        as_arg(cmd,7),as_arg(cmd,8),&span);
    if(error)return error;
    double mm=span.mm;
    DeliveryOutput work={0};
    FILE *file=NULL;
    error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"weld-segments.csv",&file);
    if(!error)fwprintf(file,L"编号,模型,边,起距_mm,终距_mm,长度_mm,AX_mm,AY_mm,AZ_mm,BX_mm,BY_mm,BZ_mm\n");
    for(int i=0;!error&&i<span.count;i++){
        double a=span.start+i*span.pitch,b=a+span.segment;
        ProVector p,q;
        cm_point(span.first,span.axis,a/mm,p);
        cm_point(span.first,span.axis,b/mm,q);
        if(fwprintf(file,L"%d,%ls,%ls,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g\n",i+1,cmd->arg[1],
            cmd->arg[3],a,b,span.segment,p[0]*mm,p[1]*mm,p[2]*mm,q[0]*mm,q[1]*mm,q[2]*mm)<0)error=PRO_TK_CANT_WRITE;
    }
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"沿原生 %ls 的 %.8g mm 直边输出 %d 段，焊缝总长 %.8g mm。坐标和端部留量已核对，接头适用性由工艺复核。%ls\n",
        cmd->arg[3],span.length,span.count,span.count*span.segment,work.target);
    return error;
}
