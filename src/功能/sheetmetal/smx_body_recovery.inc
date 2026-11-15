
/* Keep source bodies intact. Native conversion and removal run on a new,
   unsaved part and the result is checked against the source geometry. */
typedef struct { ProSolidBody body; ProOutline bounds; ProMassProperty mass; } SmxBodyState;

static ProError smx_body_state(ProSolidBody *body,SmxBodyState *state){
    state->body=*body;
    ProError error=ProSolidBodyOutlineGet(body,state->bounds);
    if(!error)error=ProSolidBodyMassPropertyGet(body,NULL,&state->mass);
    return error;
}

static int smx_same_body(const SmxBodyState *before,const SmxBodyState *after){
    for(int i=0;i<3;i++)for(int j=0;j<2;j++)
        if(!part_close(before->bounds[j][i],after->bounds[j][i]))return 0;
    return part_close(before->mass.volume,after->mass.volume)&&
        part_close(before->mass.surface_area,after->mass.surface_area);
}

static ProError smx_model_copy(ProMdl model,const wchar_t *name,ProMdl *copy){
    if(!part_target_available(model,name,L"prt"))return PRO_TK_E_FOUND;
    return ProMdlnameCopy(model,(wchar_t *)name,copy);
}

static ProError smx_rebuild_body(ProMdl model,const PartCommand *cmd,PartText *out){
    ProSolidBody body;
    SmxBodyState original,rebuilt;
    ProMdl copy=NULL;
    ProMdlsubtype subtype;
    ProError error=ProMdlSubtypeGet(model,&subtype);
    if(!error&&subtype==PROMDLSTYPE_PART_SHEETMETAL)error=PRO_TK_BAD_CONTEXT;
    if(!error){error=qa_single_body(model,&body);trace("SMX rebuild source body",error);}
    if(!error)error=ProSolidOutlineGet((ProSolid)model,original.bounds);
    if(!error)error=qa_geometry_mass(model,&original.mass);
    if(!error){error=smx_model_copy(model,cmd->arg[2],&copy);trace("SMX rebuild new copy",error);}
    if(!error)error=cm_native_command(copy,L"CREO_OP SMT_CONVERT %ls BWTL_REBUILD %ls %.12g",
        cmd->arg[2],cmd->arg[3],as_arg(cmd,4));
    if(!error)error=qa_single_body(copy,&body);
    if(!error)error=ProSolidOutlineGet((ProSolid)copy,rebuilt.bounds);
    if(!error)error=qa_geometry_mass(copy,&rebuilt.mass);
    if(!error&&!smx_same_body(&original,&rebuilt))error=PRO_TK_GENERAL_ERROR;
    if(!error){
        CabinetFace face;
        ProFeature feature;
        error=cm_feature_find(copy,L"BWTL_REBUILD",&feature);
        if(!error)error=cm_wall_face(&feature,&face);
        DeliveryOutput scratch={0};
        FabDxf *flat=calloc(1,sizeof(*flat));
        if(!flat&&!error)error=PRO_TK_OUT_OF_MEMORY;
        if(!error)error=fab_scratch(&scratch);
        if(!error)error=dv_stage_enter(&scratch);
        if(!error){
            ProPath path;
            wchar_t reference[32];
            swprintf_s(reference,_countof(reference),L"SURFACE:%d",face.id);
            error=fab_flat_read(copy,scratch.stage,L"converted",reference,flat,path);
            if(!error)part_text_add(out,L"转换副本原生展开：%d 个外轮廓图元、%d 个内轮廓图元、%d 条折弯线。\n",
                flat->outline,flat->holes,flat->bends);
        }
        free(flat);
        error=fab_discard(&scratch,error,out);
    }
    if(error&&copy&&ProMdlErase(copy)){
        sh_recovery_uncertain=1;
        part_text_add(out,L"本次副本需要检查，后续钣金写入已暂停。\n");
    }
    if(!error)part_text_add(out,L"已生成 %ls，外形、表面积、体积和原生展开已核对。副本保留在会话中，未保存。\n",cmd->arg[2]);
    return error;
}

static ProError smx_remove_other_bodies(ProMdl model,int keep,int *removed){
    ProSolidBody *bodies=NULL;
    ProReference *references=NULL;
    FdTxn tx={0};
    ProElement selection_element=NULL;
    int count=0,found=0;
    *removed=0;
    ProError error=sh_begin(&tx,model,L"BWTL_BODY_SEPARATION",PRO_FEAT_REMOVEBODY);
    if(!error)error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    if(!error)error=ProArrayAlloc(0,sizeof(ProReference),1,(ProArray *)&references);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        if(bodies[i].id==keep){found=1;continue;}
        ProBoolean construction=PRO_B_FALSE;
        error=ProSolidBodyIsConstruction(&bodies[i],&construction);
        if(error||construction)continue;
        if(tx.ref_count>=_countof(tx.refs)){error=PRO_TK_OUT_OF_RANGE;break;}
        ProSelection selection=NULL;
        error=ProSelectionAlloc(NULL,(ProModelitem *)&bodies[i],&selection);
        if(!error)error=ProSelectionToReference(selection,&tx.refs[tx.ref_count]);
        if(selection)ProSelectionFree(&selection);
        if(!error)error=ProArrayObjectAdd((ProArray *)&references,PRO_VALUE_UNUSED,1,&tx.refs[tx.ref_count++]);
        if(!error)(*removed)++;
    }
    if(!error&&(!found||!*removed))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=model_element(tx.root,PRO_E_BODY_SELECT,0,0,&selection_element);
    if(!error)error=ProElementReferencesSet(selection_element,references);
    if(!error)error=fd_create(&tx,0);
    if(!error)error=smx_verify_changed_feature(&tx);
    if(references)ProArrayFree((ProArray *)&references);
    if(bodies)ProArrayFree((ProArray *)&bodies);
    fd_cleanup(&tx,error);
    return error;
}

static ProError smx_split_body(ProMdl model,const PartCommand *cmd,PartText *out){
    ProSolidBody *bodies=NULL;
    SmxBodyState original,retained;
    ProMdl copy=NULL;
    int count=0,keep=0,found=0,removed=0;
    if(!smx_body_token(cmd->arg[2],&keep))return PRO_TK_BAD_INPUTS;
    ProError error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    if(!error&&count<2){
        part_text_add(out,L"Creo 只返回一个主体。分离的钣金壁可能共用该主体；此功能需要多个原生主体。\n");
        error=PRO_TK_BAD_CONTEXT;
    }
    for(int i=0;!error&&i<count;i++){
        if(bodies[i].id!=keep)continue;
        ProSolidBodyState state;
        ProBoolean construction;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(!error)error=ProSolidBodyIsConstruction(&bodies[i],&construction);
        if(!error&&state==PRO_BODY_STATE_ACTIVE&&!construction){found=1;error=smx_body_state(&bodies[i],&original);}
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    if(!error&&!found)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=smx_model_copy(model,cmd->arg[3],&copy);
    if(!error)error=smx_remove_other_bodies(copy,keep,&removed);
    if(!error){
        ProSolidBody body;
        error=qa_single_body(copy,&body);
        if(!error)error=smx_body_state(&body,&retained);
        if(!error&&!smx_same_body(&original,&retained))error=PRO_TK_GENERAL_ERROR;
    }
    if(error&&copy&&ProMdlErase(copy))sh_recovery_uncertain=1;
    if(!error)part_text_add(out,L"已在会话中分离 BODY:%d 到 %ls，副本移除了 %d 个其他主体；目标体积、表面积和包络已核对，未保存。\n",
        keep,cmd->arg[3],removed);
    return error;
}
