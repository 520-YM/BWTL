static ProError fab_batch_dxf(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput work={0};
    FabDxf *d=calloc(1,sizeof(*d));
    if(!d)return PRO_TK_OUT_OF_MEMORY;
    int total=(cmd->count-2)/2;
    ProError error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    for(int i=0;!error&&i<total;i++){
        const wchar_t *name=cmd->arg[i?2+2*i:1];
        const wchar_t *face=cmd->arg[3+2*i];
        ProMdl part=model;
        ProPath path;
        if(!dv_safe_leaf(name)){error=PRO_TK_BAD_INPUTS;break;}
        if(i)error=ProMdlnameInit((wchar_t *)name,PRO_MDL_PART,&part);
        if(!error)error=part_health(part);
        if(!error)error=fab_flat_read(part,work.stage,name,face,d,path);
        if(error)part_text_add(out,L"未通过 DXF 检查的零件 %ls（%ls）。\n",name,face);
    }
    free(d);
    error=dv_stage_finish(&work,error,out);
    if(!error)part_text_add(out,L"已生成并逐份回读 %d 个平板 DXF。\n目录 %ls",total,work.target);
    return error;
}
