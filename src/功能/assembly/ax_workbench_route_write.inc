#define AW_ROUTE_SIDE 12
#define AW_ROUTE_NODES (AW_ROUTE_SIDE*AW_ROUTE_SIDE*AW_ROUTE_SIDE)
#define AW_ROUTE_SEGMENTS 20

typedef struct {double value[AW_ROUTE_SIDE];int count;} AwRouteAxis;
typedef struct {ProVector low,high;} AwRouteBlock;

static void aw_route_coordinate(AwRouteAxis *axis,double value){
    for(int i=0;i<axis->count;i++)if(fabs(axis->value[i]-value)<1e-8)return;
    if(axis->count<AW_ROUTE_SIDE)axis->value[axis->count++]=value;
}

static void aw_route_point_at(const AwRouteAxis axes[3],const int index[3],ProVector p){
    for(int i=0;i<3;i++)p[i]=axes[i].value[index[i]];
}

static int aw_route_id(const int index[3]){
    return (index[0]*AW_ROUTE_SIDE+index[1])*AW_ROUTE_SIDE+index[2];
}
static void aw_route_indices(int id,int index[3]){
    index[0]=id/(AW_ROUTE_SIDE*AW_ROUTE_SIDE);
    index[1]=(id/AW_ROUTE_SIDE)%AW_ROUTE_SIDE;
    index[2]=id%AW_ROUTE_SIDE;
}

static int aw_route_clear(AwRouteBlock *blocks,int count,ProVector from,ProVector to){
    for(int j=0;j<count;j++){
        int intersects=1;
        for(int i=0;i<3;i++)
            if(fmax(from[i],to[i])<blocks[j].low[i]||fmin(from[i],to[i])>blocks[j].high[i])intersects=0;
        if(intersects)return 0;
    }
    return 1;
}

static ProError aw_routing_grid(AwRouteAxis axes[3],AwRouteBlock *blocks,int count,ProVector start,ProVector finish,
                                ProVector points[AW_ROUTE_SEGMENTS+1],int *length){
    int si[3],fi[3];
    for(int i=0;i<3;i++){
        qsort(axes[i].value,axes[i].count,sizeof(double),aw_number_order);
        si[i]=fi[i]=-1;
        for(int j=0;j<axes[i].count;j++){
            if(fabs(axes[i].value[j]-start[i])<1e-8)si[i]=j;
            if(fabs(axes[i].value[j]-finish[i])<1e-8)fi[i]=j;
        }
        if(si[i]<0||fi[i]<0)return PRO_TK_INCOMPLETE;
    }
    int origin=aw_route_id(si),target=aw_route_id(fi);
    double distance[AW_ROUTE_NODES];
    int previous[AW_ROUTE_NODES];
    unsigned char seen[AW_ROUTE_NODES]={0};
    for(int i=0;i<AW_ROUTE_NODES;i++){previous[i]=-1;distance[i]=INFINITY;}
    if(origin==target||!aw_route_clear(blocks,count,start,start)||!aw_route_clear(blocks,count,finish,finish))return PRO_TK_BAD_CONTEXT;
    distance[origin]=0;
    for(;;){
        int next=-1;
        double best=INFINITY;
        for(int i=0;i<AW_ROUTE_NODES;i++)
            if(!seen[i]&&distance[i]<best){best=distance[i];next=i;}
        if(next<0||next==target)break;
        seen[next]=1;
        int index[3];
        ProVector point;
        aw_route_indices(next,index);
        aw_route_point_at(axes,index,point);
        for(int direction=0;direction<6;direction++){
            const int axis=direction/2,step=direction%2?1:-1;
            const int old=index[axis],candidate=old+step;
            if(candidate<0||candidate>=axes[axis].count)continue;
            index[axis]=candidate;
            const int other=aw_route_id(index);
            index[axis]=old;
            if(seen[other])continue;
            ProVector p={point[0],point[1],point[2]};
            p[axis]=axes[axis].value[candidate];
            if(!aw_route_clear(blocks,count,point,p))continue;
            const double traveled=best+fabs(p[axis]-point[axis]);
            if(traveled<distance[other]){distance[other]=traveled;previous[other]=next;}
        }
    }
    if(previous[target]<0)return PRO_TK_E_NOT_FOUND;
    int vertices[AW_ROUTE_NODES],n=0;
    for(int i=target;i>=0&&n<AW_ROUTE_NODES;i=previous[i])vertices[n++]=i;
    if(n<2||vertices[n-1]!=origin)return PRO_TK_INCOMPLETE;
    memcpy(points[0],start,sizeof(ProVector));
    *length=1;
    int last_axis=-1;
    for(int i=n-2;i>=0;i--){
        int next[3],prior[3];
        aw_route_indices(vertices[i],next);aw_route_indices(vertices[i+1],prior);
        int axis=0;
        while(axis<3&&next[axis]==prior[axis])axis++;
        if(axis==3)return PRO_TK_INCOMPLETE;
        if(axis!=last_axis){
            if(*length>=AW_ROUTE_SEGMENTS+1)return PRO_TK_INCOMPLETE;
            (*length)++;
        }
        aw_route_point_at(axes,next,points[*length-1]);
        last_axis=axis;
    }
    return PRO_TK_NO_ERROR;
}

static ProError aw_build_route(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxDeviceFrame reference,endpoints[2];
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[3],&reference);
    if(!error)error=aw_frame_world(assembly,cmd->arg[6],cmd->arg[7],&endpoints[0]);
    if(!error)error=aw_frame_world(assembly,cmd->arg[8],cmd->arg[9],&endpoints[1]);
    if(error)return error;
    AwRouteAxis axes[3]={0};
    ProVector from,to,low,high;
    for(int i=0;i<3;i++){
        from[i]=ax_project(endpoints[0].origin,&reference,i)*mm;
        to[i]=ax_project(endpoints[1].origin,&reference,i)*mm;
        low[i]=as_arg(cmd,10+i);high[i]=as_arg(cmd,13+i);
        if(low[i]>=high[i]||from[i]<low[i]||from[i]>high[i]||to[i]<low[i]||to[i]>high[i])return PRO_TK_BAD_INPUTS;
        aw_route_coordinate(&axes[i],from[i]);aw_route_coordinate(&axes[i],to[i]);
        aw_route_coordinate(&axes[i],low[i]);aw_route_coordinate(&axes[i],high[i]);
    }
    AwRouteBlock blocks[4]={0};
    const double clearance=as_arg(cmd,4)*0.5+as_arg(cmd,5);
    int count=cmd->count-16;
    for(int i=0;i<count;i++){
        ProOutline box;
        if((error=aw_part_bounds(assembly,cmd->arg[16+i],&reference,mm,box)))return error;
        for(int axis=0;axis<3;axis++){
            const double a=box[0][axis]*mm-clearance,b=box[1][axis]*mm+clearance;
            blocks[i].low[axis]=a;blocks[i].high[axis]=b;
            if(a-0.02>low[axis]&&a-0.02<high[axis])aw_route_coordinate(&axes[axis],a-0.02);
            if(b+0.02>low[axis]&&b+0.02<high[axis])aw_route_coordinate(&axes[axis],b+0.02);
        }
    }
    ProVector points[AW_ROUTE_SEGMENTS+1];
    int length=0;
    error=aw_routing_grid(axes,blocks,count,from,to,points,&length);
    if(error){
        part_text_add(out,L"没有从所给起终点绕开已选障碍包络且留足线径和间隙的正交路径。\n");return error;
    }

    ProName segment[AW_ROUTE_SEGMENTS];
    int added[AW_ROUTE_SEGMENTS]={0},written=0;
    for(int i=0;i<length-1;i++){
        swprintf_s(segment[i],_countof(segment[i]),L"%ls_%02d",cmd->arg[2],i+1);
        if((error=ux_new_name(assembly,segment[i])))return error;
    }
    AwAssemblyState before={0};
    FdTxn features={0};
    if((error=aw_state_capture(assembly,&before)))return error;
    error=ProSolidFeatVisit((ProSolid)assembly,ax_public_feature,NULL,&features);
    for(int i=0;!error&&i<length-1;i++){
        wchar_t line[640];
        const double *a=points[i],*b=points[i+1];
        swprintf_s(line,_countof(line),L"CREO_OP DATUM_CURVE TARGET %ls %ls %.10g %.10g %.10g %.10g %.10g %.10g",
            segment[i],cmd->arg[3],a[0],a[1],a[2],b[0],b[1],b[2]);
        error=ax_layout_token_datum(assembly,line);
        ProFeature feature;
        ProError lookup=ux_named_feature(assembly,segment[i],&feature);
        if(!lookup)added[written++]=feature.id;
        if(!error&&lookup)error=lookup;
        if(!error){
            ProGeomitem raw;
            ProCurve curve;
            double distance;
            error=ad_single_curve(&feature,&raw,&curve);
            if(!error)error=ProCurveLengthEval(curve,&distance);
            if(!error&&fabs(distance*mm-aw_distance3(a,b))>0.0001)error=PRO_TK_INCOMPLETE;
        }
    }
    if(!error)error=aw_state_verify(assembly,&before,0,mm,out);
    if(error){
        int uncertain=0;
        for(int i=written-1;i>=0;i--)
            if(model_remove_feature((ProSolid)assembly,added[i]))uncertain=1;
        if(part_regenerate(assembly)||ax_verify_multi_delete(assembly,&features,NULL,0)||
           aw_state_verify(assembly,&before,0,mm,out))uncertain=1;
        if(uncertain)ax_uncertain(out);
        else part_text_add(out,L"已确认清理本次新增路径段，原装配状态保持。\n");
    }else{
        double total=0;
        for(int i=0;i<length-1;i++){
            double distance=aw_distance3(points[i],points[i+1]);
            total+=distance;part_text_add(out,L"CURVE:%ls 原生线段 %.8g mm。\n",segment[i],distance);
        }
        part_text_add(out,L"已生成并再生核对 %d 段基准中心线，总长 %.8g mm。避开所给障碍包络；弯头半径需按线规圆顺。未保存。\n",length-1,total);
    }
    aw_state_free(&before);
    return error;
}

static ProError aw_place_clips(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    ProVector course[2],along;
    ProError error=aw_line_curve(assembly,cmd->arg[4],cmd->arg[5],mm,course);
    if(error)return error;
    ad_sub(course[1],course[0],along);
    const double length=sqrt(ad_dot(along,along))*mm;
    if(!ad_unit(along))return PRO_TK_BAD_CONTEXT;
    const double first=as_arg(cmd,7),pitch=as_arg(cmd,8),edge=as_arg(cmd,9);
    const int count=(int)as_arg(cmd,10);
    if(first<edge||first+(count-1)*pitch>length-edge)return PRO_TK_BAD_INPUTS;
    AxComponent model={0},obstacle={0};
    AxDeviceFrame origin,dest={0},ref;
    int loaded=0;
    error=ax_batch_model(cmd->arg[2],mm,&model.model,&loaded);
    if(!error)error=ax_device_frame(&model,cmd->arg[3],0,&origin);
    if(!error)error=aw_frame_world(assembly,L"ROOT",cmd->arg[6],&ref);
    if(!error&&wcscmp(cmd->arg[11],L"NONE"))error=ax_hw_occurrence(assembly,cmd->arg[11],mm,&obstacle);
    if(error)goto done;
    memcpy(dest.axis[0],along,sizeof(ProVector));
    ad_cross(ref.axis[2],along,dest.axis[1]);
    if(!ad_unit(dest.axis[1])){error=PRO_TK_BAD_CONTEXT;goto done;}
    ad_cross(dest.axis[0],dest.axis[1],dest.axis[2]);
    AxBatchItem items[AX_BATCH_LIMIT]={0};
    for(int i=0;i<count;i++){
        for(int j=0;j<3;j++)dest.origin[j]=course[0][j]+(first+i*pitch)*along[j]/mm;
        items[i].model=model.model;
        ax_device_align(&origin,&dest,items[i].pose);
    }
    error=ax_device_create(assembly,items,count,mm,obstacle.model?&obstacle:NULL,NULL,NULL,NULL,out);
    if(!error)part_text_add(out,L"沿所选直线路径布置 %d 个现有线夹，首距 %.8g mm、节距 %.8g mm。已核对定位，未保存。\n",count,first,pitch);
done:
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(model.model);
    return error;
}
