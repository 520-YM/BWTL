/* The stock flat wall's projected entities carry constraints owned by
   its attachment edge. Generate them in the target and then apply the
   measured source dimensions to the resulting native section. */
static ProError replay_stock_wall_height(ProSection section,double *height)
{
    int *ids=NULL,count=0;
    ProError error=ProSecdimIdsGet(section,&ids,&count);
    *height=0;
    for(int i=0;!error&&i<count;i++){
        ProSecdimType kind;
        double value;
        error=ProSecdimTypeGet(section,ids[i],&kind);
        if(!error&&kind==PRO_TK_DIM_LINE){
            error=ProSecdimValueGet(section,ids[i],&value);
            if(!error&&value>*height)*height=value;
        }
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    return error?error:*height>0?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static int replay_stock_wall_profile(ProSection source,double *start,double *end,double *height)
{
    int *ids=NULL,count=0,lines=0;
    ProError error=ProSectionEntityIdsGet(source,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        ProBoolean projected,construction;
        Pro2dEntdef *entity=NULL;
        error=ProSectionEntityIsProjection(source,ids[i],&projected);
        if(!error)error=ProSectionEntityIsConstruction(source,ids[i],&construction);
        if(!error&&!projected&&!construction){
            error=ProSectionEntityGet(source,ids[i],&entity);
            if(!error&&(!entity||entity->type!=PRO_2D_LINE))error=PRO_TK_UNSUPPORTED;
            lines++;
        }
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    return !error&&lines==3&&!cm_wall_offsets(source,start,end)&&
        !replay_stock_wall_height(source,height);
}

static ProError replay_stock_wall_edge(ImitationReplay *replay,ProElement tree,ProModelitem *edge)
{
    ProElement element;
    ProReference *references=NULL;
    int count=0;
    ProError error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_FLAT_WALL_ATT_EDGE},1,&element);
    if(!error)error=ProElementReferencesGet(element,NULL,&references);
    if(!error)error=ProArraySizeGet((ProArray)references,&count);
    if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=replay_reference_item(replay,references[0],edge,0);
    if(references)ProReferencearrayFree(references);
    return error;
}

static ProError replay_stock_wall(ImitationReplay *replay,ProFeature *source,ProElement tree,
    const wchar_t *name,double start,double end,double height,ProFeature *created)
{
    if(start>1e-6||end>1e-6)return PRO_TK_UNSUPPORTED;
    ProModelitem edge={0};
    ProElement a,r,position,angle;
    double bend,radius,unit;
    int mode,flip;
    ProError error=cm_bend_elements(source,tree,&a,&r);
    if(!error)error=replay_stock_wall_edge(replay,tree,&edge);
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_WALL_HEIGHT},1,&position);
    if(!error)error=replay_tree_integer(position,PRO_E_SMT_WALL_HEIGHT_TYPE,&mode);
    if(!error&&mode!=PRO_BEND_POSITION_PROF_ON_EDGE)error=PRO_TK_UNSUPPORTED;
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_FLAT_WALL_ANGLE},1,&angle);
    if(!error)error=replay_tree_integer(angle,PRO_E_SMT_FLAT_WALL_ANGLE_FLIP,&flip);
    if(!error)error=ProElementDoubleGet(a,NULL,&bend);
    if(!error)error=ProElementDoubleGet(r,NULL,&radius);
    if(!error)error=qa_mm_per_unit(replay->target,&unit);
    CabinetEdge attachment;
    wchar_t token[32];
    swprintf_s(token,_countof(token),L"EDGE:%d",edge.id);
    if(!error)error=cm_edge_get(replay->target,token,&attachment);
    if(error)return error;
    FdTxn wall={0};
    wall.model=replay->target;
    wall.scale=1/unit;
    error=cm_attach(&wall,name,&attachment,-start,attachment.length+end,height*unit,bend,radius*unit,
        flip?L"NEG":L"POS",created);
    if(!error)error=cm_bend_verified(created,bend,radius*unit,wall.scale);
    if(!error)part_text_add(replay->out,L"平整壁 %ls：高度 %.6g mm，两端退距 %.6g / %.6g mm，折弯 %.6g° / R%.6g。\n",
        name,height*unit,fabs(start*unit),fabs(end*unit),bend,radius*unit);
    return error;
}
/* Circular wall profiles are native one-line sweeps around a complete hole.
   Use the same ring and editable stock section checks as the cabinet tool. */
static ProError replay_ring_wall(ImitationReplay *replay,ProElement tree,ProSection section,
    const wchar_t *name,ProFeature *created)
{
    int form,flip,thickness,normal;
    ProElement element,fillet;
    double height,radius,unit;
    ProError error=replay_tree_integer(tree,PRO_E_SMT_FLANGE_TYPE,&form);
    if(!error&&form!=PRO_FLANGE_WALL_TYPE_2D_SWEPT)return PRO_TK_UNSUPPORTED;
    if(!error)error=replay_tree_integer(tree,PRO_E_SMT_FLANGE_TRAJ_CRV_NORM,&normal);
    if(!error&&normal!=PRO_FLANGE_TRAJ_NORM_NO_FLIP)return PRO_TK_UNSUPPORTED;
    if(!error)error=replay_tree_integer(tree,PRO_E_SMT_WALL_THICKNESS_FLIP,&thickness);
    if(!error&&thickness)return PRO_TK_UNSUPPORTED;
    if(!error)error=replay_tree_integer(tree,PRO_E_SMT_FLANGE_SEC_FLIP,&flip);
    if(!error)error=replay_stock_wall_height(section,&height);
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_FILLETS},1,&fillet);
    int side;
    if(!error)error=replay_tree_integer(fillet,PRO_E_SMT_FILLETS_SIDE,&side);
    if(!error&&side!=PRO_BEND_RAD_INSIDE)return PRO_TK_UNSUPPORTED;
    if(!error)error=mc_tree_element(fillet,(ProElemId[]){PRO_E_SMT_FILLETS_VALUE},1,&element);
    if(!error)error=ProElementDoubleGet(element,NULL,&radius);
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_STD_CURVE_COLLECTION_APPL},1,&element);
    ProCollection collection=NULL;
    ProCrvcollinstr *list=NULL;
    ProReference *refs=NULL;
    int count=0;
    ProModelitem mapped={0};
    ProEdge edge;
    ProEnttype kind;
    if(!error)error=ProElementCollectionGet(element,NULL,&collection);
    if(!error)error=ProCrvcollectionInstructionsGet(collection,&list);
    if(!error)error=ProArraySizeGet((ProArray)list,&count);
    if(!error&&!count)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProCrvcollinstrReferencesGet(list[0],&refs);
    if(!error)error=ProArraySizeGet((ProArray)refs,&count);
    if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=replay_reference_item(replay,refs[0],&mapped,0);
    if(!error&&mapped.type!=PRO_EDGE)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProGeomitemToEdge(&mapped,&edge);
    if(!error)error=ProEdgeTypeGet(edge,&kind);
    if(!error&&kind!=PRO_ENT_CIRCLE&&kind!=PRO_ENT_ARC)error=PRO_TK_UNSUPPORTED;
    if(refs)ProReferencearrayFree(refs);
    if(list)ProCrvcollinstrArrayFree(list);
    if(collection)ProCollectionFree(&collection);
    if(error)return error;
    ProMdlName model;
    error=ProMdlMdlnameGet(replay->target,model);
    if(!error)error=qa_mm_per_unit(replay->target,&unit);
    if(error)return error;
    PartCommand cmd;
    wchar_t protocol[512],receipt[512]=L"";
    swprintf_s(protocol,_countof(protocol),L"CREO_OP CASE_COLLAR %ls %ls EDGE:%d %.12g %.12g %ls",
        model,name,mapped.id,height*unit,radius*unit,flip?L"NEG":L"POS");
    if(part_command_parse(protocol,&cmd)!=1)return PRO_TK_BAD_INPUTS;
    FdTxn tx={0};
    PartText info={receipt,_countof(receipt),0};
    error=sh_begin(&tx,replay->target,name,PRO_FEAT_WALL);
    if(!error)error=cm_collar(&tx,&cmd,&info);
    fd_cleanup(&tx,0);
    if(!error)error=cm_feature_find(replay->target,name,created);
    if(!error)part_text_add(replay->out,L"圆形凸缘 %ls：%ls\n",name,receipt);
    return error;
}
