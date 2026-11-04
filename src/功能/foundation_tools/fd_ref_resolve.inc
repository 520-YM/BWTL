static ProError fd_ref_resolve(ProMdl model,const wchar_t *token,ProType expected,FdRef *out){
    const wchar_t *value=token,*colon=wcschr(token,L':');ProType type=expected;ProModelitem feature;
    ProFeature owner;ProError e;int id=0,by_id=0;wchar_t kind[24];
    memset(out,0,sizeof(*out));
    if(!wcscmp(token,L"SELECTED")){
        ProWVerstamp stamp=NULL;int equal;
        if(fd_selected_model!=model||!fd_selected_stamp)return PRO_TK_BAD_CONTEXT;
        if((e=ProMdlVerstampGet(model,&stamp)))return e;
        equal=ProVerstampEqual(stamp,fd_selected_stamp);ProVerstampFree(&stamp);if(!equal)return PRO_TK_BAD_CONTEXT;
        out->item=fd_selected_item;type=out->item.type;
    }else{
        if(colon){
            wcsncpy_s(kind,24,token,(size_t)(colon-token));value=colon+1;
            if(!wcscmp(kind,L"PLANE"))type=PRO_SURFACE;
            else if(!wcscmp(kind,L"SURFACE")){type=PRO_SURFACE;by_id=1;}
            else if(!wcscmp(kind,L"EDGE")){type=PRO_EDGE;by_id=1;}
            else if(!wcscmp(kind,L"AXIS"))type=PRO_AXIS;
            else if(!wcscmp(kind,L"POINT"))type=PRO_POINT;
            else if(!wcscmp(kind,L"CSYS"))type=PRO_CSYS;
            else if(!wcscmp(kind,L"CURVE"))type=PRO_CURVE;
            else return PRO_TK_BAD_INPUTS;
        }
        if(type==PRO_TYPE_UNUSED){
            ProFeattype ft;if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)value,&feature))||(e=ProFeatureTypeGet((ProFeature *)&feature,&ft)))return e;
            type=ft==PRO_FEAT_DATUM?PRO_SURFACE:ft==PRO_FEAT_DATUM_AXIS?PRO_AXIS:ft==PRO_FEAT_DATUM_POINT?PRO_POINT:ft==PRO_FEAT_CSYS?PRO_CSYS:ft==PRO_FEAT_CURVE?PRO_CURVE:PRO_TYPE_UNUSED;
            if(type==PRO_TYPE_UNUSED)return PRO_TK_INVALID_TYPE;
        }
        if(by_id){if(!part_integer(value,&id))return PRO_TK_BAD_INPUTS;e=ProModelitemInit(model,id,type,&out->item);if(e)return e;}
        else{
            e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)value,&feature);if(e)return e;
            e=fd_feature_geom((ProFeature *)&feature,type,&out->item);if(e)return e;
        }
    }
    if(expected!=PRO_TYPE_UNUSED&&out->item.type!=expected)return PRO_TK_INVALID_TYPE;
    if((e=ProGeomitemFeatureGet(&out->item,&owner)))return e;out->owner_feature=owner.id;
    if((e=ProSelectionAlloc(NULL,(ProModelitem *)&out->item,&out->selection)))return e;
    return 0;
}
