#ifndef BWTL_REFINE_TOOLS_H
#define BWTL_REFINE_TOOLS_H
#include <ProDraft.h>
#include <ProRib.h>
#include <ProPattern.h>
#include <ProFlexMirror.h>
#include <ProFlxmdlOpts.h>
#include <ProSrfcollection.h>
#include <ProRefInfo.h>

static ProError rf_active_feature(ProMdl model,const wchar_t *name,ProFeature *feature,ProFeattype *type){
    ProFeatStatus status;
    ProError e;
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,(ProModelitem *)feature))||
       (e=ProFeatureStatusGet(feature,&status))||(e=ProFeatureTypeGet(feature,type)))return e;
    return status==PRO_FEAT_ACTIVE?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError rf_surface_add(FdTxn *tx,ProCollection collection,const FdRef *face){
    ProSrfcollinstr instruction=NULL;
    ProSrfcollref surface_reference=NULL;
    ProReference reference=NULL;
    ProError e;
    int i;
    if(tx->ref_count>=_countof(tx->refs))return PRO_TK_BAD_INPUTS;
    if((e=ProSelectionToReference(face->selection,&reference)))return e;
    tx->refs[tx->ref_count++]=reference;
    if(!(e=ProSrfcollinstrAlloc(PRO_SURFCOLL_SINGLE_SURF,PRO_B_TRUE,&instruction))&&
       !(e=ProSrfcollrefAlloc(PRO_SURFCOLL_REF_SINGLE,reference,&surface_reference))&&
       !(e=ProSrfcollinstrReferenceAdd(instruction,surface_reference))){
        /* The parent owns each successfully attached surface collection handle. */
        surface_reference=NULL;
        e=ProSrfcollectionInstructionAdd(collection,instruction);
        if(!e)instruction=NULL;
    }
    if(surface_reference)ProSrfcollrefFree(surface_reference);
    if(instruction)ProSrfcollinstrFree(instruction);
    if(e)return e;
    for(i=0;i<tx->parent_count;i++)if(tx->parents[i]==face->owner_feature)return PRO_TK_NO_ERROR;
    tx->parents[tx->parent_count++]=face->owner_feature;
    return PRO_TK_NO_ERROR;
}

#include "功能/refine_tools/rf_draft.inc"
#include "功能/refine_tools/rf_rib.inc"
#include "功能/refine_tools/rf_mirror.inc"
#include "功能/refine_tools/rf_pattern.inc"
#include "功能/refine_tools/rf_delete.inc"
#include "功能/refine_tools/rf_execute.inc"
#endif
