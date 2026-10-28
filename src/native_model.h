/* First extrusion follows PTC's UgSktFirstFeatureCreate sample.
   Operates only on a freshly created, unique model; never on active geometry. */
#ifndef CREO_NATIVE_MODEL_H
#define CREO_NATIVE_MODEL_H
#include <ProSolid.h>
#include <ProMdlUnits.h>
#include <ProFeature.h>
#include <ProElemId.h>
#include <ProElement.h>
#include <ProElempath.h>
#include <ProExtrude.h>
#include <ProModFeat.h>
#include <ProFeatType.h>
#include <ProFeatForm.h>
#include <ProSection.h>
#include <ProSelection.h>
#include <ProArray.h>
#include <ProWindows.h>
#include <ProSolidBody.h>
#include <ProSurface.h>
#include <ProGeomitem.h>
#include <ProReference.h>
#include <ProStdSection.h>
#include <ProSecdim.h>
#include <ProBodyOpts.h>
#include "model_plan.h"
/* Internal validation can create its own disposable part without changing
   the user's active graphics window. Normal user modeling still displays. */
static int model_background_mode=0;

typedef struct { int planes, csys; ProGeomitem plane[32]; ProVector normal[32]; int total, components; } TemplateDatums;
static ProError template_surface(ProGeomitem *item, ProError status, ProAppData data) {
    TemplateDatums *d = (TemplateDatums *)data;
    ProSurface surface; ProSrftype type;
    ProUvParam uv = {0,0}; ProVector point, first[2], second[3], normal;
    if (status != PRO_TK_NO_ERROR) return status;
    if (d->planes >= 32) return PRO_TK_NO_ERROR;
    if (ProGeomitemToSurface(item, &surface) != PRO_TK_NO_ERROR) return PRO_TK_GENERAL_ERROR;
    if (ProSurfaceTypeGet(surface, &type) != PRO_TK_NO_ERROR || type != PRO_SRF_PLANE) return PRO_TK_NO_ERROR;
    if (ProSurfaceXyzdataEval(surface, uv, point, first, second, normal) != PRO_TK_NO_ERROR) return PRO_TK_GENERAL_ERROR;
    d->plane[d->planes] = *item;
    memcpy(d->normal[d->planes], normal, sizeof(ProVector)); d->planes++;
    return PRO_TK_NO_ERROR;
}
static ProError template_feature(ProFeature *feature, ProError status, ProAppData data) {
    ProFeattype type;
    TemplateDatums *d = (TemplateDatums *)data;
    if (status != PRO_TK_NO_ERROR) return status;
    if (ProFeatureTypeGet(feature, &type) != PRO_TK_NO_ERROR) return PRO_TK_GENERAL_ERROR;
    d->total++; if(type==PRO_FEAT_COMPONENT)d->components++;
    if (type == PRO_FEAT_CSYS) d->csys++;
    if (type == PRO_FEAT_DATUM) return ProFeatureGeomitemVisit(feature, PRO_SURFACE, template_surface, NULL, data);
    return PRO_TK_NO_ERROR;
}
static ProError template_empty(ProSolid solid) {
    ProSolidBody *bodies = NULL;
    ProError error = ProSolidBodiesCollect(solid, &bodies);
    int count = 0, i;
    if (error != PRO_TK_NO_ERROR) return error;
    if (bodies) error = ProArraySizeGet((ProArray)bodies, &count);
    for (i = 0; error == PRO_TK_NO_ERROR && i < count; ++i) {
        ProSolidBodyState state;
        error = ProSolidBodyStateGet(&bodies[i], &state);
        if (error == PRO_TK_NO_ERROR && state == PRO_BODY_STATE_ACTIVE) error = PRO_TK_BAD_CONTEXT;
    }
    if (bodies) ProArrayFree((ProArray *)&bodies);
    return error;
}
static ProError model_element(ProElement parent, ProElemId id, int kind, double value, ProElement *out) {
    ProElement child = NULL;
    ProError error = ProElementAlloc(id, &child);
    if (error != PRO_TK_NO_ERROR) return error;
    if (kind == 1) error = ProElementIntegerSet(child, (int)value);
    if (kind == 2) error = ProElementDoubleSet(child, value);
    if (error == PRO_TK_NO_ERROR) error = ProElemtreeElementAdd(parent, NULL, child);
    if (error != PRO_TK_NO_ERROR) { ProElementFree(&child); return error; }
    if (out) *out = child;
    return error;
}
static ProError model_circle(ProSection section, double x, double y, double radius, int *entity_id) {
    Pro2dCircledef circle;
    int id;
    memset(&circle, 0, sizeof(circle));
    circle.type = PRO_2D_CIRCLE; circle.center[0] = x; circle.center[1] = y; circle.radius = radius;
    ProError error = ProSectionEntityAdd(section, (Pro2dEntdef *)&circle, &id);
    if (error == PRO_TK_NO_ERROR && entity_id) *entity_id = id;
    return error;
}
static ProError model_section_origin(ProSection section) {
    ProIntlist ids = NULL; int count = 0, i, found = 0, id;
    ProError error = ProSectionEntityIdsGet(section, &ids, &count);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_E_NOT_FOUND) return error;
    for (i = 0; i < count; ++i) {
        Pro2dEntdef *entity = NULL;
        error = ProSectionEntityGet(section, ids[i], &entity);
        if (error != PRO_TK_NO_ERROR) break;
        if (entity->type == PRO_2D_COORD_SYS) found = 1;
    }
    if (ids) ProArrayFree((ProArray *)&ids);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_E_NOT_FOUND) return error;
    if (!found) {
        Pro2dCoordSysdef origin = {0};
        origin.type = PRO_2D_COORD_SYS;
        return ProSectionEntityAdd(section, (Pro2dEntdef *)&origin, &id);
    }
    return PRO_TK_NO_ERROR;
}
static ProError model_section_dimension(ProSection section, int entity, ProSecdimType type, double value, double x, double y) {
    ProSectionPointType point_type = PRO_ENT_WHOLE;
    Pro2dPnt placement = {x,y};
    int dimension;
    ProError error = ProSecdimCreate(section, &entity, &point_type, 1, type, placement, &dimension);
    if (error == PRO_TK_NO_ERROR) error = ProSecdimValueSet(section, dimension, value);
    return error;
}
static void model_section_errors(ProWSecerror *errors, const char *stage) {
    int count = 0, i;
    FILE *log;
    if (ProSecerrorCount(errors, &count) != PRO_TK_NO_ERROR) return;
    trace("Section solver error count", count);
    log = runtime_open(L"section-errors.txt",L"wt, ccs=UTF-8");
    if (!log) return;
    fwprintf(log, L"Failed stage: %hs\n", stage);
    for (i = 0; i < count; ++i) {
        ProMsg message = L"";
        int entity = -1;
        ProSecerrorMsgGet(*errors, i, message);
        ProSecerrorItemGet(*errors, i, &entity);
        fwprintf(log, L"Entity %d: %ls\n", entity, message);
    }
    fclose(log);
}
#define SECTION_TRY(label, call) do { *stage = label; error = (call); trace(label, error); if (error != PRO_TK_NO_ERROR) goto done; } while (0)
static ProError model_section(const ModelPlan *p, double scale, ProSection *section, const char **stage) {
    ProError error;
    ProWSecerror errors = NULL;
    ProBoolean intent_manager = PRO_B_FALSE;
    int i, id, edges[4] = {0}, outer_id = 0, hole_id = 0;
    double a = p->a * scale, b = p->b * scale;
    double points[5][2] = {{0,0},{1,0},{1,1},{0,1},{0,0}};
    if (!*section) return PRO_TK_BAD_INPUTS;
    SECTION_TRY("Read sketch intent-manager mode", ProSectionIntentManagerModeGet(*section, &intent_manager));
    if (intent_manager) SECTION_TRY("Disable interactive sketch solver", ProSectionIntentManagerModeSet(*section, PRO_B_FALSE));
    SECTION_TRY("Initialize sketch coordinate system", model_section_origin(*section));
    if (p->cylinder) {
        SECTION_TRY("Sketch outer circle", model_circle(*section, a/2, b/2, a/2, &outer_id));
    }
    else for (i = 0; i < 4; ++i) {
        Pro2dLinedef line;
        memset(&line, 0, sizeof(line)); line.type = PRO_2D_LINE;
        line.end1[0] = points[i][0]*a; line.end1[1] = points[i][1]*b;
        line.end2[0] = points[i+1][0]*a; line.end2[1] = points[i+1][1]*b;
        SECTION_TRY("Sketch rectangle edge", ProSectionEntityAdd(*section, (Pro2dEntdef *)&line, &id));
        edges[i] = id;
    }
    if (p->hole > 0) SECTION_TRY("Sketch center hole", model_circle(*section, a/2, b/2, p->hole*scale/2, &hole_id));
    if (p->cylinder) {
        SECTION_TRY("Dimension outer diameter", model_section_dimension(*section, outer_id, PRO_TK_DIM_DIA, a, a*1.1, b*0.8));
    } else {
        SECTION_TRY("Dimension rectangle width", model_section_dimension(*section, edges[0], PRO_TK_DIM_LINE, a, a/2, -b*0.1));
        SECTION_TRY("Dimension rectangle height", model_section_dimension(*section, edges[1], PRO_TK_DIM_LINE, b, a*1.1, b/2));
    }
    if (p->hole > 0) SECTION_TRY("Dimension hole diameter", model_section_dimension(*section, hole_id, PRO_TK_DIM_DIA, p->hole*scale, a*0.8, b*0.8));
    SECTION_TRY("Allocate sketch diagnostics", ProSecerrorAlloc(&errors));
    /* Autodim both adds remaining dimensions and solves; calling Solve on
       an undimensioned rectangle first can fail before it is constrained. */
    SECTION_TRY("Set scale-aware sketch tolerance", ProSectionEpsilonSet(*section, 0.00001 * ((p->hole > 0 && p->hole*scale < (a < b ? a : b)) ? p->hole*scale : (a < b ? a : b))));
    SECTION_TRY("Autodimension and solve dimensioned sketch", ProSectionAutodim(*section, &errors));
    SECTION_TRY("Regenerate solved sketch", ProSectionRegenerate(*section, &errors));
done:
    if (errors) {
        if (error != PRO_TK_NO_ERROR) model_section_errors(&errors, *stage);
        ProSecerrorFree(&errors);
    }
    return error;
}
#undef SECTION_TRY
#define MODEL_TRY(stage_name, call) do { *stage = stage_name; error = (call); trace(stage_name, error); if (error != PRO_TK_NO_ERROR) goto cleanup; } while (0)
static ProError model_create_empty(int kind, ProMdlName name, const char **stage) {
    static unsigned serial;
    SYSTEMTIME now;ProPath path;ProMdl source=NULL,copy=NULL;
    ProMdlType type;ProMdlsubtype subtype;
    ProMdlfileType filetype=kind==3?PRO_MDLFILE_ASSEMBLY:PRO_MDLFILE_PART;
    TemplateDatums before={0},after={0};ProError error=PRO_TK_BAD_INPUTS;int window;
    if(kind<1||kind>3){*stage="Unsupported empty model type";return error;}
    GetLocalTime(&now);
    swprintf_s(name,PRO_NAME_SIZE,L"ai_%ls_%02u%02u%02u_%02u%02u%02u_%u",kind==2?L"smt":kind==3?L"asm":L"prt",now.wYear%100,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,++serial);
    MODEL_TRY("Read requested default template",ProConfigoptionGet(kind==2?L"template_sheetmetalpart":kind==3?L"template_designasm":L"template_solidpart",path));
    if(!path[0]){error=PRO_TK_E_NOT_FOUND;goto cleanup;}
    MODEL_TRY("Load requested template",ProMdlFiletypeLoad(path,filetype,PRO_B_FALSE,&source));
    MODEL_TRY("Read template model type",ProMdlTypeGet(source,&type));
    if(type!=(kind==3?PRO_MDL_ASSEMBLY:PRO_MDL_PART)){*stage="Configured template has wrong model type";error=PRO_TK_BAD_CONTEXT;goto cleanup;}
    MODEL_TRY("Read template subtype",ProMdlSubtypeGet(source,&subtype));
    if((kind==2 && subtype!=PROMDLSTYPE_PART_SHEETMETAL)||(kind==1 && subtype==PROMDLSTYPE_PART_SHEETMETAL)){
        *stage="Configured template has wrong solid or sheetmetal subtype";error=PRO_TK_BAD_CONTEXT;goto cleanup;
    }
    MODEL_TRY("Inspect original template features",ProSolidFeatVisit((ProSolid)source,template_feature,NULL,&before));
    if(kind!=3)MODEL_TRY("Check template has no solid geometry",template_empty((ProSolid)source));
    else if(before.components){*stage="Assembly template contains components";error=PRO_TK_BAD_CONTEXT;goto cleanup;}
    MODEL_TRY("Copy requested empty template",ProMdlnameCopy(source,name,&copy));
    MODEL_TRY("Verify copied model subtype",ProMdlSubtypeGet(copy,&subtype));
    if(kind==2 && subtype!=PROMDLSTYPE_PART_SHEETMETAL){error=PRO_TK_BAD_CONTEXT;goto cleanup;}
    MODEL_TRY("Inspect copied template features",ProSolidFeatVisit((ProSolid)copy,template_feature,NULL,&after));
    trace("Original template feature count",before.total);trace("Copied template feature count",after.total);
    trace("Copied datum planes",after.planes);trace("Copied coordinate systems",after.csys);
    if(before.total!=after.total||before.planes!=after.planes||before.csys!=after.csys){*stage="Template feature preservation mismatch";error=PRO_TK_GENERAL_ERROR;goto cleanup;}
    if(kind!=3)MODEL_TRY("Verify copy remains empty",template_empty((ProSolid)copy));
    if(!model_background_mode){
    MODEL_TRY("Open empty model window",ProObjectwindowMdlnameCreate(name,kind==3?PRO_ASSEMBLY:PRO_PART,&window));
    MODEL_TRY("Set empty model window",ProWindowCurrentSet(window));
    MODEL_TRY("Display empty model",ProMdlDisplay(copy));
    /* CurrentSet only changes Toolkit's graphics context. Activation is
       deferred until this command returns to Creo and updates model UI. */
    MODEL_TRY("Activate empty model window",ProWindowActivate(window));
    }
cleanup:return error;
}
static ProError model_remove_feature(ProSolid solid,int id){
    int *ids=NULL;ProFeatureDeleteOptions *opts=NULL;ProError error;
    error=ProArrayAlloc(1,sizeof(int),1,(ProArray *)&ids);if(error)return error;ids[0]=id;
    error=ProArrayAlloc(1,sizeof(*opts),1,(ProArray *)&opts);
    if(!error){opts[0]=PRO_FEAT_DELETE_NO_OPTS;error=ProFeatureWithoptionsDelete(solid,ids,opts,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);}
    if(ids)ProArrayFree((ProArray *)&ids);if(opts)ProArrayFree((ProArray *)&opts);return error;
}
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
static ProError model_create(const ModelPlan *p,ProMdlName name,const char **stage){
    return model_build_internal(p,name,stage,NULL,NULL,NULL);
}
#undef MODEL_TRY
#endif
