static ProError ax_mass(ProMdl assembly,double scale,PartText *out){
    AxScan *scan=NULL;
    ProMassProperty geometry,material;
    ProUnitsystem units;
    ProUnititem mass_unit,length_unit;
    ProError error=ax_scan(assembly,1,&scan);
    if(error)return error;
    int parts=scan->parts;
    free(scan);
    if(!parts)return PRO_TK_E_NOT_FOUND;
    if((error=qa_geometry_mass(assembly,&geometry)))return error;
    part_text_add(out,L"装配活动零件 %d 个。组件实体体积合计 %.12g mm³。\n",
        parts,geometry.volume*scale*scale*scale);
    if((error=ProSolidMassPropertyWithDensityGet((ProSolid)assembly,NULL,PRO_MP_DENS_DEFAULT,0,&material))||
       (error=ProMdlPrincipalunitsystemGet(assembly,&units))||
       (error=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_MASS,&mass_unit))||
       (error=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&length_unit))||
       !isfinite(material.mass)||material.mass<=0){
        trace("assembly native material properties",error);
        part_text_add(out,L"当前 Creo 模型质量不可用（Creo %d）。\n",error);
        return PRO_TK_NO_ERROR;
    }
    part_text_add(out,L"Creo 装配质量 %.12g %ls。重心 (%.10g, %.10g, %.10g) mm。\n"
        L"主惯量 (%.10g, %.10g, %.10g) %ls·%ls²，按模型当前密度设置计算。\n",
        material.mass,mass_unit.name,material.center_of_gravity[0]*scale,material.center_of_gravity[1]*scale,
        material.center_of_gravity[2]*scale,material.principal_moments[0],material.principal_moments[1],
        material.principal_moments[2],mass_unit.name,length_unit.name);
    return PRO_TK_NO_ERROR;
}
