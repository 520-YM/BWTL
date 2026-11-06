/* Geometry for target selection, in model XYZ millimetres. No viewport or UI. */
typedef struct {PartText *out;double scale;int count,truncated,ids[192],types[192];} McRefList;
static ProError mc_reference_item(ProGeomitem *item,ProError status,ProAppData data){
    McRefList *list=data;ProError e;ProVector p,n,a,b,c,v;ProFeature owner;ProName name=L"";
    if(status)return status;
    for(e=0;e<list->count;e++)if(list->ids[e]==item->id&&list->types[e]==item->type)return 0;
    if(list->count>=192){list->truncated=1;return 0;}
    if(!ProGeomitemFeatureGet(item,&owner))ProModelitemNameGet((ProModelitem *)&owner,name);
    if(item->type==PRO_SURFACE){
        e=fd_plane_data(item,p,n);if(e)return 0;
        part_text_add(list->out,L"SURFACE:%d 特征=%ls 平面点XYZ=(%.9g,%.9g,%.9g) 几何法向=(%.9g,%.9g,%.9g)\n",item->id,name,p[0]/list->scale,p[1]/list->scale,p[2]/list->scale,n[0],n[1],n[2]);
    }else if(item->type==PRO_EDGE){
        ProEdge edge;double len;ProEnttype type;
        if((e=ProGeomitemToEdge(item,&edge))||(e=ProEdgeLengthEval(edge,&len))||(e=ProEdgeTypeGet(edge,&type))||
           (e=ProEdgeXyzdataEval(edge,0,p,a,b,v))||(e=ProEdgeXyzdataEval(edge,1,n,a,b,v))||
           (e=ProEdgeXyzdataEval(edge,0.5,c,a,b,v)))return e;
        part_text_add(list->out,L"EDGE:%d 特征=%ls 类型=%d 长=%.9g 端点XYZ=(%.9g,%.9g,%.9g)/(%.9g,%.9g,%.9g) 中点XYZ=(%.9g,%.9g,%.9g)\n",item->id,name,type,len/list->scale,p[0]/list->scale,p[1]/list->scale,p[2]/list->scale,n[0]/list->scale,n[1]/list->scale,n[2]/list->scale,c[0]/list->scale,c[1]/list->scale,c[2]/list->scale);
    }else return 0;
    list->ids[list->count]=item->id;list->types[list->count]=item->type;list->count++;return 0;
}
static ProError mc_reference_feature(ProFeature *f,ProError status,ProAppData data){
    ProError e;if(status)return status;
    e=ProFeatureGeomitemVisit(f,PRO_SURFACE,mc_reference_item,NULL,data);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;
    e=ProFeatureGeomitemVisit(f,PRO_EDGE,mc_reference_item,NULL,data);return e==PRO_TK_E_NOT_FOUND?0:e;
}
static ProError mc_references(ProMdl model,wchar_t *report,size_t capacity){
    PartText out={report,capacity,0};McRefList list={&out,1,0,0};ProUnitsystem us;ProUnititem mm,native;ProUnitConversion conv;ProError e;
    report[0]=0;
    if((e=ProMdlPrincipalunitsystemGet(model,&us))||(e=ProUnitsystemUnitGet(&us,PRO_UNITTYPE_LENGTH,&native))||
       (e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&conv)))return e;
    if(!isfinite(conv.scale)||conv.scale<=0||conv.offset!=0)return PRO_TK_BAD_CONTEXT;list.scale=conv.scale;
    part_text_add(&out,L"加工参照，坐标为模型XYZ毫米，边端点和中点由API读取。ID仅在当前模型版本有效。平面点是解析平面的原点，不是面的中心；孔偏移沿定位参考平面几何法向为正。\n");
    e=ProSolidFeatVisit((ProSolid)model,mc_reference_feature,NULL,&list);
    if(e&&e!=PRO_TK_E_NOT_FOUND)return e;
    if(list.truncated||out.truncated)part_text_add(&out,L"参照列表截断；未列出的目标不可猜测。\n");
    return 0;
}
