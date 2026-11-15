typedef struct {PartText *out;int driving,reference,related,dependent,shown,unread;} ImitationDimensions;

static ProError imitation_dimension_kind(ProDimension *dim,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationDimensions *scan=data;
    ProBoolean driving=PRO_B_FALSE,related=PRO_B_FALSE;
    ProError error=PRO_TK_NO_ERROR;
    const int reference=dim->type==PRO_REF_DIMENSION;
    if(!reference)error=ProDimensionIsReldriven(dim,&related);
    if(!error&&!reference)error=ProDimensionIsDriving(dim,&driving);
    if(error){scan->unread++;return PRO_TK_NO_ERROR;}
    if(reference)scan->reference++;
    else if(related)scan->related++;
    else if(driving)scan->driving++;
    else scan->dependent++;
    if(scan->shown>=16||(!reference&&!related&&driving))return PRO_TK_NO_ERROR;
    ProName symbol=L"";
    ProDimension parent;
    ProDimensionSymbolGet(dim,symbol);
    part_text_add(scan->out,L"  %ls（ID %d）%ls",symbol[0]?symbol:L"尺寸",dim->id,
        reference?L"为参考尺寸":related?L"由关系式驱动":L"为从属尺寸");
    if(!reference&&!ProDimensionParentGet(dim,&parent)&&parent.id!=dim->id){
        ProName driver=L"";
        ProDimensionSymbolGet(&parent,driver);
        part_text_add(scan->out,L"，原驱动 %ls（ID %d）",driver,parent.id);
    }
    part_text_add(scan->out,L"。\n");
    scan->shown++;
    return PRO_TK_NO_ERROR;
}

static void imitation_dimension_report(ProMdl model,PartText *out)
{
    ImitationDimensions scan={.out=out};
    part_text_add(out,L"尺寸控制方式\n");
    for(int mode=0;mode<2;mode++){
        ProError error=ProSolidDimensionVisit((ProSolid)model,mode?PRO_B_TRUE:PRO_B_FALSE,
            imitation_dimension_kind,NULL,&scan);
        if(error&&error!=PRO_TK_E_NOT_FOUND)
            part_text_add(out,L"%ls尺寸读取返回 Creo %d。\n",mode?L"参考":L"标准",error);
    }
    part_text_add(out,L"直接驱动 %d 个，参考尺寸 %d 个，关系驱动 %d 个，从属尺寸 %d 个，未读 %d 个。\n",
        scan.driving,scan.reference,scan.related,scan.dependent,scan.unread);
}
