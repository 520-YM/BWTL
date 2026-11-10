#ifndef BWTL_FOUNDATION_PLAN_H
#define BWTL_FOUNDATION_PLAN_H
/* Protocol validation only. No Creo, network or desktop dependencies. */

#include "功能/foundation_plan/fd_name.inc"


#include "功能/foundation_plan/fd_reference_token.inc"


#include "功能/foundation_plan/fd_entity_token.inc"


#include "advanced_sketch_plan.h"
#include "功能/foundation_plan/fd_shape_valid.inc"

#include "solid_plan.h"
#include "advanced_solid_plan.h"
#include "machining_plan.h"
#include "refine_plan.h"
#include "advanced_datum_plan.h"
#include "surface_plan.h"
#include "advanced_machining_plan.h"
#include "structural_plan.h"
#include "sheetmetal_plan.h"
#include "reuse_plan.h"
#include "quality_plan.h"
#include "delivery_plan.h"
#include "assembly_plan.h"
#include "panel_plan.h"
#include "cabinet_plan.h"
#include "fastener_plan.h"
#include "shop_plan.h"
#include "vent_plan.h"
#include "revision_plan.h"
#include "inspection_plan.h"
#include "organize_plan.h"
#include "fabrication_plan.h"
#include "quick_parts_plan.h"
/* 0 unknown verb, 1 valid foundation command, -1 malformed command. */

#include "功能/foundation_plan/foundation_plan_valid.inc"

#endif
