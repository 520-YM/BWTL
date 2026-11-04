static ProError model_build_internal(const ModelPlan *p, ProMdlName name, const char **stage, ProMdl target, int *restored, ProFeature *created_out) {
    static unsigned serial = 0;
    SYSTEMTIME now;
    ProError error = PRO_TK_BAD_INPUTS;
    ProSolid solid = NULL;
    ProMdl template_model = NULL, copied_model = NULL;
    ProPath template_path;
    TemplateDatums datums = {0};
    ProModelitem item;
    ProSelection selection = NULL;
    ProReference plane_reference = NULL, orientation_reference = NULL;
    ProElement root = NULL, depth = NULL, from = NULL, to = NULL, extracted = NULL, sketch;
    ProElement section_element = NULL, setup = NULL, body_element = NULL;
    ProFeature feature;
    ProFeatureCreateOptions *options = NULL;
    ProErrorlist errors = {0};
    ProElempath path = NULL;
    ProElempathItem path_items[2];
    ProSection section = NULL, original_section = NULL;
    ProUnitsystem system;
    ProUnititem millimetres, native_unit;
    ProUnitConversion conversion;
    ProMassProperty mass;
    int feature_created = 0, section_attached = 0, window;
    ProMdlType target_type; ProMdlsubtype target_subtype;
    if(restored)*restored=1; if(created_out)memset(created_out,0,sizeof(*created_out));
    int *parents = NULL, parent_count = 0;
    double expected, scale;
    if (!model_plan_valid(p)) { *stage = "Validate plan"; return error; }
    if(target){
        MODEL_TRY("Verify current target is a part",ProMdlTypeGet(target,&target_type));
        MODEL_TRY("Verify target subtype",ProMdlSubtypeGet(target,&target_subtype));
        if(target_type!=PRO_MDL_PART || target_subtype==PROMDLSTYPE_PART_SHEETMETAL){*stage="Current extrusion requires an empty solid part";error=PRO_TK_BAD_CONTEXT;goto cleanup;}
        MODEL_TRY("Verify current target has no solid geometry",template_empty((ProSolid)target));
        MODEL_TRY("Read current target name",ProMdlMdlnameGet(target,name));
        solid=(ProSolid)target;
    }else{
    GetLocalTime(&now);
    swprintf_s(name, PRO_NAME_SIZE, L"ai_%02u%02u%02u_%02u%02u%02u_%u", now.wYear%100, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, ++serial);
    MODEL_TRY("Read configured solid template", ProConfigoptionGet(L"template_solidpart", template_path));
    if (!template_path[0]) { error = PRO_TK_E_NOT_FOUND; goto cleanup; }
    MODEL_TRY("Load configured template", ProMdlFiletypeLoad(template_path, PRO_MDLFILE_PART, PRO_B_FALSE, &template_model));
    MODEL_TRY("Verify empty template", template_empty((ProSolid)template_model));
    MODEL_TRY("Copy template to new part", ProMdlnameCopy(template_model, name, &copied_model));
    solid = (ProSolid)copied_model;
#ifndef CREO_MODEL_HEADLESS_TEST
    if(!model_background_mode){
    MODEL_TRY("Open copied template window", ProObjectwindowMdlnameCreate(name, PRO_PART, &window));
    MODEL_TRY("Set copied template graphics context", ProWindowCurrentSet(window));
    MODEL_TRY("Display copied template", ProMdlDisplay((ProMdl)solid));
    }
#endif
    }
    MODEL_TRY("Collect copied template datums", ProSolidFeatVisit(solid, template_feature, NULL, &datums));
    trace("Template datum planes", datums.planes); trace("Template coordinate systems", datums.csys);
    if (datums.planes < 3 || datums.csys < 1) { *stage = "Template requires 3 datum planes and coordinate system"; error = PRO_TK_BAD_CONTEXT; goto cleanup; }
    MODEL_TRY("Read model unit system", ProMdlPrincipalunitsystemGet((ProMdl)solid, &system));
    MODEL_TRY("Read length unit", ProUnitsystemUnitGet(&system, PRO_UNITTYPE_LENGTH, &native_unit));
    MODEL_TRY("Resolve millimetres", ProUnitInit((ProMdl)solid, L"mm", &millimetres));
    MODEL_TRY("Convert millimetres", ProUnitConversionCalculate(&millimetres, &native_unit, &conversion));
    scale = conversion.scale;
    if (!isfinite(scale) || scale <= 0 || conversion.offset != 0) { error = PRO_TK_BAD_INPUTS; goto cleanup; }
    /* Solve an independent 2-D section before creating any incomplete
       feature. Extracted placeholder sections are not standalone solver
       workspaces even though database entity and dimension setters succeed. */
    MODEL_TRY("Allocate independent 2D sketch", ProSection2DAlloc(&section));
    MODEL_TRY("Solve independent 2D sketch", model_section(p, scale, &section, stage));
    MODEL_TRY("Model item", ProMdlToModelitem((ProMdl)solid, &item));
    MODEL_TRY("Model selection", ProSelectionAlloc(NULL, &item, &selection));
    MODEL_TRY("Create options", ProArrayAlloc(1, sizeof(ProFeatureCreateOptions), 1, (ProArray *)&options));
    /* SDK first-solid path: fully solve 2-D section, create the first
       incomplete solid, then supply its root sketch and finalize. */
    if (root) ProElementFree(&root);
    MODEL_TRY("Allocate base feature tree", ProElementAlloc(PRO_E_FEATURE_TREE, &root));
    MODEL_TRY("Base feature type", model_element(root, PRO_E_FEATURE_TYPE, 1, PRO_FEAT_FIRST_FEAT, NULL));
    MODEL_TRY("Base extrusion form", model_element(root, PRO_E_FEATURE_FORM, 1, PRO_EXTRUDE, NULL));
    MODEL_TRY("Base depth", model_element(root, PRO_E_STD_EXT_DEPTH, 0, 0, &depth));
    MODEL_TRY("Base from", model_element(depth, PRO_E_EXT_DEPTH_FROM, 0, 0, &from));
    MODEL_TRY("Base from type", model_element(from, PRO_E_EXT_DEPTH_FROM_TYPE, 1, PRO_EXT_DEPTH_FROM_BLIND, NULL));
    MODEL_TRY("Base from value", model_element(from, PRO_E_EXT_DEPTH_FROM_VALUE, 2, p->height*scale, NULL));
    MODEL_TRY("Base to", model_element(depth, PRO_E_EXT_DEPTH_TO, 0, 0, &to));
    MODEL_TRY("Base to type", model_element(to, PRO_E_EXT_DEPTH_TO_TYPE, 1, PRO_EXT_DEPTH_TO_BLIND, NULL));
    MODEL_TRY("Base to value", model_element(to, PRO_E_EXT_DEPTH_TO_VALUE, 2, 0, NULL));
    options[0]=PRO_FEAT_CR_INCOMPLETE_FEAT;
    if(target && restored)*restored=0;
    MODEL_TRY("Create base incomplete extrusion", ProFeatureWithoptionsCreate(selection,root,options,PRO_REGEN_NO_FLAGS,&feature,&errors));
    feature_created=1;
    MODEL_TRY("Extract base tree",ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&extracted));
    MODEL_TRY("Allocate base sketch path",ProElempathAlloc(&path));
    path_items[0].type=PRO_ELEM_PATH_ITEM_TYPE_ID;path_items[0].path_item.elem_id=PRO_E_SKETCHER;
    MODEL_TRY("Base sketch path",ProElempathDataSet(path,path_items,1));
    MODEL_TRY("Get base sketch",ProElemtreeElementGet(extracted,path,&sketch));
    MODEL_TRY("Get base placeholder",ProElementSpecialvalueGet(sketch,NULL,(ProAppData *)&original_section));
    MODEL_TRY("Assign solved base sketch",ProElementSpecialvalueSet(sketch,(ProAppData)section));
    section_attached=1;
    options[0]=PRO_FEAT_CR_DEFINE_MISS_ELEMS;
    MODEL_TRY("Complete base extrusion",ProFeatureWithoptionsRedefine(NULL,&feature,extracted,options,PRO_REGEN_NO_FLAGS,&errors));

    MODEL_TRY("Regenerate new part", ProSolidRegenerate(solid, PRO_REGEN_NO_RESOLVE_MODE | PRO_REGEN_UNDO_IF_FAIL));
    MODEL_TRY("Verify template feature references", ProFeatureParentsGet(&feature, &parents, &parent_count));
    trace("Extrusion parent feature count", parent_count);
    /* First solid preserves template features but does not depend on their movement. */
    MODEL_TRY("Check resulting volume", ProSolidMassPropertyWithDensityGet(solid, NULL, PRO_MP_DENS_USE_ALWAYS, 1.0, &mass));
    expected = model_plan_volume(p)*scale*scale*scale;
    if (!isfinite(mass.volume) || fabs(mass.volume-expected) > expected*0.0001) { *stage = "Result volume mismatch"; error = PRO_TK_GENERAL_ERROR; goto cleanup; }
#ifndef CREO_MODEL_HEADLESS_TEST
    if(!model_background_mode && !target){
    MODEL_TRY("Create part window", ProObjectwindowMdlnameCreate(name, PRO_PART, &window));
    MODEL_TRY("Display new part", ProMdlDisplay((ProMdl)solid));
    MODEL_TRY("Activate new part window", ProWindowActivate(window));
    }
#endif
    if(created_out)*created_out=feature;
    /* Do not activate a window or simulate input. User can switch themselves. */
cleanup:
    if (path) ProElempathFree(&path);
    if (parents) ProArrayFree((ProArray *)&parents);
    /* Restore the extracted tree's owned placeholder before freeing it.
       The separately allocated section is caller-owned. On a failed restore,
       avoid freeing it twice and let Toolkit clean up the attached value. */
    if (section_attached && ProElementSpecialvalueSet(sketch, (ProAppData)original_section) != PRO_TK_NO_ERROR) section = NULL;
    if (extracted && feature_created) ProFeatureElemtreeFree(&feature, extracted);
    if (root) ProElementFree(&root);
    if (plane_reference) ProReferenceFree(plane_reference);
    if (orientation_reference) ProReferenceFree(orientation_reference);
    if (options) ProArrayFree((ProArray *)&options);
    if (selection) ProSelectionFree(&selection);
    if (section) ProSectionFree(&section);
    if(target && error!=PRO_TK_NO_ERROR && restored && !*restored){
        ProError rollback=0;TemplateDatums after={0};
        if(feature_created)rollback=model_remove_feature((ProSolid)target,feature.id);
        if(!rollback)rollback=ProSolidRegenerate((ProSolid)target,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
        if(!rollback)rollback=ProSolidFeatVisit((ProSolid)target,template_feature,NULL,&after);
        if(!rollback && after.total==datums.total && after.planes==datums.planes && after.csys==datums.csys && !template_empty((ProSolid)target))*restored=1;
        trace("Current target rollback verified",*restored?0:-1);
    }
    /* Keep any failed new part in session for diagnosis, never erase user data. */
    return error;
}
