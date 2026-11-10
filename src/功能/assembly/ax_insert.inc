static ProError ax_load_part(const wchar_t *name,ProMdl *part,int *loaded_here){
    *loaded_here=0;
    ProError error=ProMdlnameInit((wchar_t *)name,PRO_MDL_PART,part);
    if(!error)return PRO_TK_NO_ERROR;
    if(error!=PRO_TK_E_NOT_FOUND)return error;
    ProPath directory,leaf,path;
    if((error=ProDirectoryCurrentGet(directory))||(error=dv_native_revision(directory,name,leaf)))return error;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls",directory,leaf)<0)return PRO_TK_BAD_INPUTS;
    error=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,part);
    if(!error)*loaded_here=1;
    return error;
}

static ProError ax_insert(ProMdl assembly,const PartCommand *cmd,double scale,PartText *out){
    ProMdl part=NULL;
    ProAsmcomp feature={0};
    ProMassProperty geometry;
    ProMatrix wanted={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}},actual;
    int loaded_here=0;
    double part_scale;
    ProError error;
    if((error=ax_load_part(cmd->arg[2],&part,&loaded_here))||
       (error=part_health(part))||(error=qa_geometry_mass(part,&geometry))||
       (error=qa_mm_per_unit(part,&part_scale)))goto done;
    if(!part_close(scale,part_scale)){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<3;i++)wanted[3][i]=wcstod(cmd->arg[3+i],NULL)/scale;
    error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)part,wanted,&feature);
    if(!error)error=part_regenerate(assembly);
    if(!error)error=ax_actual_pose(assembly,&feature,actual);
    if(!error)error=ax_packaged(&feature);
    if(!error&&!ax_same_pose(wanted,actual,scale))error=PRO_TK_GENERAL_ERROR;
    if(!error)part_text_add(out,L"已放入 %ls，组件 ID:%d，未约束初始位置 (%.9g, %.9g, %.9g) mm。实际位置已核对。\n",
        cmd->arg[2],feature.id,actual[3][0]*scale,actual[3][1]*scale,actual[3][2]*scale);
done:
    if(error&&feature.owner==assembly&&feature.id>0){
        if(ax_leaf_without_children(&feature)||model_remove_feature((ProSolid)assembly,feature.id)||part_regenerate(assembly))
            ax_uncertain(out);
    }
    if(error&&loaded_here&&!ax_recovery_uncertain)ProMdlErase(part);
    return error;
}
