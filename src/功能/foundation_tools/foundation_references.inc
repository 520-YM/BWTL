static ProError foundation_references(ProMdl model,wchar_t *report,size_t capacity){
    PartText out={report,capacity,0};FdRefList list={model,&out,0};ProError e;report[0]=0;
    part_text_add(&out,L"本次实际模型参照（最多96项）：\n");e=ProSolidFeatVisit((ProSolid)model,fd_reference_feature,NULL,&list);
    if(e==PRO_TK_E_NOT_FOUND)e=0;
    if(fd_last_sketch_model==model&&fd_last_sketch[0])part_text_add(&out,L"最近创建关联草绘：%ls；可用SKETCH_INFO读取图元编号。\n",fd_last_sketch);
    return e;
}
