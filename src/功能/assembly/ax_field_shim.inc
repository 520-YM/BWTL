typedef struct {
    ProSurface face;
    const AxDeviceFrame *frame;
    ProMatrix pose;
    int loops,edges;
} AxShimFace;

static ProError ax_shim_edge(ProEdge edge,ProError status,ProAppData context){
    if(status)return status;
    AxShimFace *check=context;
    ProEnttype type;
    ProVector a,b,delta,world;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(error)return error;
    if(type!=PRO_ENT_LINE||++check->edges>4)return PRO_TK_INVALID_TYPE;
    if((error=ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL))||
       (error=ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)))return error;
    ad_sub(b,a,delta);
    if((error=ProVectorTrfEval(delta,check->pose,world)))return error;
    if(!ad_unit(world))return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<2;i++)if(fabs(ad_dot(world,check->frame->axis[i]))>1-1e-8)return PRO_TK_NO_ERROR;
    return PRO_TK_BAD_CONTEXT;
}

static ProError ax_shim_contour(ProContour loop,ProError status,ProAppData context){
    if(status)return status;
    AxShimFace *check=context;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(loop,&traversal);
    if(error)return error;
    if(++check->loops!=1||traversal!=PRO_CONTOUR_TRAV_EXTERNAL)return PRO_TK_INVALID_TYPE;
    return ProContourEdgeVisit(check->face,loop,ax_shim_edge,NULL,context);
}

static ProError ax_shim_footprint(AxComponent *owner,const AxDevicePlane *face,
    const AxDeviceFrame *frame,double z,double width,double height){
    AxShimFace check={0};
    ProMatrix inverse;
    check.face=face->handle;
    check.frame=frame;
    ProError error=ax_device_pose(owner,check.pose);
    if(!error)error=ProSurfaceContourVisit(face->handle,ax_shim_contour,NULL,&check);
    if(error||check.loops!=1||check.edges!=4)return error?error:PRO_TK_INVALID_TYPE;
    ax_device_inverse(check.pose,inverse);
    for(int corner=0;corner<4;corner++){
        ProVector point;
        for(int i=0;i<3;i++)point[i]=frame->origin[i]+z*frame->axis[2][i]+
            (corner&1?0.5:-0.5)*width*frame->axis[0][i]+
            (corner&2?0.5:-0.5)*height*frame->axis[1][i];
        if((error=ax_plane_footprint(owner,face->handle,point,inverse)))return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_field_shim(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent root={.model=assembly},contact[2]={0};
    AxDeviceFrame frame;
    AxDevicePlane face[2];
    ProMdl model=NULL,existing=NULL;
    ProMdlName name;
    AxBatchItem placed={0};
    AxScan *scan=NULL;
    double z[2],width=as_arg(cmd,10)/mm,height=as_arg(cmd,11)/mm;
    ProError error;
    if(!part_close(mm,1))return PRO_TK_BAD_CONTEXT;
    wcscpy_s(name,_countof(name),cmd->arg[2]);
    if(!part_target_available(NULL,name,L"prt")||!ProMdlnameInit(name,PRO_MDL_PART,&existing))return PRO_TK_E_FOUND;
    if((error=ax_device_frame(&root,cmd->arg[7],0,&frame)))return error;
    for(int i=0;i<3;i++)frame.origin[i]+=(as_arg(cmd,8)*frame.axis[0][i]+as_arg(cmd,9)*frame.axis[1][i])/mm;
    for(int j=0;j<2;j++){
        if((error=ax_hw_occurrence(assembly,cmd->arg[3+j*2],mm,&contact[j]))||
           (error=ax_device_plane_world(&contact[j],cmd->arg[4+j*2],&face[j])))return error;
        z[j]=ax_project(face[j].point,&frame,2);
    }
    double gap=z[1]-z[0];
    if(ax_same_occurrence(&contact[0],&contact[1])||gap*mm<0.05||gap*mm>100||
       ad_dot(face[0].normal,frame.axis[2])<1-1e-7||ad_dot(face[1].normal,frame.axis[2])>-1+1e-7){
        part_text_add(out,L"两个接合面须沿坐标系 +Z 相向，且实际间隙在 0.05～100 mm。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    for(int j=0;j<2;j++){
        if((error=ax_shim_footprint(&contact[j],&face[j],&frame,z[j],width,height))){
            part_text_add(out,L"第 %d 个接合面须为坐标系对齐、无孔的矩形实体面，并完全容纳垫片。\n",j+1);
            return error;
        }
    }
    if((error=ax_scan(assembly,1,&scan)))goto done;
    if(scan->suppressed){error=PRO_TK_INCOMPLETE;goto done;}
    ProOutline shim={{-width/2,-height/2,z[0]},{width/2,height/2,z[1]}};
    for(int i=0;i<scan->count;i++){
        AxOccurrence *entry=&scan->occurrences[i];
        if(entry->type!=PRO_MDL_PART||
           ax_field_path_matches(&contact[0].path,entry)||ax_field_path_matches(&contact[1].path,entry))continue;
        ProBoolean skeleton;
        if((error=ProMdlIsSkeleton(entry->model,&skeleton)))goto done;
        if(skeleton)continue;
        AxComponent owner={.model=entry->model};
        ProIdTable ids={0};
        ProOutline bounds;
        memcpy(ids,entry->ids,entry->depth*sizeof(int));
        if((error=ProAsmcomppathInit((ProSolid)assembly,ids,entry->depth,&owner.path))||
           (error=ax_device_world_bounds(&owner,&frame,bounds)))goto done;
        int separate=0;
        for(int axis=0;axis<3;axis++)
            if(shim[0][axis]>bounds[1][axis]+0.001/mm||bounds[0][axis]>shim[1][axis]+0.001/mm)separate=1;
        if(!separate){
            wchar_t label[256];
            ax_path_label(entry->ids,entry->depth,label,_countof(label));
            part_text_add(out,L"垫片与 %ls 的实体包络相接或重叠，已停止。\n",label);
            error=PRO_TK_BAD_CONTEXT;
            goto done;
        }
    }
    const char *stage="build field shim";
    ModelPlan plan={0,width*mm,height*mm,gap*mm,0,1};
    if((error=quick_seed(name,&model))||(error=model_build_internal(&plan,name,&stage,model,NULL,NULL)))goto done;
    AxDeviceFrame local={.axis={{1,0,0},{0,1,0},{0,0,1}}},destination=frame;
    ProOutline actual;
    if((error=ax_device_bounds(model,&local,actual)))goto done;
    for(int i=0;i<3;i++){
        if(fabs(actual[1][i]-actual[0][i]-(shim[1][i]-shim[0][i]))>0.001/mm){
            error=PRO_TK_GENERAL_ERROR;
            goto done;
        }
        for(int j=0;j<3;j++)destination.origin[j]+=shim[0][i]*frame.axis[i][j];
        local.origin[i]=actual[0][i];
    }
    placed.model=model;
    ax_device_align(&local,&destination,placed.pose);
    error=ax_device_create(assembly,&placed,1,mm,&contact[0],&face[0],&contact[1],&face[1],out);
    if(!error)part_text_add(out,L"已按实测间隙 %.8g mm 创建独立矩形垫片 %ls，长宽 %.8g × %.8g mm，未保存。\n",
        gap*mm,name,width*mm,height*mm);
done:
    if(error&&model&&!ax_recovery_uncertain&&ProMdlErase(model))ax_uncertain(out);
    free(scan);
    return error;
}
