static ProError ax_joint_geometry(ProSelection selection,int world,AxGeometry *shape){
    ProGeomitem item;
    ProError error=ProSelectionModelitemGet(selection,&item);
    if(!error)error=ax_geometry(&item,shape);
    if(!error&&world)error=ax_model_to_root(selection,shape);
    return error;
}

static ProError ax_joint_frame(ProSelection refs[3],int world,AxDeviceFrame *frame){
    AxGeometry axis,radial,axial;
    ProError error=ax_joint_geometry(refs[0],world,&axis);
    if(!error)error=ax_joint_geometry(refs[1],world,&radial);
    if(!error)error=ax_joint_geometry(refs[2],world,&axial);
    if(error)return error;
    if(axis.type!=AX_GEOM_AXIS||radial.type!=AX_GEOM_PLANE||axial.type!=AX_GEOM_PLANE||
       fabs(ad_dot(axis.direction,radial.direction))>1e-7||
       fabs(ad_dot(axis.direction,axial.direction))<1-1e-7)return PRO_TK_BAD_CONTEXT;
    memcpy(frame->axis[0],radial.direction,sizeof(ProVector));
    memcpy(frame->axis[2],axial.direction,sizeof(ProVector));
    ad_cross(frame->axis[2],frame->axis[0],frame->axis[1]);
    ProVector relative;
    ad_sub(axial.point,axis.point,relative);
    double along=ad_dot(relative,frame->axis[2]);
    for(int i=0;i<3;i++)frame->origin[i]=axis.point[i]+along*frame->axis[2][i];
    return PRO_TK_NO_ERROR;
}

static ProError ax_joint_position(AxJoint *joint,double mm){
    AxDeviceFrame base,part;
    ProError error=ax_joint_frame(joint->base_ref,1,&base);
    if(!error)error=ax_joint_frame(joint->part_ref,0,&part);
    if(error)return error;
    ProMatrix zero;
    AxGeometry line={0};
    memcpy(line.point,base.origin,sizeof(ProVector));
    memcpy(line.direction,base.axis[2],sizeof(ProVector));
    ax_device_align(&part,&base,zero);
    ax_motion_pose(zero,&line,joint->axes[AX_MOTION_ANGLE].value,joint->axes[AX_MOTION_TRAVEL].value/mm,joint->pose);
    return PRO_TK_NO_ERROR;
}

static ProError ax_joint_prepare(ProMdl assembly,const PartCommand *cmd,double mm,AxJoint *joint,int *loaded){
    AxComponent part={0};
    AxGeometry geometry;
    ProError error=ax_ref_occurrence(assembly,cmd->arg[2],&joint->base);
    double base_mm;
    if(!error)error=qa_mm_per_unit(joint->base.model,&base_mm);
    if(!error&&!part_close(base_mm,mm))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ax_batch_model(cmd->arg[3],mm,&joint->part,loaded);
    if(error)return error;
    part.model=joint->part;
    for(int i=0;i<3;i++){
        error=ax_ref_resolve(&joint->base,cmd->arg[4+2*i],1,&joint->base_ref[i],&geometry);
        if(!error)error=ax_ref_resolve(&part,cmd->arg[5+2*i],0,&joint->part_ref[i],&geometry);
        if(error)return error;
    }
    return ax_joint_position(joint,mm);
}

static ProError ax_joint_identity(ProMdl model,ProParamvalue *value){
    ProMdlName name;
    ProError error=ProMdlMdlnameGet(model,name);
    if(error)return error;
    if(wcslen(name)>=PRO_LINE_SIZE)return PRO_TK_BAD_CONTEXT;
    value->type=PRO_PARAM_STRING;
    wcscpy_s(value->value.s_val,PRO_LINE_SIZE,name);
    return PRO_TK_NO_ERROR;
}

static ProError ax_joint_store(ProAsmcomp *feature,const AxJoint *joint){
    ProParamvalue values[5]={0};
    unsigned ids[3][2];
    for(int i=0;i<3;i++){
        ProModelitem a,b;
        ProError error=ProSelectionModelitemGet(joint->base_ref[i],&a);
        if(!error)error=ProSelectionModelitemGet(joint->part_ref[i],&b);
        if(error)return error;
        ids[i][0]=a.id;ids[i][1]=b.id;
    }
    int base=joint->base.path.table_num?joint->base.path.comp_id_table[0]:0;
    values[0].type=PRO_PARAM_STRING;
    if(swprintf_s(values[0].value.s_val,PRO_LINE_SIZE,L"%d %x %x %x %x %x %x %x",
        joint->kind,base,ids[0][0],ids[0][1],ids[1][0],ids[1][1],ids[2][0],ids[2][1])<0)return PRO_TK_BAD_INPUTS;
    ProError error=ax_joint_identity(joint->base.model,&values[1]);
    if(!error)error=ax_joint_identity(joint->part,&values[2]);
    if(!error&&(!ax_joint_axis_value(&joint->axes[0],&values[3])||!ax_joint_axis_value(&joint->axes[1],&values[4])))
        error=PRO_TK_BAD_INPUTS;
    for(int i=0;!error&&i<_countof(values);i++)error=ax_joint_set_parameter(feature,i,&values[i],1);
    return error;
}

static ProError ax_joint_load(ProMdl assembly,AxComponent *target,double mm,AxJoint *joint){
    ProParameter parameter;
    ProParamvalue metadata[3]={0};
    ProError error;
    for(int i=0;i<_countof(metadata);i++)
        if((error=ax_joint_parameter(&target->feature,i,&parameter,&metadata[i])))return error;
    unsigned base,ids[3][2];
    wchar_t trailing[2],base_label[32]=L"ROOT";
    int count=swscanf_s(metadata[0].value.s_val,L"%d %x %x %x %x %x %x %x %1ls",
        &joint->kind,&base,&ids[0][0],&ids[0][1],&ids[1][0],&ids[1][1],&ids[2][0],&ids[2][1],
        trailing,(unsigned)_countof(trailing));
    if(count!=8||joint->kind<AX_JOINT_PIN||joint->kind>AX_JOINT_CYLINDER||
       base>INT_MAX||base==(unsigned)target->feature.id)return PRO_TK_BAD_CONTEXT;
    if(base)swprintf_s(base_label,_countof(base_label),L"ID:%u",base);
    if((error=ax_ref_occurrence(assembly,base_label,&joint->base)))return error;
    joint->part=target->model;
    ProParamvalue name={0};
    if((error=ax_joint_identity(joint->base.model,&name)))return error;
    if(_wcsicmp(name.value.s_val,metadata[1].value.s_val))return PRO_TK_BAD_CONTEXT;
    if((error=ax_joint_identity(joint->part,&name)))return error;
    if(_wcsicmp(name.value.s_val,metadata[2].value.s_val))return PRO_TK_BAD_CONTEXT;
    double units;
    if((error=qa_mm_per_unit(joint->base.model,&units)))return error;
    if(!part_close(units,mm))return PRO_TK_BAD_CONTEXT;
    if((error=qa_mm_per_unit(joint->part,&units)))return error;
    if(!part_close(units,mm))return PRO_TK_BAD_CONTEXT;
    for(int i=0;i<3;i++){
        if(!ids[i][0]||ids[i][0]>INT_MAX||!ids[i][1]||ids[i][1]>INT_MAX)return PRO_TK_BAD_CONTEXT;
        ProType type=i==0?PRO_AXIS:PRO_SURFACE;
        error=ax_joint_reference_get(joint->base.model,&joint->base.path,ids[i][0],type,&joint->base_ref[i]);
        if(!error)error=ax_joint_reference_get(joint->part,NULL,ids[i][1],type,&joint->part_ref[i]);
        if(error)return error;
    }
    for(int i=0;i<2;i++){
        if((error=ax_joint_axis_get(&target->feature,i,&joint->axes[i])))return error;
        if(joint->axes[i].bounded&&!ax_joint_has_axis(joint->kind,i))return PRO_TK_BAD_CONTEXT;
    }
    return ax_joint_position(joint,mm);
}

static ProError ax_joint_configure(AxJoint *joint,const PartCommand *cmd,int kind){
    if(kind==AX_MOTION_PRESET){
        int available=0;
        for(int i=0;i<2;i++){
            AxJointAxis *axis=&joint->axes[i];
            if(!axis->bounded)continue;
            axis->value=!wcscmp(cmd->arg[3],L"CLOSED")?axis->low:
                !wcscmp(cmd->arg[3],L"OPEN")?axis->high:(axis->low+axis->high)/2;
            available=1;
        }
        return available?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
    }
    int index=kind==AX_ROTATION_LIMIT?AX_MOTION_ANGLE:AX_MOTION_TRAVEL;
    if(!ax_joint_has_axis(joint->kind,index))return PRO_TK_BAD_CONTEXT;
    AxJointAxis *axis=&joint->axes[index];
    axis->low=as_arg(cmd,3);axis->high=as_arg(cmd,4);
    axis->bounded=1;
    axis->value=fmin(axis->high,fmax(axis->low,axis->value));
    return PRO_TK_NO_ERROR;
}

static ProError ax_joint_verify_pose(ProMdl assembly,ProAsmcomp *feature,ProMatrix pose,double mm){
    ProMatrix actual;
    ProError error=ax_actual_pose(assembly,feature,actual);
    return error?error:ax_same_pose(pose,actual,mm)?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError ax_joint_update(ProMdl assembly,AxComponent *target,AxJoint *joint,const PartCommand *cmd,int kind,
    double mm,PartText *out){
    ProMatrix initial,previous;
    ProParamvalue old[2]={0},next[2]={0};
    int applied[2]={0},moved=0;
    ProError error=ax_joint_verify_pose(assembly,&target->feature,joint->pose,mm);
    if(error){
        part_text_add(out,L"实际位置与连接记录不一致。请先核对基座和组件是否被移位。\n");
        return error;
    }
    memcpy(previous,joint->pose,sizeof(previous));
    if((error=ax_joint_configure(joint,cmd,kind))){
        part_text_add(out,L"该连接没有此运动轴，或需要先设置对应轴的限位。\n");
        return error;
    }
    if((error=ProAsmcompPositionGet(&target->feature,initial))||
       (error=ax_joint_position(joint,mm)))return error;
    for(int i=0;i<2;i++){
        ProParameter parameter;
        if((error=ax_joint_parameter(&target->feature,i+3,&parameter,&old[i])))return error;
        if(!ax_joint_axis_value(&joint->axes[i],&next[i]))return PRO_TK_BAD_INPUTS;
    }
    for(int i=0;!error&&i<2;i++){
        if(!wcscmp(old[i].value.s_val,next[i].value.s_val))continue;
        applied[i]=1;
        error=ax_joint_set_parameter(&target->feature,i+3,&next[i],0);
    }
    if(!error&&!ax_same_pose(previous,joint->pose,mm)){
        moved=1;
        error=ax_set_position(assembly,&target->feature,joint->pose);
    }
    if(!error)error=ax_joint_verify_pose(assembly,&target->feature,joint->pose,mm);
    if(error){
        int failed=0;
        for(int i=0;i<2;i++)if(applied[i]&&ax_joint_set_parameter(&target->feature,i+3,&old[i],0))failed=1;
        if(moved&&(ax_set_position(assembly,&target->feature,initial)||
           ax_joint_verify_pose(assembly,&target->feature,previous,mm)))failed=1;
        if(failed)ax_uncertain(out);
    }
    return error;
}

static ProError ax_joint_command(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxJoint joint={0};
    AxComponent target={0};
    int loaded=0;
    ProError error;
#define JC(call) do{if((error=(call))){trace("mechanism command " #call,error);goto done;}}while(0)
    if(kind<=AX_JOINT_CYLINDER){
        joint.kind=kind;
        JC(ax_joint_prepare(assembly,cmd,mm,&joint,&loaded));
        if(joint.base.path.table_num>1){error=PRO_TK_BAD_CONTEXT;goto done;}
        JC(ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)joint.part,joint.pose,&target.feature));
        JC(part_regenerate(assembly));
        JC(ax_packaged(&target.feature));
        JC(ax_joint_verify_pose(assembly,&target.feature,joint.pose,mm));
        JC(ax_joint_store(&target.feature,&joint));
        target.model=joint.part;
        AxJoint verify={0};
        error=ax_joint_load(assembly,&target,mm,&verify);
        if(!error&&(!ax_same_pose(joint.pose,verify.pose,mm)||joint.kind!=verify.kind))error=PRO_TK_GENERAL_ERROR;
        ax_joint_free(&verify);
        if(error)goto done;
    }else{
        JC(ax_component_resolve(assembly,cmd->arg[2],&target));
        JC(ax_ordinary_direct(&target.feature));
        JC(ax_leaf_without_children(&target.feature));
        JC(ax_packaged(&target.feature));
        JC(ax_joint_load(assembly,&target,mm,&joint));
        JC(ax_joint_update(assembly,&target,&joint,cmd,kind,mm,out));
    }
    const wchar_t *names[]={L"销轴连接",L"滑块连接",L"圆柱连接",L"开门角度限位",L"滑动行程限位",L"预设机构位置"};
    part_text_add(out,L"已完成%ls，BWTL 机构实例 ID:%d。已读回连接记录并核对 Creo 实际姿态。\n",
        names[kind-AX_JOINT_PIN],target.feature.id);
    for(int i=0;i<2;i++){
        const AxJointAxis *axis=&joint.axes[i];
        if(!ax_joint_has_axis(joint.kind,i))continue;
        const wchar_t *unit=i==AX_MOTION_ANGLE?L"度":L"mm";
        part_text_add(out,L"%ls当前位置 %.8g %ls",i==AX_MOTION_ANGLE?L"旋转":L"滑动",axis->value,unit);
        if(axis->bounded)part_text_add(out,L"，限位 %.8g 至 %.8g %ls",axis->low,axis->high,unit);
        part_text_add(out,L"。\n");
    }
    part_text_add(out,L"位置由 BWTL 控制，装配未保存。\n");
done:
    if(error&&kind<=AX_JOINT_CYLINDER){
        ax_discard_occurrence(assembly,&target.feature,out);
        if(loaded&&!ax_recovery_uncertain)ProMdlErase(joint.part);
    }
    ax_joint_free(&joint);
    return error;
#undef JC
}