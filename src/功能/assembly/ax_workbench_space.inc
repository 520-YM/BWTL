static ProError aw_part_bounds(ProMdl assembly,const wchar_t *token,const AxDeviceFrame *frame,double mm,ProOutline bounds){
    AxComponent selected={0};
    ProError error=ax_ref_occurrence(assembly,token,&selected);
    if(error)return error;
    ProMdlType type;
    if((error=ProMdlTypeGet(selected.model,&type)))return error;
    if(type==PRO_MDL_PART){
        double scale;
        if((error=qa_mm_per_unit(selected.model,&scale)))return error;
        if(!part_close(scale,mm))return PRO_TK_BAD_CONTEXT;
        return ax_device_world_bounds(&selected,frame,bounds);
    }
    if(type!=PRO_MDL_ASSEMBLY)return PRO_TK_INVALID_TYPE;
    AxScan *scan=NULL;
    error=ax_scan(assembly,1,&scan);
    if(error)return error;
    for(int axis=0;axis<3;axis++){bounds[0][axis]=INFINITY;bounds[1][axis]=-INFINITY;}
    int count=0;
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        int length=selected.path.table_num;
        if(item->type!=PRO_MDL_PART||item->state!=PRO_FEAT_ACTIVE||item->depth<=length||
           memcmp(item->ids,selected.path.comp_id_table,length*sizeof(item->ids[0])))continue;
        wchar_t path[256];
        if(!ax_bom_path(item,path)){error=PRO_TK_INCOMPLETE;break;}
        ProOutline box;
        AxComponent leaf={0};
        if((error=ax_hw_occurrence(assembly,path,mm,&leaf))||
           (error=ax_device_world_bounds(&leaf,frame,box)))break;
        count++;
        for(int j=0;j<3;j++){
            bounds[0][j]=fmin(bounds[0][j],box[0][j]);
            bounds[1][j]=fmax(bounds[1][j],box[1][j]);
        }
    }
    free(scan);
    return error?error:count?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError aw_available_rail(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame frame;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&frame);
    if(error)return error;
    const double x1=as_arg(cmd,3),x2=as_arg(cmd,4),y1=as_arg(cmd,5),y2=as_arg(cmd,6),z1=as_arg(cmd,7),z2=as_arg(cmd,8);
    const double width=as_arg(cmd,9),pitch=as_arg(cmd,10);
    if(x2<=x1||y2<=y1||z2<=z1||pitch<width)return PRO_TK_BAD_INPUTS;
    double closed[AX_BATCH_LIMIT][2];
    int count=0;
    for(int i=11;i<cmd->count;i++){
        ProOutline box;
        if((error=aw_part_bounds(assembly,cmd->arg[i],&frame,mm,box)))return error;
        if(box[1][1]*mm<=y1||box[0][1]*mm>=y2||box[1][2]*mm<=z1||box[0][2]*mm>=z2)continue;
        const double from=fmax(x1,box[0][0]*mm),to=fmin(x2,box[1][0]*mm);
        if(to<=from)continue;
        int at=count;
        while(at>0&&closed[at-1][0]>from){
            memcpy(closed[at],closed[at-1],sizeof(closed[at]));
            at--;
        }
        closed[at][0]=from;closed[at][1]=to;count++;
    }
    int quantity=0;
    double current=x1;
    for(int i=0;i<=count;i++){
        double next=i<count?closed[i][0]:x2;
        if(next>current){
            double length=next-current;
            int fit=length<width-1e-7?0:1+(int)floor((length-width+1e-7)/pitch);
            quantity+=fit;
            part_text_add(out,L"空段 X %.8g～%.8g mm，长 %.8g mm。按模块宽 %.8g / 节距 %.8g，可再放 %d 个。\n",
                current,next,length,width,pitch,fit);
        }
        if(i<count)current=fmax(current,closed[i][1]);
    }
    part_text_add(out,L"指定导轨截面、占用实例和节距下，预计可再放 %d 个模块。可用深度 %.8g mm，高度 %.8g mm。\n",
        quantity,y2-y1,z2-z1);
    return PRO_TK_NO_ERROR;
}

static ProError aw_electric_layout(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame frame;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&frame);
    if(error)return error;
    const double minx=as_arg(cmd,3),miny=as_arg(cmd,4),maxx=as_arg(cmd,5),maxy=as_arg(cmd,6);
    const double clear=as_arg(cmd,7);
    if(maxx<=minx||maxy<=miny)return PRO_TK_BAD_INPUTS;
    double x=minx+clear,y=miny+clear,shelf=0;
    int placed=0,blocked=0;
    for(int i=8;i<cmd->count;i+=3){
        AxComponent module={0};
        AxDeviceFrame local;
        ProOutline bounds;
        error=ax_hw_occurrence(assembly,cmd->arg[i],mm,&module);
        if(!error)error=ax_device_frame(&module,cmd->arg[i+1],0,&local);
        if(!error)error=ax_device_bounds(module.model,&local,bounds);
        if(error)return error;
        const wchar_t *port=cmd->arg[i+2];
        double widths[2]={(bounds[1][0]-bounds[0][0])*mm,(bounds[1][1]-bounds[0][1])*mm};
        const int swap=port[1]==L'X';
        const double width=widths[swap],depth=widths[1-swap];
        if(width+2*clear>maxx-minx||depth+2*clear>maxy-miny){
            blocked++;part_text_add(out,L"%ls 外廓 %.8g×%.8g mm，超出给定安装区。\n",cmd->arg[i],width,depth);continue;
        }
        if(x+width+clear>maxx+1e-6){y+=shelf+clear;x=minx+clear;shelf=0;}
        if(y+depth+clear>maxy+1e-6){
            blocked++;part_text_add(out,L"%ls 在给定顺序和边距下未排入，需调整顺序或扩大区域。\n",cmd->arg[i]);continue;
        }
        const int angle=swap?(port[0]==L'+'?90:-90):(port[0]==L'+'?0:180);
        part_text_add(out,L"%ls 建议外廓左下 (%.8g, %.8g)，占区 %.8g×%.8g mm，绕局部 +Z 转 %d° 后端子轴朝布局 +Y。\n",
            cmd->arg[i],x,y,width,depth,angle);
        placed++;
        shelf=fmax(shelf,depth);
        x+=width+clear;
    }
    part_text_add(out,L"已排 %d 个，未排 %d 个，边及间隔 %.8g mm。以 %ls 为布局基准，当前模型位姿保持。\n",
        placed,blocked,clear,cmd->arg[2]);
    return PRO_TK_NO_ERROR;
}

static ProError aw_transport(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame frame;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[3],&frame);
    if(error)return error;
    ProOutline bounds;
    if((error=aw_part_bounds(assembly,cmd->arg[2],&frame,mm,bounds)))return error;
    double length=(bounds[1][0]-bounds[0][0])*mm,width=(bounds[1][1]-bounds[0][1])*mm,height=(bounds[1][2]-bounds[0][2])*mm;
    const double pad=as_arg(cmd,4);
    for(int i=5;i<8;i++){
        double padding=2*as_arg(cmd,i);
        if(i==5)length+=padding;else if(i==6)width+=padding;else height+=padding;
    }
    part_text_add(out,L"含指定包装的外廓 X/Y/Z 为 %.8g × %.8g × %.8g mm，单边操作余量 %.8g mm。\n",length,width,height,pad);
    int failed=0;
    for(int i=8;i<cmd->count;i+=4){
        const wchar_t *type=cmd->arg[i];
        double x=as_arg(cmd,i+1),y=as_arg(cmd,i+2),z=as_arg(cmd,i+3);
        const int turned=!wcscmp(type,L"TURN");
        const double needed_x=(turned?hypot(length,width):!wcscmp(type,L"STRAIGHT_Y")?width:length)+2*pad;
        const double needed_y=(turned?hypot(length,width):!wcscmp(type,L"STRAIGHT_Y")?length:width)+2*pad;
        const double needed_z=height+2*pad;
        int pass=qa_at_least(x,needed_x)&&qa_at_least(y,needed_y)&&qa_at_least(z,needed_z);
        failed+=!pass;
        part_text_add(out,L"第 %d 段 %ls，剩余空间 X/Y/Z %.8g / %.8g / %.8g mm。%ls。\n",(i-8)/4+1,
            turned?L"矩形原地转向区":L"直线通过区",x-needed_x,y-needed_y,z-needed_z,pass?L"几何包络符合":L"该包络余量不足");
    }
    part_text_add(out,L"交通空间核对 %d 段，%d 段不足。转角按固定中心水平转动的保守圆形包络计算。\n",(cmd->count-8)/4,failed);
    return PRO_TK_NO_ERROR;
}
