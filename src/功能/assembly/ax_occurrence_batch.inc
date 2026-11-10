#ifdef BWTL_HARDWARE_FAULT_TEST
static int ax_batch_fail_after;
#endif

typedef struct {
    ProMdl model;
    ProMatrix pose;
    ProAsmcomp feature;
} AxBatchItem;

static ProError ax_batch_create(ProMdl assembly,AxBatchItem *items,int count,double mm,int fix,PartText *out){
    ProAsmcomppath root;
    ProAsmcompconstraint *fixed=NULL;
    AxScan *scan=NULL;
    ProMatrix actual;
    ProError error=ax_root_path(assembly,&root);
    if(!error)error=ax_scan(assembly,1,&scan);
    if(error)goto done;
    for(int i=0;i<count;i++){
        for(int j=0;j<i;j++)if(items[i].model==items[j].model&&
            ad_near(items[i].pose[3],items[j].pose[3],0.001/mm)){error=PRO_TK_E_FOUND;goto done;}
        for(int j=0;j<scan->count;j++){
            AxOccurrence *entry=&scan->occurrences[j];
            if(entry->depth!=1||entry->model!=items[i].model)continue;
            ProAsmcomp occurrence;
            if((error=ax_component_feature(assembly,entry->ids[0],&occurrence))||
               (error=ax_actual_pose(assembly,&occurrence,actual)))goto done;
            if(ad_near(actual[3],items[i].pose[3],0.001/mm)){error=PRO_TK_E_FOUND;goto done;}
        }
    }
    if(fix){
        error=ProArrayAlloc(0,sizeof(*fixed),1,(ProArray *)&fixed);
        if(!error)error=ax_add_relation(&fixed,NULL,NULL,NULL,NULL,PRO_ASM_FIX,0,NULL,NULL);
        if(error)goto done;
    }
    for(int i=0;i<count;i++){
        error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)items[i].model,items[i].pose,&items[i].feature);
        if(!error&&fix)error=ProAsmcompConstraintsSet(&root,&items[i].feature,fixed);
        if(error)goto done;
#ifdef BWTL_HARDWARE_FAULT_TEST
        if(ax_batch_fail_after==i+1){error=PRO_TK_GENERAL_ERROR;goto done;}
#endif
    }
    if((error=part_regenerate(assembly)))goto done;
    for(int i=0;i<count;i++){
        ProMdl actual_model=NULL;
        if((error=ProAsmcompMdlGet(&items[i].feature,&actual_model))||
           (error=ax_actual_pose(assembly,&items[i].feature,actual)))goto done;
        if(actual_model!=items[i].model||!ax_same_pose(actual,items[i].pose,mm)){error=PRO_TK_GENERAL_ERROR;goto done;}
        error=fix?ax_compare_constraints(&items[i].feature,&root,fixed,1,mm):ax_packaged(&items[i].feature);
        if(error)goto done;
    }
    part_text_add(out,L"已创建并核对 %d 个独立实例%ls，顶层装配未保存。\n",count,fix?L"（固定定位）":L"（按计算位置放入）");
    for(int i=0;i<count;i++)part_text_add(out,L"ID:%d%ls",items[i].feature.id,i+1==count?L"\n":L"、");
done:
    if(error){
        if(error==PRO_TK_E_FOUND)part_text_add(out,L"发现同型号的重合放置位置，已拒绝重复安装。\n");
        for(int i=count-1;i>=0;i--)ax_discard_occurrence(assembly,&items[i].feature,out);
    }
    if(fixed)ProAsmcompconstraintArrayFree(fixed);
    free(scan);
    return error;
}

static void ax_reflect(const double *v,const double *normal,double *out){
    double projection=ad_dot(v,normal);
    for(int i=0;i<3;i++)out[i]=v[i]-2*projection*normal[i];
}

static ProError ax_batch_model(const wchar_t *name,double mm,ProMdl *model,int *loaded){
    double own;
    ProError error=ax_load_part(name,model,loaded);
    if(!error)error=part_health(*model);
    if(!error)error=qa_mm_per_unit(*model,&own);
    return error?error:part_close(own,mm)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_place_array(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent source={0},root={0};
    AxBatchItem items[AX_BATCH_LIMIT]={0};
    ProMatrix origin;
    ProSelection plane=NULL;
    ProMdl model=NULL;
    int count=0,loaded=0;
    ProError error=ax_component_resolve(assembly,cmd->arg[2],&source);
    if(!error)error=ax_ordinary_direct(&source.feature);
    if(!error)error=ax_actual_pose(assembly,&source.feature,origin);
    if(error)return error;
    model=source.model;
    if(kind==AX_MIRROR_COMPONENT){
        AxGeometry mirror;
        if((error=ax_ref_occurrence(assembly,L"ROOT",&root))||
           (error=ax_ref_resolve(&root,cmd->arg[3],1,&plane,&mirror)))goto done;
        if(mirror.type!=AX_GEOM_PLANE){error=PRO_TK_INVALID_TYPE;goto done;}
        if(wcscmp(cmd->arg[4],L"SAME")&&(error=ax_batch_model(cmd->arg[4],mm,&model,&loaded)))goto done;
        AxBatchItem *item=&items[count++];
        item->model=model;
        memcpy(item->pose,origin,sizeof(origin));
        int local=ax_axis_index(cmd->arg[5]);
        for(int i=0;i<3;i++){
            ax_reflect(origin[i],mirror.direction,item->pose[i]);
            if(i==local)for(int j=0;j<3;j++)item->pose[i][j]=-item->pose[i][j];
        }
        ProVector relative,reflected;
        ad_sub(origin[3],mirror.point,relative);
        ax_reflect(relative,mirror.direction,reflected);
        for(int i=0;i<3;i++)item->pose[3][i]=mirror.point[i]+reflected[i];
        if(ad_near(item->pose[3],origin[3],0.001/mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    }else if(kind==AX_ARRAY_CIRCULAR){
        int axis=ax_axis_index(cmd->arg[3]),members=(int)as_arg(cmd,7);
        double step=as_arg(cmd,8)*AS_PI/180;
        ProVector center={as_arg(cmd,4)/mm,as_arg(cmd,5)/mm,as_arg(cmd,6)/mm},direction={0},relative,rotated;
        direction[axis]=1;
        ad_sub(origin[3],center,relative);
        ad_cross(relative,direction,rotated);
        if(sqrt(ad_dot(rotated,rotated))*mm<0.001){error=PRO_TK_BAD_CONTEXT;goto done;}
        for(int i=1;i<members;i++){
            AxBatchItem *item=&items[count++];
            double c=cos(step*i),s=sin(step*i);
            item->model=model;
            memcpy(item->pose,origin,sizeof(origin));
            for(int j=0;j<3;j++)ax_rotate_vector(origin[j],direction,c,s,item->pose[j]);
            ax_rotate_vector(relative,direction,c,s,rotated);
            for(int j=0;j<3;j++)item->pose[3][j]=center[j]+rotated[j];
        }
    }else{
        int row_axis=cmd->arg[3][0]-L'X',col_axis=0,rows=(int)as_arg(cmd,4),cols=1;
        double pitch=as_arg(cmd,5)/mm,col_pitch=0;
        if(kind==AX_ARRAY_GRID){
            col_axis=cmd->arg[3][1]-L'X';
            cols=(int)as_arg(cmd,5);
            pitch=as_arg(cmd,6)/mm;col_pitch=as_arg(cmd,7)/mm;
        }
        for(int i=0;i<rows;i++)for(int j=0;j<cols;j++)if(i||j){
            AxBatchItem *item=&items[count++];
            item->model=model;
            memcpy(item->pose,origin,sizeof(origin));
            item->pose[3][row_axis]+=i*pitch;
            item->pose[3][col_axis]+=j*col_pitch;
        }
    }
    error=ax_batch_create(assembly,items,count,mm,0,out);
done:
    if(plane)ProSelectionFree(&plane);
    if(error&&loaded&&!ax_recovery_uncertain)ProMdlErase(model);
    return error;
}
