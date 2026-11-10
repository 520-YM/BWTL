static ProError fab_paint(ProMdl model,const PartCommand *cmd,PartText *out){
    double mm,total=0;
    double area[16];
    int ids[16];
    const int count=cmd->count-4;
    ProError error=qa_single_body(model,NULL);
    if(!error)error=qa_mm_per_unit(model,&mm);
    if(!error&&!count){
        ProMassProperty mass;
        error=qa_geometry_mass(model,&mass);
        if(!error)total=mass.surface_area*mm*mm;
    }
    for(int i=0;!error&&i<count;i++){
        FdRef ref={0};
        ProSurface face;
        error=qa_surface(model,cmd->arg[i+4],1,&ref,&face);
        if(!error)error=ProSurfaceIdGet(face,&ids[i]);
        if(!error)error=ProSurfaceAreaEval(face,&area[i]);
        fd_ref_free(&ref);
        if(error)break;
        for(int j=0;j<i;j++)if(ids[i]==ids[j])error=PRO_TK_BAD_INPUTS;
        area[i]*=mm*mm;
        if(!isfinite(area[i])||area[i]<=0)error=PRO_TK_BAD_CONTEXT;
        total+=area[i];
    }
    if(!error&&(!isfinite(total)||total<=0))error=PRO_TK_BAD_CONTEXT;
    DeliveryOutput work={0};
    FILE *file=NULL;
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"paint-areas.csv",&file);
    if(!error){
        fwprintf(file,L"范围,表面ID,面积_mm²,面积_m²\n");
        for(int i=0;i<count;i++)fwprintf(file,L"指定面,%d,%.12g,%.12g\n",ids[i],area[i],area[i]/1e6);
        fwprintf(file,L"%ls,,%.12g,%.12g\n",count?L"指定面合计":L"全部实体边界含内壁",total,total/1e6);
    }
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"%ls面积 %.9g mm²（%.9g m²）。\n%ls\\paint-areas.csv",
        count?L"指定面合计":L"全部实体边界（含内壁）",total,total/1e6,work.target);
    return error;
}
