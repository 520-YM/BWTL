typedef struct {ProMdl model;PartText *out;double scale;int count;} AmInfo;
static ProError am_info_surface(ProSurface surface,ProError status,ProAppData data){
    AmInfo *info=data;ProSrftype type;ProGeomitemdata *raw=NULL;ProError e;int id;
    if(status)return status;
    if((e=ProSurfaceTypeGet(surface,&type)))return e;
    if(type!=PRO_SRF_CYL&&type!=PRO_SRF_CONE)return 0;
    if(++info->count>96)return PRO_TK_OUT_OF_RANGE;
    if((e=ProSurfaceIdGet(surface,&id))||(e=ProSurfaceDataGet(surface,&raw)))return e;
    {ProSurfacedata *s=raw->data.p_surface_data;double *origin,*axis,radius;
     if(type==PRO_SRF_CYL){origin=s->srf_shape.cylinder.origin;axis=s->srf_shape.cylinder.e3;radius=s->srf_shape.cylinder.radius;}
     else {origin=s->srf_shape.cone.origin;axis=s->srf_shape.cone.e3;radius=s->srf_shape.cone.alpha*180/AS_PI*info->scale;}
     part_text_add(info->out,L"SURFACE:%d %ls 轴点=(%.9g,%.9g,%.9g) mm 轴向=(%.9g,%.9g,%.9g) %ls=%.9g，参数范围=(%.9g,%.9g)/(%.9g,%.9g)\n",id,type==PRO_SRF_CYL?L"圆柱面":L"圆锥面",origin[0]/info->scale,origin[1]/info->scale,origin[2]/info->scale,axis[0],axis[1],axis[2],type==PRO_SRF_CYL?L"半径mm":L"半角度",radius/info->scale,s->uv_min[0],s->uv_min[1],s->uv_max[0],s->uv_max[1]);}
    ProGeomitemdataFree(&raw);return 0;
}
static ProError am_info(ProMdl model,wchar_t *report,size_t capacity){
    ProError e=mc_references(model,report,capacity);PartText text={report,capacity,0};AmInfo info={model,&text,1,0};
    ProUnitsystem units;ProUnititem mm,native;ProUnitConversion conv;
    if(e)return e;
    if((e=ProMdlPrincipalunitsystemGet(model,&units))||(e=ProUnitsystemUnitGet(&units,PRO_UNITTYPE_LENGTH,&native))||
       (e=ProUnitInit(model,L"mm",&mm))||(e=ProUnitConversionCalculate(&mm,&native,&conv)))return e;
    info.scale=conv.scale;
    part_text_add(&text,L"\n当前实体曲面入口参照。孔位置须结合实际轴向、基准和本轮参数；不能以历史编号代替。\n");
    return ProSolidSurfaceVisit((ProSolid)model,am_info_surface,NULL,&info);
}
