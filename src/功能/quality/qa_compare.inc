static ProError qa_compare(ProMdl model,const PartCommand *cmd,double scale,PartText *out){
    ProMdl second=NULL;
    ProSolidBody a,b;
    ProOutline bounds_a,bounds_b;
    ProMassProperty mass_a,mass_b;
    double scale_b,bound_delta=0,center_delta=0;
    ProVector lower,upper,shift;
    QualityComparison comparison;
    ProError error;
    if((error=ProMdlnameInit((wchar_t *)cmd->arg[2],PRO_MDL_PART,&second)))return error;
    if(second==model)return PRO_TK_BAD_INPUTS;
    if((error=part_health(second))||
       (error=qa_mm_per_unit(second,&scale_b))||
       (error=qa_single_body(model,&a))||(error=qa_single_body(second,&b))||
       (error=ProSolidBodyOutlineGet(&a,bounds_a))||(error=ProSolidBodyOutlineGet(&b,bounds_b))||
       (error=qa_geometry_mass(model,&mass_a))||(error=qa_geometry_mass(second,&mass_b)))return error;
    for(int i=0;i<3;i++){
        double low_a=bounds_a[0][i]*scale,low_b=bounds_b[0][i]*scale_b;
        double high_a=bounds_a[1][i]*scale,high_b=bounds_b[1][i]*scale_b;
        lower[i]=fmin(low_a,low_b);
        upper[i]=fmax(high_a,high_b);
        bound_delta=fmax(bound_delta,fmax(fabs(low_a-low_b),fabs(high_a-high_b)));
        shift[i]=mass_b.center_of_gravity[i]*scale_b-mass_a.center_of_gravity[i]*scale;
    }
    center_delta=sqrt(ad_dot(shift,shift));
    double tolerance=wcstod(cmd->arg[3],NULL);
    int grid;
    part_integer(cmd->arg[4],&grid);
    error=qa_compare_rays(model,second,scale,scale_b,lower,upper,NULL,0,grid,tolerance,&comparison);
    if(error)return error;
    double volume_a=mass_a.volume*scale*scale*scale,volume_b=mass_b.volume*scale_b*scale_b*scale_b;
    part_text_add(out,L"%ls → %ls，同一零件坐标框架，XYZ 三向各 %d×%d 射线。\n"
        L"体积 %.12g → %.12g mm³，净变化 %+.9g mm³。表面积 %.12g → %.12g mm²。\n"
        L"包围盒边界最大偏移 %.9g mm，几何形心偏移 %.9g mm。\n",
        cmd->arg[1],cmd->arg[2],grid,grid,volume_a,volume_b,volume_b-volume_a,
        mass_a.surface_area*scale*scale,mass_b.surface_area*scale_b*scale_b,bound_delta,center_delta);
    qa_comparison_report(out,&comparison,tolerance,!qa_at_least(tolerance,fmax(bound_delta,center_delta)));
    return PRO_TK_NO_ERROR;
}
