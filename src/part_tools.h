#ifndef CREO_PART_TOOLS_H
#define CREO_PART_TOOLS_H
/* Developed against the installed PTC Creo 10 Pro/TOOLKIT headers.
   All entry points run on Creo's command thread. No MCP, UI automation or
   third-party repository source is used. */
#include <stdarg.h>
#include <ProDimension.h>
#include <ProAssembly.h>
#include <ProParameter.h>
#include <ProModelitem.h>
#include <ProEdge.h>
#include <ProIntf3Dexport.h>
#include "part_command.h"
#define PART_FEATURE_MAX 256
#define PART_DIM_MAX 256
#define PART_PARAM_MAX 64
#define PART_GEOM_MAX 64
#define PART_TEXT_MAX 28000
typedef struct { int id,type; unsigned flags; ProFeatStatus status; ProName name; } PartFeature;
typedef struct { ProDimension handle; ProName symbol; int feature; ProDimensiontype type; double value; ProBoolean related,negative; } PartDimension;
typedef struct { ProName name; ProParamvalue value; ProLockstatus lock; } PartParameter;
typedef struct { int id,type;double size; } PartGeometry;
typedef struct {
    ProMdl model,window_model; ProMdlName name; ProMdlType type; ProMdlsubtype subtype;
    ProWVerstamp stamp,parent_stamp;int in_assembly;ProPath directory;ProUnititem length_unit,mass_unit;
    double mm_per_native,volume,mass; int has_volume,has_mass,has_outline;
    Pro3dPnt outline[2];
    PartFeature feature[PART_FEATURE_MAX];int features;
    PartDimension dimension[PART_DIM_MAX];int dimensions;
    PartParameter parameter[PART_PARAM_MAX];int parameters;
    PartGeometry edge[PART_GEOM_MAX],surface[PART_GEOM_MAX];int edges,surfaces;
    int truncated,read_errors,unhealthy,empty_solid;
} PartSnapshot;
typedef struct { wchar_t *text;size_t size;int truncated; } PartText;

#include "功能/part_tools/part_text_add.inc"


#include "功能/part_tools/part_text_quote.inc"


#include "功能/part_tools/part_close.inc"


#include "功能/part_tools/part_snapshot_clear.inc"


#include "功能/part_tools/part_geometry_visit.inc"


#include "功能/part_tools/part_feature_visit.inc"


#include "功能/part_tools/part_dimension_visit.inc"


#include "功能/part_tools/part_parameter_visit.inc"


#include "功能/part_tools/part_target_get.inc"


#include "功能/part_tools/part_request_target_get.inc"


#include "功能/part_tools/part_snapshot_take_mode.inc"


#include "功能/part_tools/part_snapshot_take.inc"


#include "功能/part_tools/part_snapshot_take_basic.inc"


#include "功能/part_tools/part_param_format.inc"


#include "功能/part_tools/part_basic_material.inc"


#include "功能/part_tools/part_snapshot_format_basic.inc"


#include "功能/part_tools/part_snapshot_format_selected.inc"


#include "功能/part_tools/part_snapshot_format.inc"

/* Avoid appending entire geometry and parameter tables to every ordinary
   chat request. The native snapshot remains complete for validation. */

#include "功能/part_tools/part_context_sections.inc"


#include "功能/part_tools/part_check_binding.inc"


#include "功能/part_tools/part_health_visit.inc"


#include "功能/part_tools/part_health.inc"


#include "功能/part_tools/part_regenerate.inc"


#include "功能/part_tools/part_regenerate_context.inc"


#include "功能/part_tools/part_find_feature.inc"


#include "功能/part_tools/part_change_dimensions.inc"


#include "功能/part_tools/part_param_equal.inc"


#include "功能/part_tools/part_param_parse.inc"


#include "功能/part_tools/part_change_parameter.inc"

/* Refuse both session-name collisions and existing versioned disk files. */

#include "功能/part_tools/part_target_available.inc"


#include "功能/part_tools/part_toggle_feature.inc"


#include "功能/part_tools/part_extrude_current.inc"

#include "foundation_tools.h"

#include "功能/part_tools/part_execute.inc"

#endif
