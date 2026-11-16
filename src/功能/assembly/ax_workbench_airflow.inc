typedef struct {double low[2],high[2];} AwRectangle;

static int aw_number_order(const void *left,const void *right){
    const double a=*(const double *)left,b=*(const double *)right;
    return (a>b)-(a<b);
}

static double aw_rectangle_union(AwRectangle *rectangles,int count){
    double x[AX_BATCH_LIMIT*2];
    for(int i=0;i<count;i++){
        x[i*2]=rectangles[i].low[0];
        x[i*2+1]=rectangles[i].high[0];
    }
    qsort(x,count*2,sizeof(x[0]),aw_number_order);
    double area=0;
    for(int i=1;i<count*2;i++){
        if(x[i]<=x[i-1])continue;
        const double middle=(x[i]+x[i-1])*0.5;
        AwRectangle active[AX_BATCH_LIMIT];
        int n=0;
        for(int k=0;k<count;k++){
            AwRectangle item=rectangles[k];
            if(item.low[0]>middle||item.high[0]<middle)continue;
            int at=n;
            while(at&&active[at-1].low[1]>item.low[1]){
                active[at]=active[at-1];
                at--;
            }
            active[at]=item;
            n++;
        }
        double y=0,end=-INFINITY;
        for(int k=0;k<n;k++){
            const double next=active[k].high[1];
            if(next<=end)continue;
            y+=next-fmax(end,active[k].low[1]);
            end=next;
        }
        area+=(x[i]-x[i-1])*y;
    }
    return area;
}

static ProError aw_rectangular_opening(ProMdl assembly,const PartCommand *cmd,int index,double mm,
                                       AxDeviceFrame *world,double opening[4],double z[2]){
    AxComponent frame={0};
    AxDeviceFrame local;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[index],mm,&frame);
    if(!error)error=ax_device_frame(&frame,cmd->arg[index+1],0,&local);
    if(!error)error=ax_device_frame(&frame,cmd->arg[index+1],1,world);
    if(error)return error;
    if(index==2)return ax_rect_opening(&frame,cmd,&local,mm,opening,z);
    PartCommand adapted={0};
    for(int i=0;i<4;i++)wcscpy_s(adapted.arg[4+i],PART_TOKEN_MAX,cmd->arg[index+2+i]);
    return ax_rect_opening(&frame,&adapted,&local,mm,opening,z);
}

static ProError aw_airflow(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    double opening[4],z[2];
    AxDeviceFrame section;
    ProError error=aw_rectangular_opening(assembly,cmd,2,mm,&section,opening,z);
    if(error)return error;
    AwRectangle blocks[AX_BATCH_LIMIT];
    int count=0,unknown=0;
    const double z_at=as_arg(cmd,8)/mm;
    if(z_at<z[0]||z_at>z[1]){
        part_text_add(out,L"选择的截面不在风道四壁共同覆盖的深度内。\n");return PRO_TK_BAD_INPUTS;
    }
    for(int i=10;i<cmd->count;i++){
        AxComponent obstacle={0};
        ProOutline box;
        error=ax_hw_occurrence(assembly,cmd->arg[i],mm,&obstacle);
        if(!error)error=ax_device_world_bounds(&obstacle,&section,box);
        if(error){unknown++;part_text_add(out,L"%ls 的风道占用未核实（Creo %d）。\n",cmd->arg[i],error);continue;}
        if(z_at<=box[0][2]+1e-7||z_at>=box[1][2]-1e-7)continue;
        ProMassProperty solid;
        error=qa_geometry_mass(obstacle.model,&solid);
        const double volume=(box[1][0]-box[0][0])*(box[1][1]-box[0][1])*(box[1][2]-box[0][2]);
        if(error||!isfinite(volume)||volume<0.001||!part_close(solid.volume,volume)){
            unknown++;
            part_text_add(out,L"%ls 穿过检查截面，实际形状无法按轴对齐实心方盒核实。\n",cmd->arg[i]);continue;
        }
        AwRectangle item;
        for(int j=0;j<2;j++){
            item.low[j]=fmax(opening[2*j],box[0][j]);
            item.high[j]=fmin(opening[2*j+1],box[1][j]);
        }
        if(item.low[0]>=item.high[0]||item.low[1]>=item.high[1])continue;
        blocks[count++]=item;
        part_text_add(out,L"%ls 在该截面占用 %.8g mm²。\n",cmd->arg[i],
            (item.high[0]-item.low[0])*(item.high[1]-item.low[1])*mm*mm);
    }
    const double gross=(opening[1]-opening[0])*(opening[3]-opening[2])*mm*mm;
    const double occupied=aw_rectangle_union(blocks,count)*mm*mm;
    const double net=fmax(0,gross-occupied),minimum=as_arg(cmd,9);
    part_text_add(out,L"截面 Z %.8g mm，毛面积 %.8g mm²，已核实阻挡面积 %.8g mm²。%ls %.8g mm²（要求 ≥ %.8g）。\n",
        z_at*mm,gross,occupied,unknown?L"可用面积上界":L"当前已选范围净截面",net,minimum);
    if(unknown)part_text_add(out,L"有 %d 个障碍未核实。需要更明确的实体截面后才能确认结果。\n",unknown);
    else part_text_add(out,L"%ls。\n",qa_at_least(net,minimum)?L"截面积符合":L"截面积不足");
    return unknown?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static ProError aw_project_face(AxQualityFace *face,const AxDeviceFrame *world,double mm,double low[3],double high[3]){
    AxDevicePlane direction={0};
    memcpy(direction.point,world->origin,sizeof(ProVector));
    for(int i=0;i<3;i++){
        memcpy(direction.normal,world->axis[i],sizeof(ProVector));
        ProError error=ax_q_span(face,&direction,mm,&low[i],&high[i]);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError aw_display_view(ProMdl assembly,const PartCommand *cmd,double mm,int kind,PartText *out){
    double opening[4],z[2];
    AxDeviceFrame frame;
    ProError error=aw_rectangular_opening(assembly,cmd,2,mm,&frame,opening,z);
    AxQualityFace display={0};
    if(!error)error=ax_q_face(assembly,cmd->arg[8],cmd->arg[9],mm,&display);
    if(error)return error;
    if(ax_q_angle(frame.axis[2],display.plane.normal)>0.001)return PRO_TK_BAD_CONTEXT;
    double surface[3],other[3],area;
    if((error=aw_project_face(&display,&frame,mm,surface,other))||
       (error=ProSurfaceAreaEval(display.plane.handle,&area)))return error;
    const double rectangle=(other[0]-surface[0])*(other[1]-surface[1]);
    if(rectangle<0.001||!part_close(rectangle,area*mm*mm))return PRO_TK_BAD_CONTEXT;
    double base[4]={surface[0],other[0],surface[1],other[1]};
    const wchar_t *sides[]={L"左",L"右",L"下",L"上"};
    int failed=0;
    if(kind==AW_DISPLAY_VIEW){
        const double x=as_arg(cmd,10),y=as_arg(cmd,11);
        const double view[]={tan(x*AS_PI/180),tan(y*AS_PI/180)};
        const double thickness_low=z[0]*mm-surface[2],thickness_high=z[1]*mm-surface[2];
        if(thickness_low<-.001)return PRO_TK_BAD_CONTEXT;
        for(int i=0;i<4;i++){
            const double a=base[i]+thickness_low*view[i/2],b=base[i]+thickness_high*view[i/2];
            const double visible=i%2?fmax(a,b):fmin(a,b);
            const double gap=(opening[i]*mm-visible)*(i%2?1:-1);
            failed+=gap<-.001;
            part_text_add(out,L"%ls侧视线余量 %.8g mm。%ls。\n",sides[i],gap,gap<-.001?L"有效显示区被遮挡":L"无遮挡");
        }
        part_text_add(out,L"相对窗口 X/Y 斜视角 %.8g° / %.8g°，遮挡 %d 边。按平行视线计算。\n",x,y,failed);
    }else{
        const double limit=as_arg(cmd,10);
        for(int i=0;i<4;i++){
            const double gap=(opening[i]*mm-base[i])*(i%2?1:-1);
            const int pass=gap>=-1e-7&&ax_q_within(gap,limit);
            failed+=!pass;
            part_text_add(out,L"%ls侧旁路缝隙 %.8g mm（允许 0～%.8g），%ls。\n",sides[i],gap,limit,
                pass?L"符合":gap<0?L"位置重叠，核对安装":L"旁路缝隙超出");
        }
        part_text_add(out,L"矩形几何查得 %d 边需核对。结果按指定滤网封边面计算。\n",failed);
    }
    return PRO_TK_NO_ERROR;
}

static ProError aw_rain_cover(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame frame;
    AxQualityFace hood={0},target={0};
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&frame);
    if(!error)error=ax_q_face(assembly,cmd->arg[3],cmd->arg[4],mm,&hood);
    if(!error)error=ax_q_face(assembly,cmd->arg[5],cmd->arg[6],mm,&target);
    if(error)return error;
    if(ax_q_angle(hood.plane.normal,frame.axis[2])>0.001)return PRO_TK_BAD_CONTEXT;
    double roof[3],high[3],lower[3],upper[3],area;
    if((error=aw_project_face(&hood,&frame,mm,roof,high))||
       (error=aw_project_face(&target,&frame,mm,lower,upper))||
       (error=ProSurfaceAreaEval(hood.plane.handle,&area)))return error;
    const double plan_area=(high[0]-roof[0])*(high[1]-roof[1]);
    if(plan_area<0.001||!part_close(plan_area,area*mm*mm))return PRO_TK_BAD_CONTEXT;
    const double required[]={as_arg(cmd,7),as_arg(cmd,8),as_arg(cmd,9),as_arg(cmd,10)};
    const double coverage[]={lower[0]-roof[0],high[0]-upper[0],lower[1]-roof[1],high[1]-upper[1]};
    const wchar_t *name[]={L"-X",L"+X",L"-Y",L"+Y"};
    int missed=0;
    for(int i=0;i<4;i++){
        int pass=qa_at_least(coverage[i],required[i]);
        missed+=!pass;
        part_text_add(out,L"沿 %ls 的挡雨罩外伸 %.8g mm（要求 ≥ %.8g），%ls。\n",name[i],coverage[i],required[i],pass?L"覆盖":L"露出或不足");
    }
    const double elevation=roof[2]-upper[2];
    part_text_add(out,L"罩面最低点相对目标最高点 %.8g mm，%d 个方向不足。%ls。\n",elevation,missed,
        elevation>0&&!missed?L"几何遮盖符合":L"本次几何遮盖未通过");
    return PRO_TK_NO_ERROR;
}
