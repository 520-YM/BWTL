/* Place a cut UDF against exactly one occurrence using a closed assembly
   quilt. Directly redefining an intersecting solid cut is unstable in Creo 10. */
#define AX_FIELD_TARGET_MAX 12
typedef struct {
    ProAsmcomppath paths[AX_FIELD_TARGET_MAX];
    int count;
} AxFieldTargets;

static int ax_field_same_path(const ProAsmcomppath *a,const ProAsmcomppath *b){
    return a->table_num==b->table_num&&
        !memcmp(a->comp_id_table,b->comp_id_table,a->table_num*sizeof(int));
}

static ProError ax_field_target(ProMdl assembly,const wchar_t *token,double mm,AxFieldTargets *targets){
    AxComponent owner={0};
    ProError error=ax_hw_occurrence(assembly,token,mm,&owner);
    if(error)return error;
    if(targets->count==AX_FIELD_TARGET_MAX)return PRO_TK_BAD_INPUTS;
    for(int i=0;i<targets->count;i++)
        if(ax_field_same_path(&targets->paths[i],&owner.path))return PRO_TK_E_FOUND;
    targets->paths[targets->count++]=owner.path;
    return PRO_TK_NO_ERROR;
}

static ProError ax_field_check_hits(ProFeature *feature,const AxFieldTargets *targets){
    ProFeatintersection *hits=NULL;
    int count=0;
    ProError error=ProFeatureAsmintersectionsCollect(feature,&hits);
    if(!error)error=ProArraySizeGet((ProArray)hits,&count);
    if(!error&&(count!=1||targets->count!=1))error=PRO_TK_BAD_CONTEXT;
    if(!error){
        ProAsmcomppath path;
        int level,local_id;
        ProFeatIntersectionType type;
        error=ProFeatintersectionDataGet(hits[0],&path,&level,&local_id);
        if(!error)error=ProFeatintersectionTypeGet(hits[0],&type);
        if(!error&&(level||type!=REGULAR_INTERSECTION||!ax_field_same_path(&path,&targets->paths[0])))
            error=PRO_TK_BAD_CONTEXT;
    }
    if(hits)ProFeatintersectionProarrayFree(hits);
    return error;
}

static ProError ax_field_template(ProPath path){
    wchar_t module[1024];
    DWORD used=GetModuleFileNameW(NULL,module,_countof(module));
    if(!used||used>=_countof(module))return PRO_TK_CANT_ACCESS;
    for(int i=0;i<3;i++){
        wchar_t *separator=wcsrchr(module,L'\\');
        if(!separator)return PRO_TK_BAD_CONTEXT;
        *separator=0;
    }
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\afx\\parts\\udfs\\quilt_cut.gph",module)<0)
        return PRO_TK_BAD_CONTEXT;
    DWORD flags=GetFileAttributesW(path);
    return flags!=INVALID_FILE_ATTRIBUTES&&!(flags&FILE_ATTRIBUTE_DIRECTORY)?PRO_TK_NO_ERROR:PRO_TK_CANT_ACCESS;
}

typedef struct {ProGeomitem item[16];int count;} AxFieldQuilts;

static ProError ax_field_quilt_item(ProGeomitem *item,ProError status,ProAppData context){
    if(status)return status;
    AxFieldQuilts *quilts=context;
    if(quilts->count==_countof(quilts->item))return PRO_TK_BAD_CONTEXT;
    quilts->item[quilts->count++]=*item;
    return PRO_TK_NO_ERROR;
}

static ProError ax_field_depth(ProMdl assembly,const wchar_t *sketch_name,const AxFieldTargets *targets,double *depth){
    FdSketch sketch={0};
    ProMatrix pose;
    ProError error=fd_sketch_open(assembly,sketch_name,&sketch,0);
    if(!error)error=ProSectionLocationGet(sketch.section,pose);
    fd_sketch_free(&sketch);
    if(error)return error;
    AxDeviceFrame frame;
    memcpy(frame.origin,pose[3],sizeof(ProVector));
    for(int i=0;i<3;i++)memcpy(frame.axis[i],pose[i],sizeof(ProVector));
    *depth=0;
    for(int i=0;i<targets->count;i++){
        AxComponent owner={.path=targets->paths[i]};
        ProOutline bounds;
        if((error=ProAsmcomppathMdlGet(&owner.path,&owner.model))||
           (error=ax_device_world_bounds(&owner,&frame,bounds)))return error;
        *depth=fmax(*depth,fmax(fabs(bounds[0][2]),fabs(bounds[1][2])));
    }
    if(!isfinite(*depth)||*depth>1000000)return PRO_TK_BAD_CONTEXT;
    *depth+=1;
    return PRO_TK_NO_ERROR;
}

static ProError ax_field_quilt(ProMdl assembly,const wchar_t *name,const wchar_t *sketch_name,
    int direction,double depth,const AxFieldTargets *targets,AxFieldQuilts *quilts){
    FdTxn tx={0};
    FdRef sketch={0};
    ProSectionShape shape;
    ProElement section,ext_depth,from,to,feature_type;
    ProError error;
#define AF(call) do{error=(call);if(error){trace("field quilt " #call,error);goto done;}}while(0)
    if(!depth){AF(ax_field_depth(assembly,sketch_name,targets,&depth));}
    AF(sd_sketch_ref(assembly,sketch_name,&sketch,&shape));
    /* Reuse the root assembly transaction checks for a non-solid feature. */
    AF(fd_begin(&tx,assembly,name,PRO_FEAT_CURVE));
    AF(mc_tree_element(tx.root,(ProElemId[]){PRO_E_FEATURE_TYPE},1,&feature_type));
    AF(ProElementIntegerSet(feature_type,PRO_FEAT_DATUM_SURF));
    AF(model_element(tx.root,PRO_E_FEATURE_FORM,1,PRO_EXTRUDE,NULL));
    AF(model_element(tx.root,PRO_E_EXT_SURF_CUT_SOLID_TYPE,1,PRO_EXT_FEAT_TYPE_SURFACE,NULL));
    AF(model_element(tx.root,PRO_E_REMOVE_MATERIAL,1,PRO_EXT_MATERIAL_ADD,NULL));
    AF(model_element(tx.root,PRO_E_STD_MATRLSIDE,1,PRO_EXT_MATERIAL_SIDE_TWO,NULL));
    AF(model_element(tx.root,PRO_E_FEAT_FORM_IS_THIN,1,PRO_EXT_FEAT_FORM_NO_THIN,NULL));
    AF(model_element(tx.root,PRO_E_STD_DIRECTION,1,direction<0?PRO_EXT_CR_IN_SIDE_ONE:PRO_EXT_CR_IN_SIDE_TWO,NULL));
    AF(model_element(tx.root,PRO_E_SRF_END_ATTRIBUTES,1,PRO_EXT_SURF_END_ATTR_CAPPED,NULL));
    AF(model_element(tx.root,PRO_E_STD_SECTION,0,0,&section));
    AF(fd_ref_element(&tx,section,PRO_E_SEC_USE_SKETCH,&sketch));
    AF(model_element(tx.root,PRO_E_STD_EXT_DEPTH,0,0,&ext_depth));
    AF(model_element(ext_depth,PRO_E_EXT_DEPTH_FROM,0,0,&from));
    AF(model_element(from,PRO_E_EXT_DEPTH_FROM_TYPE,1,direction?PRO_EXT_DEPTH_FROM_NONE:PRO_EXT_DEPTH_FROM_BLIND,NULL));
    if(!direction){AF(model_element(from,PRO_E_EXT_DEPTH_FROM_VALUE,2,depth,NULL));}
    AF(model_element(ext_depth,PRO_E_EXT_DEPTH_TO,0,0,&to));
    AF(model_element(to,PRO_E_EXT_DEPTH_TO_TYPE,1,PRO_EXT_DEPTH_TO_BLIND,NULL));
    AF(model_element(to,PRO_E_EXT_DEPTH_TO_VALUE,2,depth,NULL));
    AF(fd_create(&tx,0));
    AF(fd_verify(&tx));
    AF(ProFeatureGeomitemVisit(&tx.feature,PRO_QUILT,ax_field_quilt_item,NULL,quilts));
    if(!quilts->count)error=PRO_TK_E_NOT_FOUND;
done:
    fd_ref_free(&sketch);
    fd_cleanup(&tx,error);
    if(!tx.rollback_ok)ax_recovery_uncertain=1;
    return error;
#undef AF
}

static ProError ax_field_place_cut(ProMdl assembly,AxFieldTargets *targets,ProGeomitem *quilt,
    const wchar_t *name,ProFeature *cut){
    ProPath template_path;
    ProUdfdata data=NULL;
    ProUdfreference binding=NULL;
    ProUdfintersection intersection=NULL;
    ProSelection selection=NULL;
    ProFeature *group_features=NULL;
    ProGroup group={0};
    int count=0,found=0;
    ProError error;
#define AF(call) do{error=(call);if(error){trace("field UDF " #call,error);goto done;}}while(0)
    if(targets->count!=1)return PRO_TK_BAD_INPUTS;
    AF(ax_field_template(template_path));
    AF(ProSelectionAlloc(NULL,quilt,&selection));
    AF(ProUdfdataAlloc(&data));
    AF(ProUdfdataPathSet(data,template_path));
    AF(ProUdfdataDependencySet(data,PROUDFDEPENDENCY_INDEPENDENT));
    AF(ProUdfreferenceAlloc(L"REF_QUILT",selection,PRO_B_FALSE,&binding));
    AF(ProUdfdataReferenceAdd(data,binding));
    AF(ProUdfintersectionAlloc(&targets->paths[0],0,NULL,&intersection));
    AF(ProUdfdataIntersectionAdd(data,intersection));
    ProUdfCreateOption options[]={PROUDFOPT_NO_REDEFINE,PROUDFOPT_FIX_MODEL_UI_OFF};
    AF(ProUdfCreate((ProSolid)assembly,data,NULL,options,_countof(options),&group));
    AF(ProGroupFeaturesCollect(&group,&group_features));
    AF(ProArraySizeGet((ProArray)group_features,&count));
    for(int i=0;i<count;i++){
        if(!ax_field_check_hits(&group_features[i],targets)){*cut=group_features[i];found++;}
    }
    if(found!=1){error=PRO_TK_BAD_CONTEXT;goto done;}
    AF(ProModelitemNameSet((ProModelitem *)cut,(wchar_t *)name));
    AF(part_regenerate(assembly));
    AF(ax_field_check_hits(cut,targets));
done:
    if(intersection)ProUdfintersectionFree(intersection);
    if(binding)ProUdfreferenceFree(binding);
    if(selection)ProSelectionFree(&selection);
    if(group_features)ProArrayFree((ProArray *)&group_features);
    if(data)ProUdfdataFree(data);
    return error;
#undef AF
}
