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

#include "功能/native_model/template_surface.inc"


#include "功能/native_model/template_feature.inc"


#include "功能/native_model/template_empty.inc"


#include "功能/native_model/model_element.inc"


#include "功能/native_model/model_circle.inc"


#include "功能/native_model/model_section_origin.inc"


#include "功能/native_model/model_section_dimension.inc"


#include "功能/native_model/model_section_errors.inc"

#define SECTION_TRY(label, call) do { *stage = label; error = (call); trace(label, error); if (error != PRO_TK_NO_ERROR) goto done; } while (0)

#include "功能/native_model/model_section.inc"

#undef SECTION_TRY
#define MODEL_TRY(stage_name, call) do { *stage = stage_name; error = (call); trace(stage_name, error); if (error != PRO_TK_NO_ERROR) goto cleanup; } while (0)

#include "功能/native_model/model_create_empty.inc"


#include "功能/native_model/model_remove_feature.inc"


#include "功能/native_model/model_build_internal.inc"


#include "功能/native_model/model_create.inc"

#undef MODEL_TRY
#endif
