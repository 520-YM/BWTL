
typedef struct {
    AxQuality *result;
    const int *ids;
    int depth;
    const wchar_t *model;
} AxQualityRegen;

static ProError ax_q_regen_feature(ProFeature *feature,ProError status,ProAppData context){
    AxQualityRegen *scan=context;
    if(status)return status;
    unsigned int flags;
    ProError error=ProFeatureStatusflagsGet(feature,&flags);
    if(error)return error;
    if(!(flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|
        PRO_FEAT_STAT_CHILD_OF_EXT_FAILED|PRO_FEAT_STAT_UNREGENERATED)))return PRO_TK_NO_ERROR;
    wchar_t label[256]=L"ROOT";
    ProFeattype type;
    ProName name=L"";
    if((error=ProFeatureTypeGet(feature,&type)))return error;
    int ids[AX_PATH_DEPTH];
    if(scan->depth)memcpy(ids,scan->ids,scan->depth*sizeof(*ids));
    int depth=scan->depth;
    if(type==PRO_FEAT_COMPONENT&&depth<AX_PATH_DEPTH)ids[depth++]=feature->id;
    if(depth)ax_path_label(ids,depth,label,_countof(label));
    ProModelitemNameGet((ProModelitem *)feature,name);
    const wchar_t *reason=flags&PRO_FEAT_STAT_FAILED?L"重生成失败":
        flags&PRO_FEAT_STAT_CHILD_OF_EXT_FAILED?L"受外部失败影响":
        flags&PRO_FEAT_STAT_CHILD_OF_FAILED?L"受父特征失败影响":L"尚未重生成";
    part_text_add(scan->result->out,L"%ls %ls 特征 %d %ls，%ls。%ls。\n",
        label,scan->model,feature->id,name,reason,ax_q_check(scan->result,0));
    return PRO_TK_NO_ERROR;
}

static void ax_q_regen_model(ProMdl model,const int *ids,int depth,const wchar_t *name,AxQuality *q){
    AxQualityRegen scan={q,ids,depth,name};
    wchar_t label[256]=L"ROOT";
    ProSolidRegenerationStatus state;
    if(depth)ax_path_label(ids,depth,label,_countof(label));
    ProError error=ProSolidRegenerationstatusGet((ProSolid)model,&state);
    if(error)ax_q_missing(q,label,error);
    else{
        const wchar_t *status=state==PRO_SOLID_REGENERATED?L"已重生成":
            state==PRO_SOLID_NEEDS_REGENERATION?L"待重生成":
            state==PRO_SOLID_CONNECT_FAILED?L"机构连接失败":L"重生成失败或警告";
        if(!depth||state!=PRO_SOLID_REGENERATED)
            part_text_add(q->out,L"%ls %ls 模型状态：%ls。%ls。\n",label,name,status,ax_q_check(q,state==PRO_SOLID_REGENERATED));
        else ax_q_check(q,1);
    }
    error=ProSolidFeatVisit((ProSolid)model,ax_q_regen_feature,NULL,&scan);
    if(error&&error!=PRO_TK_E_NOT_FOUND)ax_q_missing(q,label,error);
}

static ProError ax_q_regen(ProMdl assembly,const PartCommand *cmd,AxQuality *q){
    AxScan *scan=NULL;
    ax_q_regen_model(assembly,NULL,0,cmd->arg[1],q);
    ProError error=ax_scan(assembly,0,&scan);
    if(error)return error;
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(item->model&&(item->type==PRO_MDL_PART||item->type==PRO_MDL_ASSEMBLY))
            ax_q_regen_model(item->model,item->ids,item->depth,item->name,q);
        else if(!ax_suppressed(item->state)){
            wchar_t label[256];
            ax_path_label(item->ids,item->depth,label,_countof(label));
            ax_q_missing(q,label,PRO_TK_INCOMPLETE);
        }
    }
    if(scan->limited)ax_q_missing(q,L"装配树已达到扫描上限",PRO_TK_INCOMPLETE);
    part_text_add(q->out,L"扫描 %d 个实例，抑制或排除 %d 个。本项只读取 Creo 当前重生成状态。\n",scan->count,scan->suppressed);
    free(scan);
    return PRO_TK_NO_ERROR;
}

static ProError ax_q_pair_selections(ProMdl assembly,const wchar_t *a,const wchar_t *b,ProSelection sel[2]){
    AxComponent parts[2]={0};
    ProError error=ax_component_resolve(assembly,a,&parts[0]);
    if(!error)error=ax_component_resolve(assembly,b,&parts[1]);
    if(error)return error;
    if(ax_same_occurrence(&parts[0],&parts[1]))return PRO_TK_BAD_INPUTS;
    if((error=ax_part_selection(&parts[0],&sel[0]))||(error=ax_part_selection(&parts[1],&sel[1])))return error;
    return PRO_TK_NO_ERROR;
}

static ProError ax_q_gaps(ProMdl assembly,const PartCommand *cmd,double mm,AxQuality *q){
    static const wchar_t *const zones[]={L"屏幕",L"玻璃",L"PCB",L"机箱"};
    for(int i=2;i<cmd->count;i+=4){
        ProSelection pair[2]={0};
        ProBoolean overlap=PRO_B_FALSE;
        Pro3dPnt closest[2];
        double distance=0,minimum=as_arg(cmd,i+3);
        const wchar_t *a=cmd->arg[i+1],*b=cmd->arg[i+2];
        const wchar_t *zone=zones[ax_q_zone(cmd->arg[i])];
        ProError error=ax_q_pair_selections(assembly,a,b,pair);
        if(!error)error=ProFitClearanceCompute(pair[0],PRO_FIT_INCLUDE_NONE,pair[1],PRO_FIT_INCLUDE_NONE,
            PRO_FIT_WHOLE_SURFACE,NULL,PRO_B_FALSE,&distance,NULL,&overlap,closest);
        for(int j=0;j<2;j++)if(pair[j])ProSelectionFree(&pair[j]);
        part_text_add(q->out,L"%ls %ls ↔ %ls：",zone,a,b);
        if(error||(!overlap&&(!isfinite(distance)||distance<0))){
            ax_q_missing(q,L"最小间隙",error?error:PRO_TK_INCOMPLETE);
        }else if(overlap){
            part_text_add(q->out,L"存在实体干涉，最小间隙要求 ≥ %.8g mm。%ls。\n",minimum,ax_q_check(q,0));
        }else{
            distance*=mm;
            part_text_add(q->out,L"最小间隙 %.9g mm，要求 ≥ %.8g mm。%ls。\n",
                distance,minimum,ax_q_check(q,qa_at_least(distance,minimum)));
        }
    }
    return PRO_TK_NO_ERROR;
}

static int ax_q_pair_match(const wchar_t *a,const wchar_t *b,const wchar_t *x,const wchar_t *y){
    return (!wcscmp(a,x)&&!wcscmp(b,y))||(!wcscmp(a,y)&&!wcscmp(b,x));
}

static ProError ax_q_interference(ProMdl assembly,const PartCommand *cmd,double mm,AxQuality *q){
    AxScan *scan=NULL;
    wchar_t paths[16][256];
    ProSelection parts[16]={0};
    int count=0,allowed=0,clear=0,matched[10]={0};
    ProError error=ax_scan(assembly,1,&scan);
    if(error)return error;
    if(scan->parts<2||scan->parts>_countof(parts)){error=PRO_TK_OUT_OF_RANGE;goto done;}
    for(int i=0;i<scan->count;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model||item->type!=PRO_MDL_PART)continue;
        ProBoolean skeleton;
        if((error=ProMdlIsSkeleton(item->model,&skeleton)))goto done;
        if(skeleton)continue;
        AxComponent owner={0};
        if(!ax_path_label(item->ids,item->depth,paths[count],_countof(paths[0]))){error=PRO_TK_BAD_CONTEXT;goto done;}
        if((error=ax_component_resolve(assembly,paths[count],&owner))||
           (error=ax_part_selection(&owner,&parts[count])))goto done;
        count++;
    }
    if(count<2){error=PRO_TK_E_NOT_FOUND;goto done;}
    for(int i=2;i<cmd->count;i+=3){
        int found[2]={0};
        for(int j=0;j<count;j++){
            if(!wcscmp(paths[j],cmd->arg[i]))found[0]=1;
            if(!wcscmp(paths[j],cmd->arg[i+1]))found[1]=1;
        }
        if(!found[0]||!found[1]){error=PRO_TK_E_NOT_FOUND;goto done;}
        for(int j=2;j<i;j+=3)if(ax_q_pair_match(cmd->arg[i],cmd->arg[i+1],cmd->arg[j],cmd->arg[j+1])){
            error=PRO_TK_E_FOUND;goto done;
        }
    }
    for(int i=0;i<count;i++)for(int j=i+1;j<count;j++){
        double volume=0,limit=0;
        int entry=-1;
        for(int k=2;k<cmd->count;k+=3)if(ax_q_pair_match(paths[i],paths[j],cmd->arg[k],cmd->arg[k+1])){
            entry=(k-2)/3;limit=as_arg(cmd,k+2);break;
        }
        ProError native=ax_interference_volume(parts[i],parts[j],&volume);
        if(native){
            part_text_add(q->out,L"%ls ↔ %ls：",paths[i],paths[j]);ax_q_missing(q,L"干涉量",native);continue;
        }
        volume*=mm*mm*mm;
        if(volume<=1e-8){
            clear++;ax_q_check(q,1);continue;
        }
        int permitted=entry>=0&&ax_q_within(volume,limit);
        if(entry>=0)matched[entry]=1;
        allowed+=permitted;
        part_text_add(q->out,L"%ls ↔ %ls：实测 %.9g mm³（允许上限 %.9g），%ls，%ls。\n",
            paths[i],paths[j],volume,limit,permitted?L"登记允许":L"异常干涉",ax_q_check(q,permitted));
    }
    for(int i=2;i<cmd->count;i+=3)if(!matched[(i-2)/3])
        part_text_add(q->out,L"白名单 %ls ↔ %ls：本次未记录正体积干涉，登记上限 %.9g mm³。\n",
            cmd->arg[i],cmd->arg[i+1],as_arg(cmd,i+2));
    part_text_add(q->out,L"检查 %d 个活动实体，抑制或排除 %d 个实例。无正体积干涉 %d 对，登记允许 %d 对。\n",
        count,scan->suppressed,clear,allowed);
done:
    for(int i=0;i<_countof(parts);i++)if(parts[i])ProSelectionFree(&parts[i]);
    free(scan);
    return error;
}

static ProError ax_q_solid_bounds(AxComponent *owner,AxDeviceFrame *world,ProOutline bounds){
    ProMatrix pose,inverse;
    AxDeviceFrame frame;
    ProSolidBody *bodies=NULL;
    int count=0;
    ProError error=ax_device_pose(owner,pose);
    if(error)return error;
    ax_device_inverse(pose,inverse);
    ax_transform_frame(world,inverse,&frame);
    AxDeviceBounds info={&frame,{{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY}},0};
    error=ProSolidBodiesCollect((ProSolid)owner->model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        ProBoolean construction;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(!error)error=ProSolidBodyIsConstruction(&bodies[i],&construction);
        if(!error&&state==PRO_BODY_STATE_ACTIVE&&!construction)
            error=ProSolidBodySurfaceVisit(&bodies[i],ax_device_bound_surface,&info);
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    if(error||!info.seen)return error?error:PRO_TK_E_NOT_FOUND;
    memcpy(bounds,info.box,sizeof(info.box));
    return PRO_TK_NO_ERROR;
}

static ProError ax_q_space_part(ProMdl assembly,const wchar_t *label,AxDeviceFrame *frame,
    const double *low,const double *high,double mm,AxQuality *q){
    AxComponent owner={0};
    ProOutline box;
    ProError error=ax_hw_occurrence(assembly,label,mm,&owner);
    if(!error)error=ax_q_solid_bounds(&owner,frame,box);
    if(error)return error;
    double over[6];
    int pass=1;
    for(int axis=0;axis<3;axis++){
        over[axis]=fmax(0,low[axis]-box[0][axis]*mm);
        over[axis+3]=fmax(0,box[1][axis]*mm-high[axis]);
        pass&=ax_q_within(over[axis],0)&&ax_q_within(over[axis+3],0);
    }
    part_text_add(q->out,L"%ls 实测 X[%.8g,%.8g] Y[%.8g,%.8g] Z[%.8g,%.8g] mm，%ls。\n",
        label,box[0][0]*mm,box[1][0]*mm,box[0][1]*mm,box[1][1]*mm,box[0][2]*mm,box[1][2]*mm,ax_q_check(q,pass));
    if(!pass)part_text_add(q->out,L"    超界 -X %.8g / +X %.8g / -Y %.8g / +Y %.8g / -Z %.8g / +Z %.8g mm。\n",
        over[0],over[3],over[1],over[4],over[2],over[5]);
    return PRO_TK_NO_ERROR;
}

static ProError ax_q_space(ProMdl assembly,const PartCommand *cmd,double mm,AxQuality *q){
    AxDeviceFrame frame;
    double low[3],high[3];
    ProError error=ax_q_root_frame(assembly,cmd->arg[2],&frame);
    if(error)return error;
    for(int i=0;i<3;i++){
        low[i]=as_arg(cmd,3+i);high[i]=as_arg(cmd,6+i);
        if(low[i]>=high[i])return PRO_TK_BAD_INPUTS;
    }
    part_text_add(q->out,L"%ls 允许空间 X[%.8g,%.8g] Y[%.8g,%.8g] Z[%.8g,%.8g] mm。\n",
        cmd->arg[2],low[0],high[0],low[1],high[1],low[2],high[2]);
    if(cmd->count>9){
        for(int i=9;i<cmd->count;i++){
            error=ax_q_space_part(assembly,cmd->arg[i],&frame,low,high,mm,q);
            if(error)ax_q_missing(q,cmd->arg[i],error);
        }
        return PRO_TK_NO_ERROR;
    }
    AxScan *scan=NULL;
    if((error=ax_scan(assembly,1,&scan)))return error;
    for(int i=0;i<scan->count&&!q->out->truncated;i++){
        AxOccurrence *item=&scan->occurrences[i];
        if(!item->model||item->type!=PRO_MDL_PART)continue;
        ProBoolean skeleton=PRO_B_FALSE;
        wchar_t label[256];
        ax_path_label(item->ids,item->depth,label,_countof(label));
        error=ProMdlIsSkeleton(item->model,&skeleton);
        if(!error&&!skeleton)error=ax_q_space_part(assembly,label,&frame,low,high,mm,q);
        if(error)ax_q_missing(q,label,error);
    }
    part_text_add(q->out,L"抑制或排除 %d 个实例。\n",scan->suppressed);
    free(scan);
    return q->checked||q->unknown?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}
