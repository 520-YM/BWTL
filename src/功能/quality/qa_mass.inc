static ProError qa_mass(ProMdl model,double scale,PartText *out){
    ProMassProperty geometry,material;
    ProError error=qa_geometry_mass(model,&geometry);
    if(error)return error;
    part_text_add(out,L"实体体积 %.12g mm³，表面积 %.12g mm²。\n",
        geometry.volume*scale*scale*scale,geometry.surface_area*scale*scale);
    part_text_add(out,L"几何形心 (%.10g, %.10g, %.10g) mm，基于均匀密度。\n",
        geometry.center_of_gravity[0]*scale,geometry.center_of_gravity[1]*scale,geometry.center_of_gravity[2]*scale);
    ProUnitsystem units;
    ProUnititem mass_unit,length_unit;
    if((error=ProSolidMassPropertyGet((ProSolid)model,NULL,&material))||
       (error=ProMdlPrincipalunitsystemGet(model,&units))||
       (error=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_MASS,&mass_unit))||
       (error=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&length_unit))||
       !isfinite(material.mass)||material.mass<=0){
        part_text_add(out,L"模型质量与惯量不可用（Creo %d）。\n",error);
        return PRO_TK_NO_ERROR;
    }
    part_text_add(out,L"Creo 模型质量 %.12g %ls，平均密度 %.12g %ls/%ls³。\n"
        L"质量重心 (%.10g, %.10g, %.10g) mm。\n"
        L"主惯量 (%.10g, %.10g, %.10g) %ls·%ls²。质量数值采用当前 Creo 模型的密度设置。\n",
        material.mass,mass_unit.name,material.density,mass_unit.name,length_unit.name,
        material.center_of_gravity[0]*scale,material.center_of_gravity[1]*scale,material.center_of_gravity[2]*scale,
        material.principal_moments[0],material.principal_moments[1],material.principal_moments[2],
        mass_unit.name,length_unit.name);
    return PRO_TK_NO_ERROR;
}
