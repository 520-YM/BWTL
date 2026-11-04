static ProError fd_reference_item(ProGeomitem *item,ProError status,ProAppData data){
    FdRefList *list=data;ProName name=L"";ProFeature feature;ProSrftype surface_type;ProSurface surface;
    if(status)return status;if(list->count>=96)return 0;ProModelitemNameGet((ProModelitem *)item,name);
    /* Named reference tokens resolve FEATURE names, not a different name
       assigned to one of that feature's geometry items. */
    if(!ProGeomitemFeatureGet(item,&feature))ProModelitemNameGet((ProModelitem *)&feature,name);
    if(item->type==PRO_SURFACE){
        if(ProGeomitemToSurface(item,&surface)||ProSurfaceTypeGet(surface,&surface_type))return 0;
        part_text_add(list->out,L"SURFACE:%d 名称=%ls，%ls\n",item->id,name,surface_type==PRO_SRF_PLANE?L"平面":surface_type==PRO_SRF_CYL?L"圆柱面":L"其他曲面");
    }else if(item->type==PRO_EDGE)part_text_add(list->out,L"EDGE:%d 所属特征=%ls\n",item->id,name);
    else part_text_add(list->out,L"%ls%ls（ID=%d）\n",item->type==PRO_AXIS?L"AXIS:":item->type==PRO_POINT?L"POINT:":item->type==PRO_CSYS?L"CSYS:":L"CURVE:",name,item->id);
    list->count++;return 0;
}
