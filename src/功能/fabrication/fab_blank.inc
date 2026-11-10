static ProError fab_blank(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d)return PRO_TK_OUT_OF_MEMORY;
    FILE *file=NULL;
    ProPath path;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_flat_read(model,work.stage,L"blank_probe",cmd->arg[3],d,path);
    if(!error&&!DeleteFileW(path))error=PRO_TK_CANT_WRITE;
    if(!error)error=fab_csv_open(work.stage,L"blank-sizes.csv",&file);
    double width=d->high[0]-d->low[0],height=d->high[1]-d->low[1];
    if(!error){
        fwprintf(file,L"模型,厚度_mm,下料包络X_mm,下料包络Y_mm,外轮廓图元,折弯线\n");
        if(!dv_csv_text(file,cmd->arg[1]))error=PRO_TK_CANT_WRITE;
        fwprintf(file,L",%.10g,%.10g,%.10g,%d,%d\n",thickness,width,height,d->outline,d->bends);
    }
    free(d);
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"板料包络 %.8g × %.8g mm，厚 %.8g mm。\n%ls\\blank-sizes.csv",width,height,thickness,work.target);
    return error;
}
