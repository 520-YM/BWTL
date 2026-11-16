typedef struct {
    PartText *out;
    ReplayPatternSource patterns;
    ProExtRefInfo *external;
    int external_groups,external_blocked;
    int top, hidden, embedded, members, derived, inactive, unsupported,failed,suppressed;
    int template_datums, annotations, pattern_headers, footer;
    int first_block_step,first_block_id;
    const wchar_t *first_block_operation;
    ProName first_block_name;
} ReplayPreview;

typedef struct {int active,frozen,missing,invalid,restricted;} ReplayPreviewRefs;

#include "imitation_tree_structure.inc"

static ReplayPreviewRefs replay_preview_refs(const ReplayPreview *scan,int feature){
    ReplayPreviewRefs result={0};
    for(int i=0;i<scan->external_groups;i++)for(int j=0;j<scan->external[i].n_refs;j++){
        ProExtFeatRef ref=scan->external[i].ext_refs[j];
        ProFeature owner;
        if(ProExtRefOwnFeatGet(ref,&owner)||owner.id!=feature)continue;
        ProRefState state;
        if(ProExtRefStateGet(ref,&state)){result.restricted++;continue;}
        switch(state){
        case PRO_REF_ACTIVE:result.active++;break;
        case PRO_REF_FROZEN:case PRO_REF_FROZEN_PLACE:result.frozen++;break;
        case PRO_REF_NOT_FOUND:case PRO_REF_MISSING:result.missing++;break;
        case PRO_REF_INVALID:result.invalid++;break;
        default:result.restricted++;break;
        }
    }
    return result;
}

static ProError replay_preview_feature(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ReplayPreview *scan=data;
    ProFeattype type;
    ProFeatStatus state;
    unsigned int state_flags=0;
    const wchar_t *issue=NULL;
    ProPatternStatus pattern;
    ProGrppatternStatus group_pattern;
    ProBoolean visible,embedded;
    ProName name=L"";
    ProError error=ProFeatureTypeGet(feature,&type);
    if(!error)error=replay_feature_state(feature,&state,&state_flags,&issue);
    if(!error)error=ProFeaturePatternStatusGet(feature,&pattern);
    if(!error)error=ProFeatureGrppatternStatusGet(feature,&group_pattern);
    if(!error)error=ProFeatureVisibilityGet(feature,&visible);
    if(!error)error=ProFeatureIsEmbedded(feature,&embedded);
    if(error)return error;
    if(embedded){
        scan->embedded++;
        if(scan->out)replay_preview_embedded(feature,scan->out);
        return PRO_TK_NO_ERROR;
    }
    ProModelitemNameGet((ProModelitem *)feature,name);
    if(state==PRO_FEAT_FAMTAB_SUPPRESSED){
        scan->suppressed++;
        if(scan->out)part_text_add(scan->out,L"  族表实例停用特征 %ls ID %d。\n",name,feature->id);
        return PRO_TK_NO_ERROR;
    }
    const int standard=replay_standard_feature(type,name);
    ProBoolean footer=PRO_B_FALSE;
    if(!ProFeatureIsInFooter(feature,&footer)&&footer)scan->footer++;
    if(standard||type==PRO_FEAT_ANNOTATION){
        if(standard)scan->template_datums++;
        else scan->annotations++;
        if(scan->out)part_text_add(scan->out,L"  %ls %ls%lsID %d%ls。\n",
            standard?L"模板基准":L"注释特征",name,name[0]?L" ":L"",feature->id,footer?L"，位于页脚":L"");
        return PRO_TK_NO_ERROR;
    }
    if(!visible){
        scan->hidden++;
        if(!issue)return PRO_TK_NO_ERROR;
    }
    if(pattern==PRO_PATTERN_MEMBER||group_pattern==PRO_GRP_PATTERN_MEMBER){scan->members++;return PRO_TK_NO_ERROR;}
    if(type==PRO_FEAT_DERIVED_MEMBER&&!issue){
        scan->derived++;
        return PRO_TK_NO_ERROR;
    }
    if(pattern==PRO_PATTERN_HEADER||group_pattern==PRO_GRP_PATTERN_HEADER){
        scan->pattern_headers++;
        return PRO_TK_NO_ERROR;
    }
    scan->top++;
    int supported=replay_type_supported(type);
    const wchar_t *operation=replay_operation_name(type);
    ReplayLocalGroup group={0};
    const int local_group=type==PRO_FEAT_GROUP_HEAD&&!issue&&!replay_local_group_read(feature,&group);
    if(type==PRO_FEAT_RM_SURF){
        int member;
        if(!replay_exclusion_member(feature,&member))operation=L"阵列实例几何排除";
        else if(!issue){
            ProElement tree=NULL;
            ProError read=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
            supported=!read&&replay_definition_supported(tree,type,&operation);
            if(tree)ProFeatureElemtreeFree(feature,tree);
        }
    }
    if(local_group)operation=group.patterned?L"本地特征组阵列":L"本地特征组";
    if(!issue&&!local_group&&supported&&(type==PRO_FEAT_GROUP_HEAD||type==PRO_FEAT_CURVE||type==PRO_FEAT_DATUM_SURF||type==PRO_FEAT_SRF_MDL||type==PRO_FEAT_DATUM_QUILT||type==PRO_FEAT_EXTEND||type==PRO_FEAT_BOOLEANBODIES||type==PRO_FEAT_SPLIT_BODY||type==PRO_FEAT_WALL||type==PRO_FEAT_HOLE||
                           type==PRO_FEAT_FIRST_FEAT||type==PRO_FEAT_PROTRUSION||type==PRO_FEAT_CUT||type==PRO_FEAT_CUT_SMT||
                           type==PRO_FEAT_ROUND||type==PRO_FEAT_DRAFT||type==PRO_FEAT_BEND||type==PRO_FEAT_UNBEND||type==PRO_FEAT_BEND_BACK||
                           type==PRO_FEAT_FLAT_PAT||type==PRO_FEAT_SMT_SKETCH_FORM||type==PRO_FEAT_DEFORM_AREA||type==PRO_FEAT_PUNCH_QUILT)){
        ProElement tree=NULL;
        ProError read=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
        supported=!read&&replay_definition_supported(tree,type,&operation);
        if(read==PRO_TK_INVALID_TYPE&&(type==PRO_FEAT_DATUM_SURF||type==PRO_FEAT_PROTRUSION)){
            ProFeature sections[2];
            supported=!replay_blend_sections(feature,sections,type==PRO_FEAT_DATUM_SURF);
            if(supported)operation=type==PRO_FEAT_DATUM_SURF?L"两边自由边界混合曲面":L"两截面平行实体混合";
        }
        if(!read&&supported&&type==PRO_FEAT_PROTRUSION){
            int separate=0;
            supported=!replay_extrusion_new_body(feature,tree,&separate);
            if(supported&&separate)operation=L"拉伸新建实体";
        }
        if(tree)ProFeatureElemtreeFree(feature,tree);
    }
    int pattern_type=0;
    if(!issue&&pattern==PRO_PATTERN_LEADER){
        ProElement tree=NULL;
        const ProPatternClass pattern_class=replay_pattern_class(type);
        ProError read=replay_pattern_definition_class(feature,pattern_class,&scan->patterns,&tree,&pattern_type);
        if(tree)ProElementFree(&tree);
        if(read||!(pattern_class==PRO_GROUP_PATTERN?local_group&&replay_group_pattern_supported(pattern_type):
                   replay_pattern_supported(pattern_type)))supported=0;
    }
    if(issue)scan->inactive++;
    if(state>=PRO_FEAT_FAMTAB_SUPPRESSED&&state<=PRO_FEAT_SUPPRESSED)scan->suppressed++;
    if(state_flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED))scan->failed++;
    if(!supported)scan->unsupported++;
    const ReplayPreviewRefs refs=replay_preview_refs(scan,feature->id);
    const int external_block=refs.frozen||refs.missing||refs.invalid||refs.restricted;
    if(external_block)scan->external_blocked++;
    if(!scan->first_block_step&&(issue||external_block||!supported)){
        scan->first_block_step=scan->top;
        scan->first_block_id=feature->id;
        scan->first_block_operation=issue?issue:external_block?L"外部参照":operation;
        wcscpy_s(scan->first_block_name,_countof(scan->first_block_name),name);
    }
    if(!scan->out||scan->top>REPLAY_FEATURE_MAX)return PRO_TK_NO_ERROR;
    int sections=0,parents=0,children=0,*ids=NULL,*next=NULL;
    ProFeatureNumSectionsGet(feature,&sections);
    ProError parents_error=ProFeatureParentsGet(feature,&ids,&parents);
    ProGroupStatus group_status;
    ProGroup owner;
    const int in_group=!ProFeatureGroupStatusGet(feature,&group_status)&&group_status==PRO_GROUP_MEMBER&&
        !ProFeatureGroupGet(feature,&owner)&&owner.id!=feature->id;
    part_text_add(scan->out,L"第 %d 步 · %ls%ls%ls，ID %d，类型 %d，截面 %d%ls",
        scan->top,operation,name[0]?L" ":L"",name,feature->id,type,sections,
        issue?L"，当前不可重建":external_block?L"，需核对外部参照":!supported?L"，待接入":L"，可尝试重建");
    if(issue)part_text_add(scan->out,L"，设计状态 %ls（%d，标志 0x%x）",issue,state,state_flags);
    if(refs.active||external_block)part_text_add(scan->out,L"，外部参照可用 %d、冻结 %d、缺失 %d、无效 %d、受限 %d",
        refs.active,refs.frozen,refs.missing,refs.invalid,refs.restricted);
    if(pattern==PRO_PATTERN_LEADER)part_text_add(scan->out,L"，阵列类型 %d",pattern_type);
    if(in_group)part_text_add(scan->out,L"，所属组 ID %d",owner.id);
    if(footer)part_text_add(scan->out,L"，位于页脚");
    if(local_group){
        part_text_add(scan->out,L"，组成员");
        for(int i=0;i<group.count&&i<8;i++)part_text_add(scan->out,L" %d",group.members[i]);
        if(group.count>8)part_text_add(scan->out,L" 等 %d 个",group.count);
    }
    if(!parents_error&&parents>0){
        part_text_add(scan->out,L"，父特征");
        for(int i=0;i<parents&&i<6;i++)part_text_add(scan->out,L" %d",ids[i]);
        if(parents>6)part_text_add(scan->out,L" 等 %d 个",parents);
    }
    if(!ProFeatureChildrenGet(feature,&next,&children)&&children>0){
        part_text_add(scan->out,L"，子特征");
        for(int i=0;i<children&&i<6;i++)part_text_add(scan->out,L" %d",next[i]);
        if(children>6)part_text_add(scan->out,L" 等 %d 个",children);
    }
    ProBoolean hidden;
    if(!ProModelitemIsHidden(feature,&hidden)&&hidden)
        part_text_add(scan->out,children>0?L"，已隐藏，仍有 %d 个下游子特征":L"，已隐藏",children);
    part_text_add(scan->out,L"。\n");
    replay_preview_structure(feature,type,pattern,sections,scan->out);
    if(ids)ProArrayFree((ProArray *)&ids);
    if(next)ProArrayFree((ProArray *)&next);
    return PRO_TK_NO_ERROR;
}

static void replay_preview(ProMdl source,PartText *out){
    ReplayPreview scan={0};
    scan.out=out;
    ProError refs=ProSolidExternParentsGet((ProSolid)source,PRO_ALL_EXT_REF_TYPES,&scan.external,&scan.external_groups);
    if(refs&&refs!=PRO_TK_E_NOT_FOUND){
        scan.external_groups=0;
        part_text_add(out,L"外部参照状态读取返回 Creo %d。\n",refs);
    }
    part_text_add(out,L"原特征树预检（读取定义，不创建零件）：\n");
    ProError error=ProSolidFeatVisit((ProSolid)source,replay_preview_feature,NULL,&scan);
    if(error)part_text_add(out,L"特征树读取返回 Creo %d。\n",error);
    ProError cleanup=replay_pattern_source_close(&scan.patterns);
    if(cleanup)part_text_add(out,L"临时阵列定义释放返回 Creo %d。\n",cleanup);
    if(scan.external)ProExtRefInfoFree(&scan.external);
    if(scan.top>REPLAY_FEATURE_MAX)part_text_add(out,L"已列出前 %d 步。\n",REPLAY_FEATURE_MAX);
    part_text_add(out,L"建模步骤 %d，隐藏特征 %d，内嵌特征 %d，阵列从属实例 %d，状态阻断 %d（抑制 %d、失败及其子特征 %d），待接入 %d。\n",
        scan.top,scan.hidden,scan.embedded,scan.members,scan.inactive,scan.suppressed,scan.failed,scan.unsupported);
    part_text_add(out,L"模型树另有模板基准 %d、注释特征 %d、阵列表头 %d，位于页脚的特征 %d。\n",
        scan.template_datums,scan.annotations,scan.pattern_headers,scan.footer);
    if(scan.external_blocked)part_text_add(out,L"外部参照阻断 %d 道建模步骤。\n",scan.external_blocked);
    if(scan.derived)part_text_add(out,L"另有 %d 项原生组内派生特征，由相应组操作生成。\n",scan.derived);
}
