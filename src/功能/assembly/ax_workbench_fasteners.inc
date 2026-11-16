static ProError aw_fastener_side(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent panel={0},fastener={0};
    AxHardwareSite hole,shaft;
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&panel);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[5],mm,&fastener);
    if(!error)error=ax_hw_site(&panel,cmd->arg[3],cmd->arg[4],mm,&hole);
    if(!error)error=ax_hw_site(&fastener,cmd->arg[6],cmd->arg[7],mm,&shaft);
    if(!error)error=ax_hw_world(&panel,&hole);
    if(!error)error=ax_hw_world(&fastener,&shaft);
    if(error)return error;
    if(!hole.bore||shaft.bore)return PRO_TK_INVALID_TYPE;
    const double angle=ax_q_angle(hole.cylinder.direction,shaft.cylinder.direction);
    const double offset=ax_hw_separation(hole.contact,shaft.contact,hole.cylinder.direction,mm);
    ProVector delta;
    ad_sub(shaft.contact,hole.contact,delta);
    const double seat=ad_dot(delta,hole.plane.direction)*mm;
    double reach[2],opening[2];
    ax_hw_axial_span(&shaft,hole.contact,hole.plane.direction,mm,&reach[0],&reach[1]);
    ax_hw_axial_span(&hole,hole.contact,hole.plane.direction,mm,&opening[0],&opening[1]);
    if(opening[0]>0.001||opening[1]>0.001){part_text_add(out,L"所选板面外法向没有指向孔外，请核对安装面。\n");return PRO_TK_BAD_CONTEXT;}
    const int aligned=angle<0.01&&ax_q_within(offset,as_arg(cmd,8))&&ax_q_within(fabs(seat),as_arg(cmd,9));
    const int reversed=aligned&&reach[0]>-.001&&reach[1]>.001;
    const int passing=aligned&&!reversed&&reach[0]<-.001&&(shaft.cylinder.radius-hole.cylinder.radius)*mm<.001;
    part_text_add(out,L"%ls 杆轴偏心 %.8g mm，夹角 %.8g°，承压面偏离 %+.8g mm，杆沿安装面外法向范围 [%.8g, %.8g] mm。\n",
        cmd->arg[5],offset,angle,seat,reach[0],reach[1]);
    part_text_add(out,L"%ls。检查针对给定紧固件承压面和指定安装侧。\n",
        reversed?L"杆朝孔外，判断为反装":passing?L"杆朝入孔且座面符合":L"承压、方向或尺寸条件不满足");
    return PRO_TK_NO_ERROR;
}

static ProError aw_mixed_fasteners(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxBomData data={0};
    ProError error=ax_bom_collect(assembly,&data,1);
    if(error){ax_bom_clear(&data);return error;}
    const size_t capacity=(size_t)data.scan->count+1;
    ProMatrix *pose=calloc(capacity,sizeof(*pose));
    unsigned char *eligible=calloc(capacity,1);
    if(!pose||!eligible){error=PRO_TK_OUT_OF_MEMORY;goto done;}
    const int parent_index=!wcscmp(cmd->arg[2],L"ROOT")?-1:aw_bom_instance(&data,cmd->arg[2]);
    if(parent_index<0&&wcscmp(cmd->arg[2],L"ROOT")){error=PRO_TK_E_NOT_FOUND;goto done;}
    const AxOccurrence *parent=parent_index<0?NULL:&data.scan->occurrences[parent_index];
    if(parent&&parent->type!=PRO_MDL_ASSEMBLY){error=PRO_TK_INVALID_TYPE;goto done;}
    int checked=0,unknown=0,problems=0;
    const double maximum=as_arg(cmd,3);
    for(int i=0;i<data.scan->count;i++){
        int row=data.rows[i+1];
        if(!aw_bom_in_scope(&data.scan->occurrences[i],parent)||row<0)continue;
        const AxBomModel *item=&data.models[row];
        if(_wcsicmp(item->value[AB_KIND],L"FASTENER")||item->type!=PRO_MDL_PART)continue;
        if(!item->value[AB_SPEC][0]||!qa_number(item->value[AB_LENGTH],0.01,1e5)||
           ax_layout_measure(assembly,&data.scan->occurrences[i],pose[i])){unknown++;continue;}
        eligible[i]=1;
        checked++;
        for(int j=0;j<i;j++){
            if(!eligible[j])continue;
            const AxBomModel *other=&data.models[data.rows[j+1]];
            if(_wcsicmp(item->value[AB_SPEC],other->value[AB_SPEC])||
               fabs(wcstod(item->value[AB_LENGTH],NULL)-wcstod(other->value[AB_LENGTH],NULL))<1e-6)continue;
            double gap=ax_layout_shift(pose[i],mm,pose[j],mm);
            if(gap>maximum)continue;
            wchar_t a[256],b[256];
            if(problems++<60&&ax_bom_path(&data.scan->occurrences[i],a)&&ax_bom_path(&data.scan->occurrences[j],b))
                part_text_add(out,L"%ls 与 %ls 相距 %.8g mm，同规格 %ls、标称长度不同 %ls / %ls mm。\n",b,a,gap,
                    item->value[AB_SPEC],other->value[AB_LENGTH],item->value[AB_LENGTH]);
        }
    }
    part_text_add(out,L"读取有 FASTENER 标记且资料完整的 %d 个实例，发现 %d 对邻近混长，%d 个已标记件缺规格、长度或位姿。\n",
        checked,problems,unknown);
    error=checked?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
done:
    free(pose);free(eligible);ax_bom_clear(&data);
    return error;
}
