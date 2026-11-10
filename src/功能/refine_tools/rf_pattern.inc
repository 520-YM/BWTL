static ProError rf_pattern_tree(FdTxn *tx,const FdRef *reference,int circular,int count,double increment,int positive){
    ProElement branch=NULL,primary=NULL,direction=NULL,first=NULL;
    ProError e;
#define PAT_TREE(call) do{e=(call);if(e)return e;}while(0)
    PAT_TREE(ProElementFree(&tx->root));
    PAT_TREE(ProElementAlloc(PRO_E_PATTERN_ROOT,&tx->root));
    PAT_TREE(model_element(tx->root,PRO_E_GENPAT_TYPE,1,circular?PRO_GENPAT_AXIS_DRIVEN:PRO_GENPAT_DIR_DRIVEN,NULL));
    PAT_TREE(model_element(tx->root,PRO_E_GENPAT_REGEN_METHOD,1,PRO_PAT_GENERAL,NULL));
    PAT_TREE(model_element(tx->root,circular?PRO_E_GENPAT_AXIS:PRO_E_GENPAT_DIR,0,0,&branch));
    if(circular){
        PAT_TREE(fd_ref_element(tx,branch,PRO_E_GENPAT_AXIS_REF,reference));
        PAT_TREE(model_element(branch,PRO_E_GENPAT_AXIS1_INC,2,increment,NULL));
        PAT_TREE(model_element(branch,PRO_E_AXIS_PAT_DIR1_FLIP,1,positive?0:1,NULL));
    }else{
        PAT_TREE(model_element(branch,PRO_E_GENPAT_DIR1,0,0,&primary));
        PAT_TREE(model_element(primary,PRO_E_DIRECTION_COMPOUND,0,0,&direction));
        PAT_TREE(fd_ref_element(tx,direction,PRO_E_DIRECTION_REFERENCE,reference));
        PAT_TREE(model_element(direction,PRO_E_DIRECTION_FLIP,1,PRO_DIRECTION_FLIP_ALONG,NULL));
        PAT_TREE(model_element(branch,PRO_E_GENPAT_DIR1_INC,2,increment,NULL));
        PAT_TREE(model_element(branch,PRO_E_DIR_PAT_DIR1_FLIP,1,positive?0:1,NULL));
        PAT_TREE(model_element(branch,PRO_E_DIR_PAT_DIR1_OPT,1,-1,NULL));
    }
    PAT_TREE(model_element(branch,PRO_E_GENPAT_DIM_FIRST_DIR,0,0,&first));
    return model_element(branch,PRO_E_GENPAT_DIM_FIRST_DIR_NUM_INST,1,count,NULL);
#undef PAT_TREE
}

typedef struct {ProSolid model;const ProFeature *members;int count;unsigned found;} RfPatternGeometry;

static ProError rf_pattern_face(ProSurface surface,ProError status,ProAppData data){
    RfPatternGeometry *geometry=data;
    ProGeomitem item;
    ProFeature owner;
    ProError e;
    int i;
    if(status)return status;
    if((e=ProSurfaceToGeomitem(geometry->model,surface,&item))||(e=ProGeomitemFeatureGet(&item,&owner)))return e;
    for(i=0;i<geometry->count;i++)if(owner.id==geometry->members[i].id){geometry->found|=1u<<i;break;}
    return PRO_TK_NO_ERROR;
}

static ProError rf_pattern(FdTxn *tx,const PartCommand *cmd,int circular,double *volume){
    FdRef ref={0};
    ProFeature seed,leader,header,*members=NULL;
    ProFeattype type;
    ProPattern pattern;
    ProPatternStatus pattern_status;
    ProGroupStatus group_status;
    ProMassProperty mass;
    ProVector point,normal;
    ProName name;
    double increment;
    int count,actual,bodies,created=0,i,seed_count=0,positive=!wcscmp(cmd->arg[7],L"POS");
    ProError e;
#define PAT_STEP(label,call) do{tx->stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    PAT_STEP("resolve ungrouped native pattern seed",rf_active_feature(tx->model,cmd->arg[3],&seed,&type));
    if(type!=PRO_FEAT_HOLE&&type!=PRO_FEAT_CUT&&type!=PRO_FEAT_PROTRUSION){e=PRO_TK_UNSUPPORTED;goto done;}
    PAT_STEP("read seed pattern status",ProFeaturePatternStatusGet(&seed,&pattern_status));
    PAT_STEP("read seed group status",ProFeatureGroupStatusGet(&seed,&group_status));
    if(pattern_status!=PRO_PATTERN_NONE||group_status!=PRO_GROUP_NONE){e=PRO_TK_BAD_CONTEXT;goto done;}
    PAT_STEP("resolve native pattern direction",fd_ref_resolve(tx->model,cmd->arg[4],circular?PRO_AXIS:PRO_SURFACE,&ref));
    if(!circular){PAT_STEP("verify planar direction normal",fd_plane_data(&ref.item,point,normal));}
    part_integer(cmd->arg[5],&count);part_number(cmd->arg[6],&increment);
    PAT_STEP("build native direction or axis pattern",rf_pattern_tree(tx,&ref,circular,count,circular?increment:increment*tx->scale,positive));
    tx->stage="create native feature pattern";
    e=ProPatternCreate(&seed,PRO_FEAT_PATTERN,tx->root);
    trace(tx->stage,e);
    if(!ProFeaturePatternGet(&seed,PRO_FEAT_PATTERN,&pattern))created=1;
    if(e)goto done;
    if(!created){e=PRO_TK_GENERAL_ERROR;goto done;}
    PAT_STEP("read native pattern header",ProPatternHeaderGet(&pattern,&header));
    if(fd_was_feature(tx,header.id)){e=PRO_TK_GENERAL_ERROR;goto done;}
    PAT_STEP("name native pattern header",ProModelitemNameSet((ProModelitem *)&header,tx->name));
    PAT_STEP("regenerate native feature pattern",part_regenerate(tx->model));
    PAT_STEP("verify native pattern name",ProModelitemNameGet((ProModelitem *)&header,name));
    PAT_STEP("verify native pattern leader",ProPatternLeaderGet(&pattern,&leader));
    PAT_STEP("read native pattern members",ProPatternMembersGet(&pattern,&members));
    PAT_STEP("verify native pattern member count",ProArraySizeGet((ProArray)members,&actual));
    trace("pattern members including leader",actual);
    if(_wcsicmp(name,tx->name)||leader.id!=seed.id||actual!=count){e=PRO_TK_GENERAL_ERROR;goto done;}
    for(i=0;i<actual;i++){
        ProFeatStatus status;
        PAT_STEP("verify active native pattern member",ProFeatureStatusGet(&members[i],&status));
        if(status!=PRO_FEAT_ACTIVE||(members[i].id!=seed.id&&fd_was_feature(tx,members[i].id))){e=PRO_TK_GENERAL_ERROR;goto done;}
        if(members[i].id==seed.id)seed_count++;
    }
    if(seed_count!=1){e=PRO_TK_GENERAL_ERROR;goto done;}
    {
        RfPatternGeometry geometry={(ProSolid)tx->model,members,actual,0};
        PAT_STEP("inspect actual solid faces of each pattern member",ProSolidSurfaceVisit((ProSolid)tx->model,rf_pattern_face,NULL,&geometry));
        if(geometry.found!=(1u<<actual)-1){e=PRO_TK_BAD_CONTEXT;goto done;}
    }
    PAT_STEP("verify single patterned solid body",sd_body_count(tx->model,&bodies));
    PAT_STEP("verify patterned material volume",ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass));
    if(bodies!=1||!isfinite(mass.volume)||mass.volume<=0||part_close(mass.volume,tx->volume)||
       (type==PRO_FEAT_PROTRUSION?mass.volume<tx->volume:mass.volume>tx->volume)){e=PRO_TK_BAD_CONTEXT;goto done;}
    *volume=mass.volume;
    tx->feature=header;
done:
    if(e&&created&&(ProPatternDelete(&pattern)||part_regenerate(tx->model)))tx->rollback_ok=0;
    if(members)ProArrayFree((ProArray *)&members);
    fd_ref_free(&ref);
    return e;
#undef PAT_STEP
}
