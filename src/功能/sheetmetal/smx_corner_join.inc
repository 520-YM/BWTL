
#include <ProSmtCornerRelief.h>

static ProError smx_surface_distance(ProMdl model,const wchar_t *a,const wchar_t *b,double *distance){
    FdRef first={0},second={0};
    Pro2dPnt first_uv,second_uv;
    Pro3dPnt first_point,second_point;
    ProError error=fd_ref_resolve(model,a,PRO_SURFACE,&first);
    if(!error)error=fd_ref_resolve(model,b,PRO_SURFACE,&second);
    if(!error&&first.item.id==second.item.id)error=PRO_TK_BAD_INPUTS;
    if(!error)error=ProSelectionWithOptionsDistanceEval(first.selection,PRO_B_FALSE,second.selection,
        PRO_B_FALSE,first_uv,second_uv,first_point,second_point,distance);
    fd_ref_free(&first);
    fd_ref_free(&second);
    return error;
}

static ProError smx_flange_clearance(ProMdl model,const PartCommand *cmd,PartText *out){
    FdRef faces[2]={0};
    ProFeature owner[2];
    double mm,thickness,distance;
    ProError error=fab_thickness(model,&mm,&thickness);
    for(int i=0;!error&&i<2;i++){
        ProFeattype type;
        error=fd_ref_resolve(model,cmd->arg[2+i],PRO_SURFACE,&faces[i]);
        if(!error)error=ProGeomitemFeatureGet(&faces[i].item,&owner[i]);
        if(!error)error=ProFeatureTypeGet(&owner[i],&type);
        if(!error&&type!=PRO_FEAT_WALL)error=PRO_TK_BAD_CONTEXT;
    }
    if(!error&&owner[0].id==owner[1].id)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=smx_surface_distance(model,cmd->arg[2],cmd->arg[3],&distance);
    for(int i=0;i<2;i++)fd_ref_free(&faces[i]);
    if(!error){
        distance*=mm;
        double minimum=as_arg(cmd,4);
        part_text_add(out,L"指定两处翻边的实体修剪面最小距离 %.8g mm，下限 %.8g mm，%ls。\n",distance,minimum,
            distance<0.0001?L"面接触或相交，需核对实体干涉":distance<minimum?L"间距不足":L"所选两面间距符合");
    }
    return error;
}

static ProError smx_relief_create(ProMdl model,const PartCommand *cmd,int type,PartText *out,double *removed){
    FdTxn tx={0};
    FdRef refs[3]={0};
    ProElement all,array,set,reference_array,refset,definition,relief,width,depth,rotate,offset;
    ProMassProperty after;
    ProError error=sh_begin(&tx,model,cmd->arg[2],PRO_FEAT_SMT_CRN_REL);
#define SMX_CR(call) do{error=(call);if(error){trace(#call,error);goto done;}}while(0)
    if(error)goto done;
    if(!tx.has_volume||tx.volume<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
    for(int i=0;i<3;i++){
        ProSurface surface;
        ProSmtSurfType role;
        ProSrftype type;
        SMX_CR(fd_ref_resolve(model,cmd->arg[3+i],PRO_SURFACE,&refs[i]));
        SMX_CR(ProGeomitemToSurface(&refs[i].item,&surface));
        SMX_CR(ProSmtSurfaceTypeGet((ProPart)model,surface,&role));
        SMX_CR(ProSurfaceTypeGet(surface,&type));
        if(role!=PRO_SMT_SURF_FACE||type!=(i?PRO_SRF_CYL:PRO_SRF_PLANE)){error=PRO_TK_BAD_CONTEXT;goto done;}
    }
    if(refs[1].item.id==refs[2].item.id){error=PRO_TK_BAD_INPUTS;goto done;}
    SMX_CR(model_element(tx.root,PRO_E_CORNER_RELIEFS,0,0,&all));
    SMX_CR(model_element(all,PRO_E_CORNER_RELIEFS_CR_STATE,1,PRO_CRN_REL_CR_IN_FORMED,NULL));
    SMX_CR(model_element(all,PRO_E_CORNER_RELIEFS_ARR,0,0,&array));
    SMX_CR(model_element(array,PRO_E_CORNER_RELIEF_SET,0,0,&set));
    SMX_CR(model_element(set,PRO_E_CORNER_RELIEF_REF_ARR,0,0,&reference_array));
    SMX_CR(model_element(reference_array,PRO_E_CORNER_RELIEF_REF_SET,0,0,&refset));
    SMX_CR(model_element(refset,PRO_E_CORNER_RELIEF_REF_TYPE,1,PRO_CRN_REL_3_SURFACES,NULL));
    SMX_CR(fd_ref_element(&tx,refset,PRO_E_CORNER_RELIEF_REF_FLAT,&refs[0]));
    SMX_CR(fd_ref_element(&tx,refset,PRO_E_CORNER_RELIEF_REF_BND_1,&refs[1]));
    SMX_CR(fd_ref_element(&tx,refset,PRO_E_CORNER_RELIEF_REF_BND_2,&refs[2]));
    SMX_CR(model_element(set,PRO_E_CORNER_RELIEF_DEFINE,0,0,&definition));
    SMX_CR(model_element(definition,PRO_E_SMT_CORNER_RELIEF,0,0,&relief));
    SMX_CR(model_element(relief,PRO_E_SMT_CORNER_RELIEF_TYPE,1,type,NULL));
    SMX_CR(model_element(relief,PRO_E_SMT_CORNER_RELIEF_WIDTH,0,0,&width));
    SMX_CR(model_element(width,PRO_E_SMT_CORNER_RELIEF_WIDTH_TYPE,1,PRO_DIM_ENTER,NULL));
    SMX_CR(model_element(width,PRO_E_SMT_CORNER_RELIEF_WIDTH_VAL,2,as_arg(cmd,cmd->count-2)*tx.scale,NULL));
    SMX_CR(model_element(relief,PRO_E_SMT_CORNER_RELIEF_DEPTH,0,0,&depth));
    SMX_CR(model_element(depth,PRO_E_SMT_CORNER_RELIEF_DEPTH_TYPE,1,PRO_CRN_RLF_DEPTH_BLIND,NULL));
    SMX_CR(model_element(depth,PRO_E_SMT_CORNER_RELIEF_DEPTH_VAL,2,as_arg(cmd,cmd->count-1)*tx.scale,NULL));
    SMX_CR(model_element(relief,PRO_E_SMT_CORNER_RELIEF_ROTATE,0,0,&rotate));
    SMX_CR(model_element(rotate,PRO_E_SMT_CORNER_RELIEF_ROTATE_ADD,1,PRO_B_FALSE,NULL));
    SMX_CR(model_element(relief,PRO_E_SMT_CORNER_RELIEF_OFFSET,0,0,&offset));
    SMX_CR(model_element(offset,PRO_E_SMT_CORNER_RELIEF_OFFSET_ADD,1,PRO_B_FALSE,NULL));
    SMX_CR(model_element(definition,PRO_E_CORNER_RELIEFS_DIM_SCHEME,1,PRO_CRN_REL_DIM_REF_BEND_AXES_XSECTION,NULL));
    SMX_CR(model_element(definition,PRO_E_CORNER_RELIEF_ORIENT_REF,1,PRO_CRN_REL_BI_SECTOR,NULL));
    SMX_CR(fd_create(&tx,0));
    SMX_CR(smx_verify_changed_feature(&tx));
    SMX_CR(qa_geometry_mass(model,&after));
    *removed=(tx.volume-after.volume)/pow(tx.scale,3);
    if(*removed<1e-6){
        part_text_add(out,L"拼角避空重生成后没有产生可核对的去料。\n");
        error=PRO_TK_BAD_CONTEXT;
    }
done:
    for(int i=0;i<3;i++)fd_ref_free(&refs[i]);
    if(error&&tx.root){
        wchar_t rollback[768];
        am_finish(&tx,error,1,rollback,_countof(rollback));
        if(!tx.rollback_ok)sh_recovery_uncertain=1;
        part_text_add(out,L"%ls\n",rollback);
    }else fd_cleanup(&tx,0);
    return error;
#undef SMX_CR
}

static ProError smx_relief_apply(ProMdl model,const PartCommand *cmd,int kind,PartText *out){
    const int types[]={PRO_CORNER_RELIEF_RECTANGULAR,PRO_CORNER_RELIEF_CIRCULAR,PRO_CORNER_RELIEF_V_NOTCH};
    const wchar_t *shapes[]={L"RECT",L"ROUND",L"V"};
    if(kind==SMX_RELIEF){
        int type=0;
        for(int i=0;i<_countof(shapes);i++)if(!wcscmp(cmd->arg[6],shapes[i]))type=i;
        double removed;
        ProError error=smx_relief_create(model,cmd,types[type],out,&removed);
        if(!error)part_text_add(out,L"已创建原生 %ls 拼角避空，实体去料 %.8g mm³。未保存。\n",shapes[type],removed);
        return error;
    }
    DeliveryOutput scratch={0};
    ProError error=fab_scratch(&scratch);
    if(!error)error=dv_stage_enter(&scratch);
    int successful=0;
    for(int i=0;!error&&i<_countof(types);i++){
        ProMdl copy=NULL;
        error=smx_copy_scratch(model,&copy);
        if(error)break;
        double removed=0;
        ProError tried=smx_relief_create(copy,cmd,types[i],out,&removed);
        if(!tried){
            successful++;
            part_text_add(out,L"%ls 候选去料 %.8g mm³。\n",shapes[i],removed);
        }else part_text_add(out,L"%ls 候选未通过原生验收（Creo %d）。\n",shapes[i],tried);
        error=ProMdlErase(copy);
    }
    if(!error&&successful!=_countof(types))error=PRO_TK_BAD_CONTEXT;
    return fab_discard(&scratch,error,out);
}
