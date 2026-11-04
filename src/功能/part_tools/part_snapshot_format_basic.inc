static void part_snapshot_format_basic(const PartSnapshot *s,wchar_t *text,size_t capacity){
    PartText out={text,capacity,0};double x,y,z;text[0]=0;
    part_text_add(&out,L"名称：%ls.%ls\n类型：%ls\n",s->name,s->type==PRO_MDL_ASSEMBLY?L"asm":L"prt",
        s->type==PRO_MDL_ASSEMBLY?L"装配":s->subtype==PROMDLSTYPE_PART_SHEETMETAL?L"钣金零件":L"实体零件");
    x=(s->outline[1][0]-s->outline[0][0])*s->mm_per_native;
    y=(s->outline[1][1]-s->outline[0][1])*s->mm_per_native;
    z=(s->outline[1][2]-s->outline[0][2])*s->mm_per_native;
    if(s->empty_solid)part_text_add(&out,L"尺寸：空零件，暂无实体\n");
    else if(s->has_outline&&isfinite(x)&&isfinite(y)&&isfinite(z)&&x>=0&&y>=0&&z>=0)
        part_text_add(&out,L"外形尺寸：%.6g × %.6g × %.6g mm\n",x,y,z);
    else part_text_add(&out,L"外形尺寸：暂未获取\n");
    part_text_add(&out,L"材料：%.80ls\n",part_basic_material(s));
    if(s->has_mass&&isfinite(s->mass)&&s->mass>=0)part_text_add(&out,L"重量：%.6g %ls\n",s->mass,s->mass_unit.name[0]?s->mass_unit.name:L"（模型质量单位）");
    else part_text_add(&out,L"重量：暂未获取\n");
    if(s->in_assembly)part_text_add(&out,L"位置：装配中当前激活的零件\n");
    if(s->unhealthy)part_text_add(&out,L"状态：存在重生成问题\n");
}
