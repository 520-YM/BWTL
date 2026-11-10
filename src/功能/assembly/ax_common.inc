static int ax_recovery_uncertain;

typedef struct {
    ProAsmcomppath path;
    ProAsmcomp feature;
    ProMdl model;
} AxComponent;

static ProError ax_master(ProMdl assembly){
    ProSimprep rep;
    ProSimprepType type;
    ProBoolean dynamic;
    ProError error;
    if((error=ProSimprepActiveGet((ProSolid)assembly,&rep))||
       (error=ProSimprepTypeGet(&rep,&type))||
       (error=ProAssemblyDynPosGet((ProAssembly)assembly,&dynamic)))return error;
    return type==PRO_SIMPREP_MASTER_REP&&!dynamic?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_component_feature(ProMdl owner,int id,ProAsmcomp *feature){
    ProFeattype type;
    ProError error=ProFeatureInit((ProSolid)owner,id,feature);
    if(!error)error=ProFeatureTypeGet(feature,&type);
    if(error)return error;
    return type==PRO_FEAT_COMPONENT?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_component_resolve(ProMdl assembly,const wchar_t *token,AxComponent *result){
    ProIdTable ids={0};
    int count=ax_path_parse(token,ids);
    if(!count)return PRO_TK_BAD_INPUTS;
    ProMdl owner=assembly;
    ProError error;
    for(int i=0;i<count;i++){
        ProFeatStatus state;
        ProMdlType type;
        if((error=ProMdlTypeGet(owner,&type)))return error;
        if(type!=PRO_MDL_ASSEMBLY)return PRO_TK_BAD_CONTEXT;
        if((error=ax_master(owner))||
           (error=ax_component_feature(owner,ids[i],&result->feature))||
           (error=ProFeatureStatusGet(&result->feature,&state)))return error;
        if(state!=PRO_FEAT_ACTIVE)return PRO_TK_BAD_CONTEXT;
        if((error=ProAsmcompMdlGet(&result->feature,&owner)))return error;
    }
    result->model=owner;
    return ProAsmcomppathInit((ProSolid)assembly,ids,count,&result->path);
}

static int ax_path_label(const int *ids,int count,wchar_t *label,size_t capacity){
    int used=swprintf_s(label,capacity,L"ID:");
    if(used<0)return 0;
    for(int i=0;i<count;i++){
        int length=swprintf_s(label+used,capacity-used,L"%ls%d",i?L"/":L"",ids[i]);
        if(length<0)return 0;
        used+=length;
    }
    return 1;
}

static ProError ax_ordinary_direct(ProAsmcomp *feature){
    ProBoolean readonly,embedded,flexible,substitute;
    ProGroupStatus group;
    ProPatternStatus pattern;
    ProMdlType type;
    ProError error;
    if((error=ProFeatureIsReadonly(feature,&readonly))||
       (error=ProAsmcompIsEmbedded(feature,&embedded))||
       (error=ProAsmcompIsFlexible(feature,&flexible))||
       (error=ProAsmcompIsSubstitute(feature,&substitute))||
       (error=ProFeatureGroupStatusGet(feature,&group))||
       (error=ProFeaturePatternStatusGet(feature,&pattern))||
       (error=ProAsmcompMdldataGet(feature,&type,NULL,NULL)))return error;
    return !readonly&&!embedded&&!flexible&&!substitute&&group==PRO_GROUP_NONE&&
        pattern==PRO_PATTERN_NONE&&type==PRO_MDL_PART?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_leaf_without_children(ProAsmcomp *feature){
    int *children=NULL,count=0;
    ProError error=ProFeatureChildrenGet(feature,&children,&count);
    if(children)ProArrayFree((ProArray *)&children);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    return count?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
}

static ProError ax_constraint_count(ProAsmcomp *feature,int *count){
    ProAsmcompconstraint *constraints=NULL;
    *count=0;
    ProError error=ProAsmcompConstraintsWithComppathGet(feature,NULL,&constraints);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error&&constraints)error=ProArraySizeGet((ProArray)constraints,count);
    if(constraints)ProAsmcompconstraintArrayFree(constraints);
    return error;
}

static ProError ax_packaged(ProAsmcomp *feature){
    ProBoolean packaged;
    int count;
    ProError error=ProAsmcompIsPackaged(feature,&packaged);
    if(!error)error=ax_constraint_count(feature,&count);
    if(error)return error;
    return packaged&&!count?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static int ax_same_pose(ProMatrix a,ProMatrix b,double scale){
    for(int i=0;i<4;i++)for(int j=0;j<4;j++){
        double tolerance=i==3&&j<3?1e-5/scale:1e-7;
        if(!isfinite(a[i][j])||!isfinite(b[i][j])||fabs(a[i][j]-b[i][j])>tolerance)return 0;
    }
    return 1;
}

static ProError ax_actual_pose(ProMdl assembly,ProAsmcomp *feature,ProMatrix result){
    ProAsmcomppath path;
    ProIdTable ids={feature->id};
    ProError error=ProAsmcomppathInit((ProSolid)assembly,ids,1,&path);
    return error?error:ProAsmcomppathTrfGet(&path,PRO_B_TRUE,result);
}

static ProError ax_set_position(ProMdl assembly,ProAsmcomp *feature,ProMatrix position){
    ProError error=ProAsmcompPositionSet(NULL,feature,position);
    if(!error)error=ProAsmcompRegenerate(feature,PRO_B_FALSE);
    if(!error)error=part_regenerate(assembly);
    return error;
}

static void ax_uncertain(PartText *out){
    ax_recovery_uncertain=1;
    part_text_add(out,L"装配原状态恢复未能核实，本次会话已停止装配写入。\n");
}
