static ProError fab_preflight(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d)return PRO_TK_OUT_OF_MEMORY;
    ProPath path;
    ProError error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_flat_read(model,work.stage,L"preflight",cmd->arg[2],d,path);
    error=fab_discard(&work,error,out);
    if(!error)part_text_add(out,L"下料 DXF 检查通过。毫米单位，外轮廓 %.8g × %.8g mm，轮廓图元 %d，内孔图元 %d，折弯线 %d。切割轮廓端点闭合，未发现重复图元。",
        d->high[0]-d->low[0],d->high[1]-d->low[1],d->outline,d->holes,d->bends);
    free(d);
    return error;
}
