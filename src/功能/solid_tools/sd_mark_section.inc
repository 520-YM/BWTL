static ProError sd_mark_section(FdSketch *s){
    ProElempath path=NULL;ProElement element=NULL;ProElempathItem items[2];ProError e;
    items[0].type=items[1].type=PRO_ELEM_PATH_ITEM_TYPE_ID;items[0].path_item.elem_id=PRO_E_STD_SECTION;items[1].path_item.elem_id=PRO_E_SKETCHER;
    if((e=ProElempathAlloc(&path)))return e;e=ProElempathDataSet(path,items,2);
    if(!e)e=ProElemtreeElementGet(s->tree,path,&element);if(!e)e=ProElementSpecialvalueSet(element,(ProAppData)s->section);
    ProElempathFree(&path);return e;
}
