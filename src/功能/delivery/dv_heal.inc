static ProError dv_import_feature(ProPath path,ProMdlName name,int make_solid,ProMdl *result){
    ProIntfType format=dv_exchange_file(wcsrchr(path,L'\\')+1)==1?PRO_INTF_STEP:PRO_INTF_IGES;
    ProIntfDataSource source={0};
    ProImportfeatAttr options={0};
    ProFeature feature;
    ProSolid part=NULL;
    *result=NULL;
    ProError error=ProSolidMdlnameCreate(name,PRO_MDLFILE_PART,&part);
    if(error)return error;
    options.join_surfaces=1;
    options.attempt_make_solid=make_solid!=0;
    options.body_use_opt=PRO_IMPORT_BODY_USE_DEFAULT;
    error=ProIntfDataSourceInit(format,path,&source);
    if(!error){
        error=ProImportfeatCreate(part,&source,NULL,&options,&feature);
        ProIntfDataSourceClean(&source);
    }
    if(error){
        ProMdlErase((ProMdl)part);
        return error;
    }
    *result=(ProMdl)part;
    return PRO_TK_NO_ERROR;
}

static ProError dv_heal(ProMdl source,const PartCommand *cmd,PartText *out){
    ProPath path;
    ProMdl result=NULL;
    ProMdlName name;
    ProMassProperty mass;
    double scale;
    int imports;
    ProError error=dv_import_count(source,&imports);
    if(error)return error;
    if(!imports)return PRO_TK_BAD_CONTEXT;
    if(!qa_geometry_mass(source,&mass)&&!part_health(source)){
        part_text_add(out,L"当前导入件已有有效实体，重新缝合实体化无需执行。\n");
        return PRO_TK_NO_CHANGE;
    }
    if(!part_target_available(source,cmd->arg[2],L"prt"))return PRO_TK_E_FOUND;
    wcscpy_s(name,_countof(name),cmd->arg[2]);
    if((error=dv_work_file(cmd->arg[3],path))||
       (error=dv_source_part(path,dv_import_type(cmd->arg[3])))||
       (error=dv_import_feature(path,name,1,&result)))return error;
    if((error=part_health(result))||(error=qa_single_body(result,NULL))||
       (error=qa_geometry_mass(result,&mass))||(error=qa_mm_per_unit(result,&scale))||
       (error=dv_display_new(result))){
        if(ProMdlErase(result))part_text_add(out,L"本次修复副本仍在会话中，可检查其状态。\n");
        part_text_add(out,L"原生缝合后未能确认单一有效实体。\n");
        return error;
    }
    part_text_add(out,L"已用 Creo 原生缝合与实体化生成修复副本 %ls。实体体积 %.10g mm³，已通过几何和重生成检查。\n",
        name,mass.volume*scale*scale*scale);
    return PRO_TK_NO_ERROR;
}
