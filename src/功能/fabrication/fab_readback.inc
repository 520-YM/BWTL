static int fab_dxf_equivalent(const FabDxf *candidate,const FabDxf *native,double tol){
    if(candidate->count!=native->count)return 0;
    unsigned char matched[FAB_DXF_MAX]={0};
    for(int i=0;i<candidate->count;i++){
        int match=-1;
        for(int j=0;j<native->count;j++)if(!matched[j]&&fab_same_entity(&candidate->entities[i],&native->entities[j],tol)){
            match=j;
            break;
        }
        if(match<0)return 0;
        matched[match]=1;
    }
    return 1;
}

static ProError fab_readback_dxf(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    ProPath folder,path;
    FabDxf *candidate=calloc(1,sizeof(*candidate)),*native=calloc(1,sizeof(*native));
    if(!candidate||!native){free(candidate);free(native);return PRO_TK_OUT_OF_MEMORY;}
    double tol=as_arg(cmd,5);
    ProError error=ProDirectoryCurrentGet(folder);
    if(!error)error=fab_leaf_path(folder,cmd->arg[3],L"",path);
    if(!error){
        error=fab_dxf_read(path,candidate);
        if(error)part_text_add(out,L"指定 DXF 未通过单位、图元或闭合检查。\n");
    }
    if(!error)error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_flat_read(model,work.stage,L"readback",cmd->arg[4],native,path);
    if(!error&&!fab_dxf_equivalent(candidate,native,tol)){
        part_text_add(out,L"DXF 与当前展开几何存在差异。文件 %d 个图元，当前模型 %d 个图元，容差 %.9g mm。\n",
            candidate->count,native->count,tol);
        error=PRO_TK_BAD_CONTEXT;
    }
    error=fab_discard(&work,error,out);
    if(!error)part_text_add(out,L"DXF 回读通过。%d 个图元与当前展开按图层、位置及形状匹配，容差 %.9g mm。",native->count,tol);
    free(candidate);free(native);
    return error;
}

static ProError fab_readback_step(ProMdl model,const PartCommand *cmd,PartText *out){
    static LONG serial;
    ProPath path;
    ProMdlName name;
    ProMdl temporary=NULL;
    ProSolidBody bodies[2];
    ProOutline bounds[2];
    ProMassProperty mass[2];
    double mm[2];
    const double tol=as_arg(cmd,4);
    swprintf_s(name,_countof(name),L"fb_read_%08lx_%08lx",GetCurrentProcessId(),InterlockedIncrement(&serial));
    ProError error=part_target_available(model,name,L"prt")?PRO_TK_NO_ERROR:PRO_TK_E_FOUND;
    if(!error)error=dv_work_file(cmd->arg[3],path);
    if(!error)error=dv_source_part(path,PRO_INTF_IMPORT_STEP);
    DeliveryOutput work={0};
    if(!error)error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=ProIntfimportModelWithOptionsMdlnameCreate(path,NULL,PRO_INTF_IMPORT_STEP,PRO_MDL_PART,
        PRO_IMPORTREP_MASTER,name,NULL,NULL,&temporary);
    ProMdl compare[]={model,temporary};
    for(int i=0;!error&&i<2;i++){
        error=part_health(compare[i]);
        if(!error)error=qa_single_body(compare[i],&bodies[i]);
        if(!error)error=qa_mm_per_unit(compare[i],&mm[i]);
        if(!error)error=qa_geometry_mass(compare[i],&mass[i]);
        if(!error)error=ProSolidBodyOutlineGet(&bodies[i],bounds[i]);
    }
    double volume_diff=0,area_diff=0,max_bounds=0;
    if(!error){
        ProVector lower,upper;
        double extent=0;
        for(int i=0;i<3;i++){
            double lo=bounds[0][0][i]*mm[0],hi=bounds[0][1][i]*mm[0];
            double clo=bounds[1][0][i]*mm[1],chi=bounds[1][1][i]*mm[1];
            lower[i]=fmin(lo,clo);upper[i]=fmax(hi,chi);
            max_bounds=fmax(max_bounds,fmax(fabs(lo-clo),fabs(hi-chi)));
            extent=fmax(extent,upper[i]-lower[i]);
        }
        double area_a=mass[0].surface_area*mm[0]*mm[0],area_b=mass[1].surface_area*mm[1]*mm[1];
        double vol_a=mass[0].volume*mm[0]*mm[0]*mm[0],vol_b=mass[1].volume*mm[1]*mm[1]*mm[1];
        volume_diff=fabs(vol_a-vol_b);area_diff=fabs(area_a-area_b);
        QualityComparison comparison;
        error=qa_compare_rays(model,temporary,mm[0],mm[1],lower,upper,NULL,0,7,tol,&comparison);
        if(!error){
            int difference=max_bounds>tol||volume_diff>fmax(1e-6,area_a*tol)||
                area_diff>fmax(1e-6,12*extent*tol);
            qa_comparison_report(out,&comparison,tol,difference);
            part_text_add(out,L"包络坐标最大偏差 %.9g mm，体积差 %.9g mm³，表面积差 %.9g mm²。\n",
                max_bounds,volume_diff,area_diff);
            if(difference||comparison.different||comparison.ambiguous||!comparison.material_rays)error=PRO_TK_BAD_CONTEXT;
        }
    }
    if(temporary){
        ProError erase=ProMdlErase(temporary);
        if(erase){
            dv_recovery_uncertain=1;
            part_text_add(out,L"临时回读模型 %ls 未能清理。\n",name);
            if(!error)error=erase;
        }
    }
    error=fab_discard(&work,error,out);
    if(!error)part_text_add(out,L"STEP 回读通过，已清理临时导入模型。");
    return error;
}
