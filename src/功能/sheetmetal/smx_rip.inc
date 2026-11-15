
#include <ProSmtEdgeRip.h>

static ProError smx_single_chain(ProElement element,FdRef *edge){
    ProCrvcollinstr instruction=NULL;
    ProCollection collection=NULL;
    ProReference reference=NULL;
    ProError error=ProSelectionToReference(edge->selection,&reference);
    if(!error)error=ProCrvcollinstrAlloc(PRO_CURVCOLL_ADD_ONE_INSTR,&instruction);
    if(!error)error=ProCrvcollinstrReferenceAdd(instruction,reference);
    if(!error)error=ProCrvcollectionAlloc(&collection);
    if(!error)error=ProCrvcollectionInstructionAdd(collection,instruction);
    if(!error)error=ProElementCollectionSet(element,collection);
    if(collection)ProCollectionFree(&collection);
    if(instruction)ProCrvcollinstrFree(instruction);
    if(reference)ProReferenceFree(reference);
    return error;
}

static ProError smx_rip_native(ProMdl model,const wchar_t *edge,double gap,int flip){
    FdTxn tx={0};
    FdRef selection={0};
    ProElement array,set,refs,ref,collector,detail,dimension;
    ProError error=sh_begin(&tx,model,L"BWTL_RIP_TRIAL",PRO_FEAT_RIP);
#define SMX_RIP_TRY(call) do{error=(call);if(error)goto done;}while(0)
    if(error)goto done;
    SMX_RIP_TRY(fd_ref_resolve(model,edge,PRO_EDGE,&selection));
    ProEdge native;
    ProSurface adjacent[2];
    SMX_RIP_TRY(ProGeomitemToEdge(&selection.item,&native));
    SMX_RIP_TRY(ProEdgeNeighborsGet(native,NULL,NULL,&adjacent[0],&adjacent[1]));
    for(int i=0;i<2;i++){
        ProSmtSurfType role;
        ProSrftype shape;
        SMX_RIP_TRY(ProSmtSurfaceTypeGet((ProPart)model,adjacent[i],&role));
        SMX_RIP_TRY(ProSurfaceTypeGet(adjacent[i],&shape));
        if(role!=PRO_SMT_SURF_FACE||shape!=PRO_SRF_PLANE){error=PRO_TK_BAD_CONTEXT;goto done;}
    }
    SMX_RIP_TRY(model_element(tx.root,PRO_E_SMT_RIP_TYPE,1,PRO_SMT_RIP_EDGE,NULL));
    SMX_RIP_TRY(model_element(tx.root,PRO_E_SMT_EDGE_RIP_ARRAY,0,0,&array));
    SMX_RIP_TRY(model_element(array,PRO_E_SMT_EDGE_RIP_SET,0,0,&set));
    SMX_RIP_TRY(model_element(set,PRO_E_SMT_EDGE_RIP_REFERENCES,0,0,&refs));
    SMX_RIP_TRY(model_element(refs,PRO_E_SMT_EDGE_REFERENCES,0,0,&ref));
    SMX_RIP_TRY(model_element(ref,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&collector));
    SMX_RIP_TRY(smx_single_chain(collector,&selection));
    SMX_RIP_TRY(model_element(set,PRO_E_SMT_EDGE_RIP,0,0,&detail));
    SMX_RIP_TRY(model_element(detail,PRO_E_SMT_EDGE_RIP_TYPE,1,
        gap?PRO_EDGE_RIP_MITER_CUT:PRO_EDGE_RIP_OPEN,NULL));
    SMX_RIP_TRY(model_element(detail,PRO_E_SMT_EDGE_RIP_FLIP,1,flip,NULL));
    SMX_RIP_TRY(model_element(detail,PRO_E_SMT_EDGE_RIP_ADD_GAP,1,gap>0,NULL));
    SMX_RIP_TRY(model_element(detail,PRO_E_SMT_EDGE_RIP_CLOSE_CORNER,1,PRO_B_FALSE,NULL));
    if(gap){
        SMX_RIP_TRY(model_element(detail,PRO_E_SMT_EDGE_RIP_DIM_1,0,0,&dimension));
        SMX_RIP_TRY(model_element(dimension,PRO_E_SMT_DIMENSION_TYPE,1,PRO_EDGE_RIP_DIM_TYPE_BLIND,NULL));
        SMX_RIP_TRY(model_element(dimension,PRO_E_SMT_DIMENSION_VALUE,2,gap*tx.scale,NULL));
    }
    SMX_RIP_TRY(fd_create(&tx,0));
    SMX_RIP_TRY(smx_verify_changed_feature(&tx));
done:
    fd_ref_free(&selection);
    fd_cleanup(&tx,error);
    return error;
#undef SMX_RIP_TRY
}

static ProError smx_rip_candidates(ProMdl model,const PartCommand *cmd,PartText *out){
    DeliveryOutput scratch={0};
    FabDxf *flat=calloc(1,sizeof(*flat));
    if(!flat)return PRO_TK_OUT_OF_MEMORY;
    ProError error=fab_scratch(&scratch);
    if(!error)error=dv_stage_enter(&scratch);
    int passed=0;
    for(int i=4;!error&&i<cmd->count;i++){
        for(int variant=0;!error&&variant<3;variant++){
            ProMdl copy=NULL;
            error=smx_copy_scratch(model,&copy);
            if(error)break;
            double requested=variant?as_arg(cmd,3):0;
            ProError attempt=smx_rip_native(copy,cmd->arg[i],requested,variant==2);
            if(!attempt){
                wchar_t name[40];
                swprintf_s(name,_countof(name),L"rip_%d_%d",i,variant);
                ProPath path;
                memset(flat,0,sizeof(*flat));
                attempt=fab_flat_read(copy,scratch.stage,name,cmd->arg[2],flat,path);
            }
            if(!attempt){
                part_text_add(out,L"%ls / %ls：展开轮廓已闭合，包络 %.8g × %.8g mm，%d 条折弯线，设置缝宽 %.8g mm。\n",
                    cmd->arg[i],variant==0?L"开缝":variant==1?L"斜接":L"反向斜接",
                    flat->high[0]-flat->low[0],flat->high[1]-flat->low[1],flat->bends,requested);
                passed++;
            }else part_text_add(out,L"%ls 候选 %d 的撕裂或展开未通过（Creo %d）。\n",cmd->arg[i],variant+1,attempt);
            error=ProMdlErase(copy);
        }
    }
    free(flat);
    if(!error&&!passed)error=PRO_TK_BAD_CONTEXT;
    if(!error)part_text_add(out,L"共 %d 个候选通过副本展开，原件保留。\n",passed);
    return fab_discard(&scratch,error,out);
}
