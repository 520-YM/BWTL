static void part_snapshot_format_selected(const PartSnapshot *s,wchar_t *text,size_t capacity,unsigned sections){
    PartText out={text,capacity,0};int i;text[0]=0;
    part_text_add(&out,L"当前操作目标：%ls；类型=%ls；subtype=%d；工作目录：%ls\n单位：%ls；1模型长度单位=%.17g毫米；角度尺寸单位=度。所有尺寸表数值均为模型原生单位。\n",s->name,s->type==PRO_MDL_ASSEMBLY?L"装配":L"零件",s->subtype,s->directory,s->length_unit.name,s->mm_per_native);
    if(s->in_assembly){ProMdlName parent=L"";ProMdlMdlnameGet(s->window_model,parent);part_text_add(&out,L"所在装配窗口：%ls；此目标是实际激活的组件零件；编辑将影响该零件的全部实例。\n",parent);}
    part_text_add(&out,L"EMPTY_SOLID=%d（1=当前目标为空实体零件，可原位创建首个基础拉伸；0不适用）\n",s->empty_solid);
    if(s->has_outline)part_text_add(&out,L"外形范围(模型单位)：X[%.9g,%.9g] Y[%.9g,%.9g] Z[%.9g,%.9g]\n",s->outline[0][0],s->outline[1][0],s->outline[0][1],s->outline[1][1],s->outline[0][2],s->outline[1][2]);
    if(s->has_volume)part_text_add(&out,L"几何体积：%.12g（模型长度单位³）\n",s->volume);
    if(s->has_mass)part_text_add(&out,L"Creo返回质量：%.12g %ls（取决于模型材料/密度设置；未据此核验材料）\n",s->mass,s->mass_unit.name);
    else part_text_add(&out,L"质量属性：API未成功返回，不能推算或声称已知材料质量。\n");
    part_text_add(&out,L"特征(%d条；status=0活动,5普通抑制)：\n",s->features);
    if(sections&1){
    for(i=0;i<s->features;i++){const PartFeature *f=&s->feature[i];part_text_add(&out,L"FID=%d TYPE=%d STATUS=%d FLAGS=%u NAME=",f->id,f->type,f->status,f->flags);part_text_quote(&out,f->name);part_text_add(&out,L"\n");}
    }else part_text_add(&out,L"本次未展开特征列表，不得猜测ID。\n");
    part_text_add(&out,L"尺寸(%d条；TYPE=9线性,3半径,4直径,21角度,72弧长；RELATED=1不可修改)：\n",s->dimensions);
    if(sections&2){
    for(i=0;i<s->dimensions;i++){const PartDimension *d=&s->dimension[i];part_text_add(&out,L"FID=%d DIM=%ls VALUE=%.17g TYPE=%d RELATED=%d NEGATIVE=%d\n",d->feature,d->symbol,d->value,d->type,d->related,d->negative);}
    }else part_text_add(&out,L"本次未展开尺寸表，不得猜测符号或值。\n");
    part_text_add(&out,L"模型参数(%d条；LOCK=0可写；类型50浮点,51字符串,52整数,53布尔)：\n",s->parameters);
    if(sections&4){
    for(i=0;i<s->parameters;i++){const PartParameter *p=&s->parameter[i];part_text_add(&out,L"PARAM=%ls TYPE=%d LOCK=%d VALUE=",p->name,p->value.type,p->lock);part_param_format(&out,&p->value);part_text_add(&out,L"\n");}
    }else part_text_add(&out,L"本次未展开参数表，不得猜测参数值。\n");
    part_text_add(&out,L"几何摘要（边长度/面面积均为模型单位）：\n");
    if(sections&8){
    for(i=0;i<s->edges;i++)part_text_add(&out,L"EDGE=%d TYPE=%d LENGTH=%.9g\n",s->edge[i].id,s->edge[i].type,s->edge[i].size);
    for(i=0;i<s->surfaces;i++)part_text_add(&out,L"SURFACE=%d TYPE=%d AREA=%.9g\n",s->surface[i].id,s->surface[i].type,s->surface[i].size);
    }else part_text_add(&out,L"本次未展开边面列表；需要时明确请求读取几何。\n");
    if(s->truncated||out.truncated)part_text_add(&out,L"注意：模型数据超过当前读取上限，结果不完整，不得猜测未列出的特征或尺寸。\n");
    if(s->read_errors)part_text_add(&out,L"注意：%d项API读取未成功，其信息未知。\n",s->read_errors);
    if(s->unhealthy)part_text_add(&out,L"注意：存在失败特征，写入操作已禁止。\n");
    if(out.truncated){
        const wchar_t *marker=L"\n注意：输出空间已满，以上数据不完整；禁止猜测未显示的数据。\n";
        size_t length=wcslen(marker);if(capacity>length+1)wcscpy_s(text+capacity-length-1,length+1,marker);
    }
}
