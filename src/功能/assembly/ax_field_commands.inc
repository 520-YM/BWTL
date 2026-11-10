typedef struct {
    ProName feature,sketch;
    const wchar_t *plane,*orient;
    AxFieldTargets targets;
    AxFieldProfile profile;
    double depth;
    int direction,generate;
} AxFieldStep;

static int ax_field_direction(const wchar_t *token){
    return !wcscmp(token,L"POS")?1:!wcscmp(token,L"NEG")?-1:0;
}

static ProError ax_field_line_profile(ProMdl assembly,const PartCommand *cmd,int kind,double mm,AxFieldProfile *profile){
    ProVector origin,axis,delta;
    ProError error;
    if(kind==AX_FIELD_TRANSFER){
        AxComponent source={0};
        FdRef ref={0};
        QualityBore bore;
        ProMatrix pose;
        error=ax_hw_occurrence(assembly,cmd->arg[6],mm,&source);
        if(!error)error=qa_bore(source.model,cmd->arg[7],&ref,&bore);
        fd_ref_free(&ref);
        if(error)return error;
        if((error=ax_device_pose(&source,pose))||
           (error=ProPntTrfEval(bore.origin,pose,origin))||
           (error=ProVectorTrfEval(bore.axis,pose,axis)))return error;
        profile->size=as_arg(cmd,8);
        if(!profile->size)profile->size=2*bore.radius*mm;
    }else{
        AxCopyGeometry route={0};
        if((error=ax_copy_geometry(assembly,cmd->arg[6],&route)))return error;
        if(route.type!=PRO_CURVE||route.count!=2)return PRO_TK_INVALID_TYPE;
        memcpy(origin,route.points[0],sizeof(ProVector));
        ad_sub(route.points[1],origin,axis);
        if(!ad_unit(axis))return PRO_TK_BAD_CONTEXT;
        if((error=ax_field_axis_center(assembly,cmd->arg[3],origin,axis,profile->center)))return error;
        ad_sub(profile->center,origin,delta);
        double offset=ad_dot(delta,axis);
        ad_sub(route.points[1],origin,delta);
        if(offset<-0.001/mm||offset>ad_dot(delta,axis)+0.001/mm)return PRO_TK_BAD_CONTEXT;
        profile->size=as_arg(cmd,7);
        return PRO_TK_NO_ERROR;
    }
    return ax_field_axis_center(assembly,cmd->arg[3],origin,axis,profile->center);
}

static ProError ax_field_steps(ProMdl assembly,const PartCommand *cmd,int kind,double mm,AxFieldStep *steps,int *total){
    AxFieldStep *first=&steps[0];
    const wchar_t *name=cmd->arg[2];
    ProError error;
    int target_arg=0;
    wcscpy_s(first->feature,PRO_NAME_SIZE,name);
    first->profile.nx=first->profile.ny=1;
    *total=1;
    if(kind==AX_FIELD_CUT){
        wcscpy_s(first->sketch,PRO_NAME_SIZE,cmd->arg[3]);
        first->direction=ax_field_direction(cmd->arg[4]);
        first->depth=as_arg(cmd,5);
        target_arg=6;
    }else if(kind==AX_FIELD_LAYERS){
        ProVector origin,axis,point;
        if((error=ax_field_plane(assembly,cmd->arg[9],point,axis)))return error;
        for(int i=0;i<3;i++)origin[i]=as_arg(cmd,5+i)/mm;
        *total=0;
        AxFieldTargets unique={0};
        for(int i=8;i<cmd->count;i+=6){
            AxFieldStep *step=&steps[*total];
            if((error=ax_field_target(assembly,cmd->arg[i],mm,&unique)))return error;
            step->targets.paths[0]=unique.paths[unique.count-1];
            step->targets.count=1;
            step->plane=cmd->arg[i+1];
            step->orient=cmd->arg[3];
            step->direction=ax_field_direction(cmd->arg[4]);
            step->profile.nx=step->profile.ny=1;
            step->profile.size=as_arg(cmd,i+3);
            int counterbore=!wcscmp(cmd->arg[i+2],L"CBORE");
            step->depth=counterbore?0:as_arg(cmd,i+5);
            if((error=ax_field_axis_center(assembly,step->plane,origin,axis,step->profile.center)))return error;
            (*total)++;
            if(counterbore){
                steps[*total]=*step;
                steps[*total].depth=as_arg(cmd,i+5);
                steps[*total].profile.size=as_arg(cmd,i+4);
                (*total)++;
            }
        }
        for(int i=0;i<*total;i++)swprintf_s(steps[i].feature,PRO_NAME_SIZE,L"%ls_%d",name,i+1);
    }else{
        first->plane=cmd->arg[3];
        first->orient=cmd->arg[4];
        first->direction=ax_field_direction(cmd->arg[5]);
        if(kind==AX_FIELD_HOLE||kind==AX_FIELD_GRID){
            for(int i=0;i<3;i++)first->profile.center[i]=as_arg(cmd,6+i)/mm;
            first->profile.size=as_arg(cmd,9);
            target_arg=10;
            if(kind==AX_FIELD_GRID){
                first->profile.nx=(int)as_arg(cmd,10);
                first->profile.ny=(int)as_arg(cmd,11);
                first->profile.pitch_x=as_arg(cmd,12);
                first->profile.pitch_y=as_arg(cmd,13);
                if(first->profile.nx*first->profile.ny>16||
                   (first->profile.nx>1&&first->profile.pitch_x<=first->profile.size)||
                   (first->profile.ny>1&&first->profile.pitch_y<=first->profile.size))return PRO_TK_BAD_INPUTS;
                target_arg=14;
            }
        }else if(kind==AX_FIELD_WINDOW){
            first->profile.clearance=as_arg(cmd,7);
            error=ax_hw_occurrence(assembly,cmd->arg[6],mm,&first->profile.window);
            if(error)return error;
            target_arg=8;
        }else{
            if((error=ax_field_line_profile(assembly,cmd,kind,mm,&first->profile)))return error;
            target_arg=kind==AX_FIELD_TRANSFER?9:8;
        }
    }
    if(kind!=AX_FIELD_LAYERS){
        for(int i=target_arg;i<cmd->count;i++)
            if((error=ax_field_target(assembly,cmd->arg[i],mm,&first->targets)))return error;
        if(kind==AX_FIELD_TRANSFER||kind==AX_FIELD_WINDOW){
            ProIdTable ids;
            int depth=ax_path_parse(cmd->arg[6],ids);
            for(int i=0;i<first->targets.count;i++){
                ProAsmcomppath *target=&first->targets.paths[i];
                if(target->table_num==depth&&!memcmp(target->comp_id_table,ids,depth*sizeof(int)))
                    return PRO_TK_BAD_INPUTS;
            }
        }
    }

    for(int i=0;i<*total;i++)if(steps[i].plane){
        swprintf_s(steps[i].sketch,PRO_NAME_SIZE,L"%ls_P%d",name,i+1);
        steps[i].generate=1;
    }
    if(kind!=AX_FIELD_LAYERS&&first->targets.count>1){
        AxFieldTargets combined=first->targets;
        *total=combined.count;
        for(int i=1;i<*total;i++){
            steps[i]=*first;
            steps[i].generate=0;
        }
        for(int i=0;i<*total;i++){
            steps[i].targets.paths[0]=combined.paths[i];
            steps[i].targets.count=1;
            swprintf_s(steps[i].feature,PRO_NAME_SIZE,L"%ls_%d",name,i+1);
        }
    }
    return PRO_TK_NO_ERROR;
}

static ProError ax_field_rollback(ProMdl assembly,FdTxn *before){
    FdTxn after={0};
    int *ids=NULL;
    ProFeatureDeleteOptions *options=NULL;
    ProError error=ProSolidFeatVisit((ProSolid)assembly,fd_feature_ids,NULL,&after);
    if(error)return error;
    int count=0,new_ids[512];
    for(int i=after.before_count-1;i>=0;i--)if(!fd_was_feature(before,after.before_ids[i]))
        new_ids[count++]=after.before_ids[i];
    if(!count)return after.before_count==before->before_count?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    if((error=ProArrayAlloc(count,sizeof(*ids),1,(ProArray *)&ids))||
       (error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options)))goto done;
    memcpy(ids,new_ids,count*sizeof(*ids));
    options[0]=PRO_FEAT_DELETE_NO_OPTS;
    if((error=ProFeatureWithoptionsDelete((ProSolid)assembly,ids,options,
        PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL))||(error=part_regenerate(assembly)))goto done;
    after.before_count=0;
    if((error=ProSolidFeatVisit((ProSolid)assembly,fd_feature_ids,NULL,&after)))goto done;
    if(after.before_count!=before->before_count){error=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<after.before_count;i++)
        if(!fd_was_feature(before,after.before_ids[i])){error=PRO_TK_GENERAL_ERROR;break;}
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    if(options)ProArrayFree((ProArray *)&options);
    return error;
}

static ProError ax_field_volume(ProMdl assembly,ProAsmcomppath *path,double *volume){
    ProMdl model=NULL;
    ProMassProperty mass;
    ProError error;
    if(path->table_num==1){
        ProAsmcomp component;
        error=ax_component_feature(assembly,path->comp_id_table[0],&component);
        if(!error)error=ProAsmcompMdlGet(&component,&model);
    }else error=ProAsmcomppathMdlGet(path,&model);
    if(!error)error=qa_geometry_mass(model,&mass);
    if(error)return error;
    *volume=mass.volume;
    return isfinite(*volume)&&*volume>0?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_field_execute(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxFieldStep steps[AX_FIELD_TARGET_MAX]={0};
    ProFeature features[24]={0};
    int feature_steps[24]={0};
    FdTxn before={0};
    ProPath template_path;
    int total=0,started=0,completed=0,upper_bound=0;
    ProError error;
#define AF(call) do{error=(call);if(error){trace("field command " #call,error);goto done;}}while(0)
    if(!part_close(mm,1)){
        part_text_add(out,L"当前装配加工路径需要毫米制总装和毫米制目标零件。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    if((error=ax_field_template(template_path))){
        part_text_add(out,L"此 Creo 安装中未找到所需的原生封闭曲面加工模板。\n");
        return error;
    }
    AF(part_health(assembly));
    AF(ax_field_steps(assembly,cmd,kind,mm,steps,&total));
    for(int i=0;i<total;i++){
        upper_bound+=steps[i].profile.nx*steps[i].profile.ny;
        AF(ux_new_name(assembly,steps[i].feature));
        if(steps[i].generate){AF(ux_new_name(assembly,steps[i].sketch));}
    }
    if(upper_bound>_countof(features))return PRO_TK_BAD_INPUTS;
    AF(ProSolidFeatVisit((ProSolid)assembly,fd_feature_ids,NULL,&before));
    if(before.before_count>410)return PRO_TK_BAD_CONTEXT;
    started=1;
    for(int i=0;i<total;i++){
        AxFieldStep *step=&steps[i];
        if(step->generate){
            AF(ax_field_sketch(assembly,step->sketch,step->plane,step->orient,&step->profile,mm));
        }
    }
    for(int i=0;i<total;i++){
        AxFieldStep *step=&steps[i];
        AxFieldQuilts quilts={0};
        ProName quilt_name;
        swprintf_s(quilt_name,PRO_NAME_SIZE,L"%ls_Q%d",cmd->arg[2],i+1);
        AF(ax_field_quilt(assembly,quilt_name,step->sketch,step->direction,step->depth/mm,&step->targets,&quilts));
        if(completed+quilts.count>_countof(features)){error=PRO_TK_BAD_INPUTS;goto done;}
        for(int j=0;j<quilts.count;j++){
            double volume_before,volume_after;
            ProName feature_name;
            if(quilts.count==1)wcscpy_s(feature_name,PRO_NAME_SIZE,step->feature);
            else swprintf_s(feature_name,PRO_NAME_SIZE,L"%ls_%d",step->feature,j+1);
            AF(ux_new_name(assembly,feature_name));
            AF(ax_field_volume(assembly,&step->targets.paths[0],&volume_before));
            AF(ax_field_place_cut(assembly,&step->targets,&quilts.item[j],feature_name,&features[completed]));
            AF(ax_field_volume(assembly,&step->targets.paths[0],&volume_after));
            if(volume_before-volume_after<=1e-5){
                part_text_add(out,L"%ls 在指定实例上没有可验证的材料去除。\n",feature_name);
                error=PRO_TK_BAD_CONTEXT;
                goto done;
            }
            feature_steps[completed++]=i;
        }
    }
    for(int i=0;i<completed;i++)
        AF(ax_field_check_hits(&features[i],&steps[feature_steps[i]].targets));
    for(int i=0;i<completed;i++){
        ProName name;
        if(ProModelitemNameGet((ProModelitem *)&features[i],name))continue;
        AxFieldStep *step=&steps[feature_steps[i]];
        wchar_t label[256];
        ax_path_label(step->targets.paths[0].comp_id_table,step->targets.paths[0].table_num,label,_countof(label));
        part_text_add(out,L"%ls：已核对 1 个指定实例 %ls，仅在当前顶层装配可见。\n",name,label);
    }
    part_text_add(out,L"Creo 原生装配加工已完成，核对了 %d 个加工特征的实际去料和实例范围，未保存。\n",completed);
done:
    if(error&&started){
        if(ax_field_rollback(assembly,&before))ax_uncertain(out);
        else part_text_add(out,L"本次新增加工特征和辅助几何已清理。\n");
    }
    return error;
#undef AF
}
