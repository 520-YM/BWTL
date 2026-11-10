typedef struct {ProMdl model;PartText *text;double scale;int quilts,faces,edges;ProSurface surface;} SfInfo;
static ProError sf_info_edge(ProEdge edge,ProError status,ProAppData data){
    SfInfo *info=data;
    ProVector start,end;
    int id;
    ProError e;
    if(status)return status;
    if(++info->edges>192)return PRO_TK_OUT_OF_RANGE;
    if((e=ProEdgeIdGet(edge,&id))||(e=ProEdgeXyzdataEval(edge,0,start,NULL,NULL,NULL))||
       (e=ProEdgeXyzdataEval(edge,1,end,NULL,NULL,NULL)))return e;
    part_text_add(info->text,L"    EDGE:%d，端点 (%.7g,%.7g,%.7g) → (%.7g,%.7g,%.7g) mm\n",
        id,start[0]/info->scale,start[1]/info->scale,start[2]/info->scale,
        end[0]/info->scale,end[1]/info->scale,end[2]/info->scale);
    return PRO_TK_NO_ERROR;
}
static ProError sf_info_contour(ProContour contour,ProError status,ProAppData data){
    SfInfo *info=data;
    if(status)return status;
    return ProContourEdgeVisit(info->surface,contour,sf_info_edge,NULL,info);
}
static ProError sf_info_face(ProSurface surface,ProError status,ProAppData data){
    SfInfo *info=data;
    int id;
    double area;
    ProError e;
    if(status)return status;
    if(++info->faces>96)return PRO_TK_OUT_OF_RANGE;
    if((e=ProSurfaceIdGet(surface,&id))||(e=ProSurfaceAreaEval(surface,&area)))return e;
    part_text_add(info->text,L"  SURFACE:%d，面积 %.9g mm²\n",id,area/pow(info->scale,2));
    info->surface=surface;
    return ProSurfaceContourVisit(surface,sf_info_contour,NULL,info);
}
static ProError sf_info_quilt(ProQuilt quilt,ProError status,ProAppData data){
    SfInfo *info=data;
    ProGeomitem item;
    ProFeature feature;
    ProName name=L"";
    ProError e;
    if(status)return status;
    if(++info->quilts>64)return PRO_TK_OUT_OF_RANGE;
    if((e=ProQuiltToGeomitem((ProSolid)info->model,quilt,&item))||
       (e=ProGeomitemFeatureGet(&item,&feature))||
       (e=ProModelitemNameGet((ProModelitem *)&feature,name)))return e;
    part_text_add(info->text,L"QUILT:%d，特征 %ls\n",item.id,name);
    return ProQuiltSurfaceVisit(quilt,sf_info_face,NULL,info);
}
static ProError sf_info(ProMdl model,wchar_t *report,size_t capacity){
    PartText text={report,capacity,0};
    SfInfo info={0};
    ProUnitsystem units;
    ProUnititem millimeters,native;
    ProUnitConversion conversion;
    ProSolidBody *bodies=NULL;
    int n=0;
    ProError e;
    if((e=ProMdlPrincipalunitsystemGet(model,&units))||
       (e=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&native))||
       (e=ProUnitInit(model,L"mm",&millimeters))||
       (e=ProUnitConversionCalculate(&millimeters,&native,&conversion)))return e;
    if(!isfinite(conversion.scale)||conversion.scale<=0||conversion.offset!=0)return PRO_TK_BAD_CONTEXT;
    report[0]=0;
    info.model=model;info.text=&text;info.scale=conversion.scale;
    part_text_add(&text,L"当前曲面与多实体参照，ID 仅适用于本次模型状态。\n");
    e=ProSolidQuiltVisit((ProSolid)model,sf_info_quilt,NULL,&info);
    if(e==PRO_TK_E_NOT_FOUND)e=0;
    if(e)goto done;
    e=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(e==PRO_TK_E_NOT_FOUND)e=0;
    if(e||!bodies)goto done;
    if((e=ProArraySizeGet((ProArray)bodies,&n)))goto done;
    for(int i=0;i<n;i++){
        ProSolidBodyState state;
        ProMassProperty mass;
        if((e=ProSolidBodyStateGet(&bodies[i],&state)))goto done;
        if(state!=PRO_BODY_STATE_ACTIVE)continue;
        if((e=ProSolidBodyMassPropertyGet(&bodies[i],NULL,&mass)))goto done;
        part_text_add(&text,L"BODY:%d，体积 %.9g mm³\n",bodies[i].id,mass.volume/pow(info.scale,3));
    }
done:
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return e;
}
