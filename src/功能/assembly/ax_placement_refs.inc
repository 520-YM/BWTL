typedef enum {AX_GEOM_PLANE,AX_GEOM_AXIS,AX_GEOM_CYLINDER} AxGeometryType;
typedef struct {
    AxGeometryType type;
    ProVector point,direction;
    double radius;
} AxGeometry;

static ProError ax_geometry(ProGeomitem *item,AxGeometry *shape){
    memset(shape,0,sizeof(*shape));
    if(item->type==PRO_AXIS){
        FdRef ref={0};
        ref.item=*item;
        shape->type=AX_GEOM_AXIS;
        return ad_line(&ref,shape->point,shape->direction);
    }
    if(item->type!=PRO_SURFACE)return PRO_TK_INVALID_TYPE;
    ProSurface surface;
    ProSrftype type;
    ProError error=ProGeomitemToSurface(item,&surface);
    if(!error)error=ProSurfaceTypeGet(surface,&type);
    if(error)return error;
    if(type==PRO_SRF_PLANE){
        shape->type=AX_GEOM_PLANE;
        error=fd_plane_data(item,shape->point,shape->direction);
    }else if(type==PRO_SRF_CYL){
        ProGeomitemdata *raw=NULL;
        shape->type=AX_GEOM_CYLINDER;
        if((error=ProSurfaceDataGet(surface,&raw)))return error;
        const ProCylinderdata *cylinder=&raw->data.p_surface_data->srf_shape.cylinder;
        memcpy(shape->point,cylinder->origin,sizeof(shape->point));
        memcpy(shape->direction,cylinder->e3,sizeof(shape->direction));
        shape->radius=cylinder->radius;
        ProGeomitemdataFree(&raw);
        if(!isfinite(shape->radius)||shape->radius<=0)return PRO_TK_BAD_CONTEXT;
    }else return PRO_TK_INVALID_TYPE;
    if(error)return error;
    return ad_unit(shape->direction)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_root_path(ProMdl assembly,ProAsmcomppath *path){
    ProIdTable ids={0};
    return ProAsmcomppathInit((ProSolid)assembly,ids,0,path);
}

static ProError ax_ref_occurrence(ProMdl assembly,const wchar_t *token,AxComponent *component){
    if(wcscmp(token,L"ROOT"))return ax_component_resolve(assembly,token,component);
    component->model=assembly;
    return ax_root_path(assembly,&component->path);
}

static ProError ax_ref_resolve(AxComponent *component,const wchar_t *token,int assembly_side,
    ProSelection *selection,AxGeometry *shape){
    FdRef ref={0};
    ProFeature feature;
    ProFeatStatus state;
    ProError error=fd_ref_resolve(component->model,token,PRO_TYPE_UNUSED,&ref);
    if(!error)error=ProGeomitemFeatureGet(&ref.item,&feature);
    if(!error)error=ProFeatureStatusGet(&feature,&state);
    if(!error&&state!=PRO_FEAT_ACTIVE)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ax_geometry(&ref.item,shape);
    if(!error)error=ProSelectionAlloc(assembly_side?&component->path:NULL,&ref.item,selection);
    fd_ref_free(&ref);
    return error;
}

typedef struct {AxComponent *component;PartText *out;double mm;int count,limited;} AxRefList;

static ProError ax_list_ref(ProGeomitem *item,ProError status,ProAppData data){
    AxRefList *list=data;
    if(status)return status;
    if(list->count>=96){list->limited=1;return PRO_TK_NO_ERROR;}
    AxGeometry shape;
    if(ax_geometry(item,&shape))return PRO_TK_NO_ERROR;
    ProFeature owner;
    ProName name=L"";
    if(!ProGeomitemFeatureGet(item,&owner))ProModelitemNameGet(&owner,name);
    if(item->type==PRO_AXIS){
        if(!fd_name(name))return PRO_TK_NO_ERROR;
        part_text_add(list->out,L"AXIS:%ls 基准轴",name);
    }else{
        part_text_add(list->out,L"SURFACE:%d %ls %ls",item->id,name,
            shape.type==AX_GEOM_PLANE?L"平面":L"圆柱面");
        if(shape.type==AX_GEOM_CYLINDER){
            wchar_t token[40];
            FdRef ref={0};
            QualityBore bore;
            swprintf_s(token,_countof(token),L"SURFACE:%d",item->id);
            int hole=!qa_bore(list->component->model,token,&ref,&bore);
            fd_ref_free(&ref);
            part_text_add(list->out,L"%ls Ø%.6g mm",hole?L"（孔壁）":L"",2*shape.radius*list->mm);
        }
    }
    part_text_add(list->out,L" 点(%.6g, %.6g, %.6g) mm 方向(%.6g, %.6g, %.6g)\n",
        shape.point[0]*list->mm,shape.point[1]*list->mm,shape.point[2]*list->mm,
        shape.direction[0],shape.direction[1],shape.direction[2]);
    list->count++;
    return PRO_TK_NO_ERROR;
}

static ProError ax_ref_feature(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProFeatStatus state;
    ProFeattype type;
    ProError error=ProFeatureStatusGet(feature,&state);
    if(!error)error=ProFeatureTypeGet(feature,&type);
    if(error||state!=PRO_FEAT_ACTIVE||type==PRO_FEAT_COMPONENT)return error;
    error=ProFeatureGeomitemVisit(feature,PRO_SURFACE,ax_list_ref,NULL,data);
    if(!error||error==PRO_TK_E_NOT_FOUND)
        error=ProFeatureGeomitemVisit(feature,PRO_AXIS,ax_list_ref,NULL,data);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}

static ProError ax_placement_refs(ProMdl assembly,const wchar_t *token,PartText *out){
    AxComponent component={0};
    AxRefList list={&component,out,0,0,0};
    ProMdlName name;
    ProError error=ax_ref_occurrence(assembly,token,&component);
    if(!error)error=qa_mm_per_unit(component.model,&list.mm);
    if(!error)error=ProMdlMdlnameGet(component.model,name);
    if(error)return error;
    part_text_add(out,L"%ls %ls 的定位参照，坐标在该模型自身坐标系。ID 仅在当前模型版本有效。\n",token,name);
    error=ProSolidFeatVisit((ProSolid)component.model,ax_ref_feature,NULL,&list);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error)part_text_add(out,L"共列出 %d 条参照。%ls\n",list.count,list.limited?L"达到列表上限。":L"");
    return error;
}
