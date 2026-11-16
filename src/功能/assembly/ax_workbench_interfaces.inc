static ProError aw_interface_check(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent owner={0};
    AxDeviceFrame frame;
    AxQualityFace mount={0};
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&owner);
    if(!error)error=ax_device_frame(&owner,cmd->arg[3],1,&frame);
    if(!error)error=ax_q_face(assembly,cmd->arg[2],cmd->arg[4],mm,&mount);
    if(error)return error;
    double low[3],high[3];
    if((error=aw_project_face(&mount,&frame,mm,low,high)))return error;
    const double nominal=as_arg(cmd,5),deviation=fmax(fabs(low[2]-nominal),fabs(high[2]-nominal));
    const double angle=ax_q_angle(mount.plane.normal,frame.axis[2]);
    int bad=!ax_q_within(deviation,as_arg(cmd,6))||!ax_q_within(angle,as_arg(cmd,7));
    part_text_add(out,L"接口安装面高度 %.8g～%.8g mm，名义 %.8g mm，最大偏差 %.8g mm，倾角 %.8g°。%ls。\n",
        low[2],high[2],nominal,deviation,angle,bad?L"安装面不符":L"安装面符合");
    for(int i=8;i<cmd->count;i+=5){
        ProVector point,axis;
        if((error=ax_q_bore(assembly,cmd->arg[2],cmd->arg[i],mm,point,axis)))return error;
        const double expected_x=as_arg(cmd,i+1),expected_y=as_arg(cmd,i+2);
        const double dot=ad_dot(axis,frame.axis[2]);
        if(fabs(dot)<1e-6)return PRO_TK_BAD_CONTEXT;
        const double distance=(nominal/mm-ax_project(point,&frame,2))/dot;
        for(int j=0;j<3;j++)point[j]+=distance*axis[j];
        const double x=ax_project(point,&frame,0)*mm,y=ax_project(point,&frame,1)*mm;
        const double offset=hypot(x-expected_x,y-expected_y),slope=ax_q_angle(axis,frame.axis[2]);
        const int pass=ax_q_within(offset,as_arg(cmd,i+3))&&ax_q_within(slope,as_arg(cmd,i+4));
        bad+=!pass;
        part_text_add(out,L"%ls 投到接口标高的孔心 (%.8g, %.8g) mm，偏位 %.8g mm，孔轴偏角 %.8g°。%ls。\n",
            cmd->arg[i],x,y,offset,slope,pass?L"符合":L"不符合");
    }
    part_text_add(out,L"核对 1 个安装面与 %d 个指定孔；%d 项不符。未指定的接口特征未计入。\n",(cmd->count-8)/5,bad);
    return PRO_TK_NO_ERROR;
}

static ProError aw_layout_preview(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxLayoutGraph *graph=calloc(1,sizeof(*graph));
    AxScan *scan=NULL;
    if(!graph)return PRO_TK_OUT_OF_MEMORY;
    AxComponent control={0};
    UxDimension dimension;
    double scale;
    ProError error=ax_ref_occurrence(assembly,cmd->arg[2],&control);
    if(!error)error=qa_mm_per_unit(control.model,&scale);
    if(!error)error=ux_dimension(control.model,cmd->arg[3],&dimension);
    if(!error)error=ax_scan(assembly,0,&scan);
    if(error)goto done;
    ProDimensiontype type;
    ProBoolean relation;
    if((error=ProDimensionTypeGet(&dimension.dim,&type))||(error=ProDimensionIsReldriven(&dimension.dim,&relation)))goto done;
    if(type!=PRODIMTYPE_LINEAR||relation){error=PRO_TK_BAD_CONTEXT;goto done;}
    const int controller=ax_layout_graph_node(graph,control.model);
    graph->limited=scan->limited;
    for(int i=0;i<scan->count;i++)if(scan->occurrences[i].model)
        ax_layout_graph_node(graph,scan->occurrences[i].model);
    for(int i=0;i<graph->count;i++)ax_layout_graph_model(graph,i,out,0);
    ax_layout_graph_close(graph);
    const double original=dimension.value*scale,proposed=as_arg(cmd,4);
    part_text_add(out,L"实测控制尺寸 %ls 当前 %.8g mm，拟变为 %.8g mm，差 %+.8g mm。只预览可读取的引用关系。\n",
        cmd->arg[3],original,proposed,proposed-original);
    int candidates=0,placement=0;
    const int depth=control.path.table_num;
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model||item->state!=PRO_FEAT_ACTIVE||item->model==control.model)continue;
        int graph_index=0;
        while(graph_index<graph->count&&graph->models[graph_index]!=item->model)graph_index++;
        int effect=graph_index<graph->count&&graph->edges[graph_index][controller];
        if(!effect&&depth<=item->depth){
            wchar_t path[256];
            if(!ax_bom_path(item,path))continue;
            AxComponent component={0};
            ProAsmcomppath parent;
            if(ax_component_resolve(assembly,path,&component)||
               ProAsmcomppathInit((ProSolid)assembly,item->ids,item->depth-1,&parent))continue;
            ProAsmcompconstraint *rows=NULL;
            int count=0;
            if(!ax_read_constraints(&component.feature,&parent,&rows,&count)){
                for(int j=0;j<count&&!effect;j++){
                    ProSelection ref=NULL;
                    ProModelitem geometry;
                    ProDatumside side;
                    if(!ProAsmcompconstraintAsmreferenceGet(rows[j],&ref,&side)&&ref&&!ProSelectionModelitemGet(ref,&geometry)&&
                       geometry.owner==control.model){effect=1;placement++;}
                }
            }
            if(rows)ProAsmcompconstraintArrayFree(rows);
        }
        if(effect){
            wchar_t path[256];
            if(candidates++<70&&ax_bom_path(item,path))part_text_add(out,L"%ls %ls 通过外部参照或定位引用该控制模型，需复核随动。\n",path,item->name);
        }
    }
    part_text_add(out,L"共 %d 个候选受影响实例，其中直接定位引用 %d 个。失效/不可读引用 %d，未加载实例 %d。实际改尺寸并再生后才可确认变动。\n",
        candidates,placement,graph->missing+graph->unavailable,scan->unavailable);
    if(graph->limited)part_text_add(out,L"依赖图超出扫描上限。\n");
done:
    free(scan);free(graph);
    return error;
}

static ProError aw_interface_draw(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame reference;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[3],&reference);
    if(error)return error;
    ProOutline box;
    if((error=aw_part_bounds(assembly,L"ROOT",&reference,mm,box)))return error;
    int count=(cmd->count-4)/3;
    double positions[AX_BATCH_LIMIT][3];
    for(int i=0;i<count;i++){
        int offset=4+3*i;
        AxDeviceFrame feature;
        if((error=aw_frame_world(assembly,cmd->arg[offset+1],cmd->arg[offset+2],&feature)))return error;
        for(int j=0;j<3;j++)positions[i][j]=ax_project(feature.origin,&reference,j)*mm;
    }
    double width=(box[1][0]-box[0][0])*mm,depth=(box[1][1]-box[0][1])*mm;
    if(width<=0||depth<=0)return PRO_TK_BAD_CONTEXT;
    DeliveryOutput output={0};
    if((error=dv_stage_begin(cmd->arg[2],&output)))return error;
    FILE *svg=NULL,*csv=NULL;
    if((error=ax_bom_open(output.stage,L"assembly-interfaces.svg",&svg))||
       (error=ax_bom_open(output.stage,L"assembly-interfaces.csv",&csv)))goto done;
    const double zoom=fmin(580/width,300/depth);
    int written=fwprintf(svg,L"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 720 %d\">\n"
        L"<rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/>\n"
        L"<text x=\"28\" y=\"29\" fill=\"#143247\" font-size=\"18\">装配接口 · %ls · 平面投影示意</text>\n"
        L"<rect x=\"62\" y=\"56\" width=\"%.5g\" height=\"%.5g\" fill=\"#eff4f6\" stroke=\"#38566a\"/>\n"
        L"<text x=\"58\" y=\"385\" font-size=\"13\" fill=\"#143247\">外廓 %.8g × %.8g × %.8g mm；基准 %ls</text>\n",
        440+23*count,cmd->arg[1],width*zoom,depth*zoom,width,depth,(box[1][2]-box[0][2])*mm,cmd->arg[3])>0&&
        fwprintf(csv,L"接口,实例,基准,X_mm,Y_mm,Z_mm\n")>0;
    for(int i=0;written&&i<count;i++){
        int offset=4+3*i;
        double x=62+(positions[i][0]-box[0][0]*mm)*zoom,y=56+(box[1][1]*mm-positions[i][1])*zoom;
        wchar_t coords[3][32];
        for(int j=0;j<3;j++)swprintf_s(coords[j],_countof(coords[j]),L"%.10g",positions[i][j]);
        const wchar_t *columns[]={cmd->arg[offset],cmd->arg[offset+1],cmd->arg[offset+2],coords[0],coords[1],coords[2]};
        written=fwprintf(svg,L"<circle cx=\"%.5g\" cy=\"%.5g\" r=\"4\" fill=\"#1d6492\"/>"
            L"<text x=\"%.5g\" y=\"%.5g\" font-size=\"12\">%ls</text>\n"
            L"<text x=\"58\" y=\"%d\" font-size=\"13\">%ls %ls (%.8g, %.8g, %.8g) mm</text>\n",
            x,y,x+7,y-4,cmd->arg[offset],412+23*i,cmd->arg[offset],cmd->arg[offset+1],
            positions[i][0],positions[i][1],positions[i][2])>0&&ax_bom_csv(csv,columns,_countof(columns));
    }
    if(!written||fputws(L"</svg>\n",svg)==WEOF)error=PRO_TK_CANT_WRITE;
done:
    if(svg&&fclose(svg)&&!error)error=PRO_TK_CANT_WRITE;
    if(csv&&fclose(csv)&&!error)error=PRO_TK_CANT_WRITE;
    error=dv_stage_finish(&output,error,out);
    if(!error)part_text_add(out,L"已输出 %d 个实际接口的 XY 投影示意及 XYZ 坐标表：%ls。\n",count,output.target);
    return error;
}
