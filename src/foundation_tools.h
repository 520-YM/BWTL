#ifndef BWTL_FOUNDATION_TOOLS_H
#define BWTL_FOUNDATION_TOOLS_H
#include <ProDtmPln.h>
#include <ProDtmAxis.h>
#include <ProDtmPnt.h>
#include <ProDtmCsys.h>
#include <ProDtmCrv.h>
#include <ProAxis.h>
#include <ProPoint.h>
#include <ProCsys.h>
#include <ProCurve.h>
#include <ProGeomitemdata.h>
#include <ProSecConstr.h>

typedef struct { ProGeomitem item;int owner_feature;ProSelection selection; } FdRef;
typedef struct {
    ProMdl model;ProElement root;ProSelection model_selection;
    ProReference refs[32];int ref_count,parents[32],parent_count;
    ProFeature feature;int created,before_ids[512],before_count;
    ProFeatureCreateOptions *options;ProErrorlist errors;
    double scale,volume;int has_volume,was_empty;ProName name;
    const char *stage;int rollback_ok;
} FdTxn;
typedef struct {ProGeomitem item;int count;} FdCollect;
typedef struct {ProMdl model;ProName name;wchar_t plane[256],orient[256];ProWVerstamp stamp;} FdDraft;
static FdDraft fd_draft={0};
static ProMdl fd_selected_model=NULL,fd_last_sketch_model=NULL;
static ProModelitem fd_selected_item={0};static ProWVerstamp fd_selected_stamp=NULL;
static ProName fd_last_sketch=L"";

#include "功能/foundation_tools/foundation_reset.inc"


#include "功能/foundation_tools/fd_collect_geom.inc"


#include "功能/foundation_tools/fd_feature_ids.inc"


#include "功能/foundation_tools/fd_was_feature.inc"


#include "功能/foundation_tools/fd_feature_geom.inc"


#include "功能/foundation_tools/fd_plane_data.inc"


#include "功能/foundation_tools/fd_ref_resolve.inc"


#include "功能/foundation_tools/fd_ref_free.inc"


#include "功能/foundation_tools/fd_begin.inc"


#include "功能/foundation_tools/fd_ref_element.inc"


#include "功能/foundation_tools/fd_create.inc"


#include "功能/foundation_tools/fd_verify.inc"


#include "功能/foundation_tools/fd_cleanup.inc"

#define FD_TRY(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)

#include "功能/foundation_tools/fd_create_datum.inc"

#undef FD_TRY

typedef struct {
    ProFeature feature;ProElement tree,backup;ProSection section,original;
    ProFeatureCreateOptions *options;int editing;
} FdSketch;

#include "功能/foundation_tools/fd_section_from_tree.inc"


#include "功能/foundation_tools/fd_sketch_open.inc"


#include "功能/foundation_tools/fd_sketch_free.inc"


#include "功能/foundation_tools/fd_entity_reference.inc"


#include "功能/foundation_tools/fd_same_refs.inc"


#include "功能/foundation_tools/fd_constraint_evidence.inc"


#include "功能/foundation_tools/fd_constraint_create.inc"


#include "功能/foundation_tools/fd_dimension_create.inc"


#include "功能/foundation_tools/fd_shape_append.inc"


#include "功能/foundation_tools/fd_section_solve.inc"


#include "功能/foundation_tools/fd_sketch_setup.inc"


#include "功能/foundation_tools/fd_sketch_create.inc"


#include "功能/foundation_tools/fd_sketch_info.inc"

typedef struct {int id;ProSecdimType type;double value;} FdDimValue;

#include "功能/foundation_tools/fd_dimensions_capture.inc"


#include "功能/foundation_tools/fd_sketch_edit.inc"

typedef struct {ProMdl model;PartText *out;int count;} FdRefList;

#include "功能/foundation_tools/fd_reference_item.inc"


#include "功能/foundation_tools/fd_reference_feature.inc"


#include "功能/foundation_tools/foundation_references.inc"

#include "solid_tools.h"
#include "machining_tools.h"

#include "功能/foundation_tools/foundation_execute.inc"

#endif
