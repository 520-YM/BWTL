static ProError fd_section_from_tree(ProElement tree,ProSection *section){
    ProElempath path=NULL;ProElement el=NULL;ProElempathItem items[2];ProError e;
    items[0].type=items[1].type=PRO_ELEM_PATH_ITEM_TYPE_ID;
    items[0].path_item.elem_id=PRO_E_STD_SECTION;items[1].path_item.elem_id=PRO_E_SKETCHER;
    if((e=ProElempathAlloc(&path)))return e;
    e=ProElempathDataSet(path,items,2);if(!e)e=ProElemtreeElementGet(tree,path,&el);
    if(!e)e=ProElementSpecialvalueGet(el,NULL,(ProAppData *)section);ProElempathFree(&path);return e;
}
