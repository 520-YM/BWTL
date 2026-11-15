typedef struct {
    PartText *out;
    ProFeature feature;
    ProName name;
    int count,unread;
} ImitationDefinitionReport;

static const wchar_t *imitation_reference_type(ProType type)
{
    switch(type){
    case PRO_SURFACE:return L"面";
    case PRO_EDGE:return L"边";
    case PRO_AXIS:return L"轴";
    case PRO_POINT:return L"点";
    case PRO_CSYS:return L"坐标系";
    case PRO_CURVE:return L"曲线";
    case PRO_DIMENSION:return L"驱动尺寸";
    case PRO_REF_DIMENSION:return L"参考尺寸";
    case PRO_FEATURE:return L"特征";
    case PRO_BODY:return L"实体";
    default:return L"项目";
    }
}

static const wchar_t *imitation_reference_state(ProRefState state)
{
    switch(state){
    case PRO_REF_ACTIVE:return L"可用";
    case PRO_REF_FROZEN:return L"冻结";
    case PRO_REF_MISSING:case PRO_REF_NOT_FOUND:return L"缺失";
    case PRO_REF_INVALID:return L"无效";
    default:return L"受限";
    }
}

static void imitation_definition_path(PartText *out,ProElempath path,ProElemId leaf)
{
    ProElempathItem *items=NULL;
    int count=0;
    if(path&&!ProArrayAlloc(0,sizeof(*items),8,(ProArray *)&items)&&
       !ProElempathDataGet(path,&items))
        ProArraySizeGet((ProArray)items,&count);
    if(!count)part_text_add(out,L"%d",leaf);
    for(int i=0;i<count;i++){
        if(items[i].type==PRO_ELEM_PATH_ITEM_TYPE_INDEX)
            part_text_add(out,L"[%d]",items[i].path_item.elem_index);
        else part_text_add(out,L"%ls%d",i?L"/":L"",items[i].path_item.elem_id);
    }
    if(items)ProArrayFree((ProArray *)&items);
}

static void imitation_definition_item(ImitationDefinitionReport *scan,ProElemId leaf,
                                      ProElempath path,ProReference reference)
{
    ProType type;
    int id=0;
    ProRefState state;
    if(!reference||ProReferenceTypeGet(reference,&type))return;
    if(ProReferenceIdGet(reference,&id)||id<=0)ProReferenceOriginalidGet(reference,&id);
    if(id<=0)return;
    const ProError state_error=ProReferenceStatusGet(reference,&state);
    ProFamilyMdlName model=L"";
    if(ProReferenceOwnerMdlnameGet(reference,model)||!model[0])
        ProReferenceOriginalownerMdlnameGet(reference,model);
    ProMdl owner=NULL;
    ProModelitem item;
    ProFeature parent={0};
    if(!state_error&&state==PRO_REF_ACTIVE&&!ProReferenceOwnerGet(reference,&owner)&&owner&&
       !ProModelitemInit(owner,id,type,&item)){
        if(type==PRO_FEATURE)parent=item;
        else if(type==PRO_DIMENSION||type==PRO_REF_DIMENSION)
            ProDimensionOwnerfeatureGet(&item,&parent);
        else if(type==PRO_SURFACE||type==PRO_EDGE||type==PRO_AXIS||type==PRO_POINT||
                type==PRO_CSYS||type==PRO_CURVE)
            ProGeomitemFeatureGet(&item,&parent);
    }
    scan->count++;
    if(scan->count>100)return;
    part_text_add(scan->out,L"%ls%ls特征 %d，定义 ",scan->name,scan->name[0]?L" · ":L"",scan->feature.id);
    imitation_definition_path(scan->out,path,leaf);
    part_text_add(scan->out,L" → %ls %ls %d（类型 %d，%ls）",
        model[0]?model:L"模型名未返回",imitation_reference_type(type),id,type,
        state_error?L"状态未返回":imitation_reference_state(state));
    if(parent.id>0){
        ProName name=L"";
        ProModelitemNameGet(&parent,name);
        part_text_add(scan->out,L"，上游 %ls%lsID %d",name,name[0]?L" ":L"",parent.id);
    }
    part_text_add(scan->out,L"。\n");
}

static void imitation_definition_array(ImitationDefinitionReport *scan,ProElemId leaf,
                                       ProElempath path,ProReference *references)
{
    int count=0;
    if(references&&!ProArraySizeGet((ProArray)references,&count))
        for(int i=0;i<count;i++)imitation_definition_item(scan,leaf,path,references[i]);
    if(references)ProReferencearrayFree(references);
}

static ProError imitation_definition_element(ProElement tree,ProElement element,ProElempath path,ProAppData data)
{
    ImitationDefinitionReport *scan=data;
    ProElemId leaf;
    ProBoolean many=PRO_B_FALSE;
    if(ProElementIdGet(element,&leaf))return PRO_TK_NO_ERROR;
    if(leaf==PRO_E_STD_CURVE_COLLECTION_APPL){
        ProCollection collection=NULL;
        ProCrvcollinstr *instructions=NULL;
        int count=0;
        if(!ProElementCollectionGet(element,NULL,&collection)&&
           !ProCrvcollectionInstructionsGet(collection,&instructions)&&instructions&&
           !ProArraySizeGet((ProArray)instructions,&count)){
            for(int i=0;i<count;i++){
                ProReference *references=NULL;
                if(!ProCrvcollinstrReferencesGet(instructions[i],&references))
                    imitation_definition_array(scan,leaf,path,references);
            }
        }
        if(instructions)ProCrvcollinstrArrayFree(instructions);
        if(collection)ProCollectionFree(&collection);
    }else if((!ProElementIsMultival(element,NULL,&many)&&many)||leaf==PRO_E_SMT_FLAT_WALL_ATT_EDGE){
        ProReference *references=NULL;
        if(!ProElementReferencesGet(element,NULL,&references))
            imitation_definition_array(scan,leaf,path,references);
    }else{
        ProValueDataType type;
        ProReference reference=NULL;
        if(!ProElementValuetypeGet(element,&type)&&type==PRO_VALUE_TYPE_SELECTION&&
           !ProElementReferenceGet(element,NULL,&reference)){
            imitation_definition_item(scan,leaf,path,reference);
            if(reference)ProReferenceFree(reference);
        }
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_definition_feature(ProFeature *feature,ProError status,ProAppData data)
{
    ImitationDefinitionReport *scan=data;
    if(status)return status;
    ProPatternStatus pattern;
    if(!ProFeaturePatternStatusGet(feature,&pattern)&&
       (pattern==PRO_PATTERN_HEADER||pattern==PRO_PATTERN_MEMBER))return PRO_TK_NO_ERROR;
    ProFeattype type;
    if(!ProFeatureTypeGet(feature,&type)&&(type==PRO_FEAT_ANNOTATION||type==PRO_FEAT_ZONE))
        return PRO_TK_NO_ERROR;
    ProElement tree=NULL;
    ProError error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error){
        scan->feature=*feature;
        scan->name[0]=0;
        ProModelitemNameGet(feature,scan->name);
        error=ProElemtreeElementVisit(tree,NULL,NULL,imitation_definition_element,scan);
    }
    if(tree)ProFeatureElemtreeFree(feature,tree);
    if(error&&error!=PRO_TK_E_NOT_FOUND)scan->unread++;
    return PRO_TK_NO_ERROR;
}

static void imitation_definition_reference_report(ProMdl model,PartText *out)
{
    ImitationDefinitionReport scan={0};
    scan.out=out;
    part_text_add(out,L"源特征定义参照\n");
    ProError error=ProSolidFeatVisit((ProSolid)model,imitation_definition_feature,NULL,&scan);
    if(error&&error!=PRO_TK_E_NOT_FOUND)part_text_add(out,L"读取特征返回 Creo %d。\n",error);
    part_text_add(out,L"定义参照 %d 条，已显示 %d 条，%d 项特征的定义无法读取。\n",
        scan.count,scan.count>100?100:scan.count,scan.unread);
}
