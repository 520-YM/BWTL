#include <ProLayer.h>

static void replay_preview_section(ProFeature *feature,int index,PartText *out)
{
    ProSection section=NULL;
    int *ids=NULL,count=0,construction_count=0,projection_count=0;
    int dimensions=0,constraints=0,*dimension_ids=NULL;
    ProIntlist constraint_ids=NULL;
    ProSectionShape shape=0;
    ProError error=ProFeatureSectionCopy(feature,index,&section);
    if(!error)error=ProSectionEntityIdsGet(section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        Pro2dEntdef *entity=NULL;
        ProBoolean construction;
        ProBoolean projected=PRO_B_FALSE;
        error=ProSectionEntityGet(section,ids[i],&entity);
        if(!error)error=ProSectionEntityIsConstruction(section,ids[i],&construction);
        if(!error&&!ProSectionEntityIsProjection(section,ids[i],&projected)&&projected)projection_count++;
        if(!error&&(construction||(entity&&entity->type==PRO_2D_CENTER_LINE)))
            ids[construction_count++]=ids[i];
    }
    part_text_add(out,L"  特征 ID %d 的截面 %d",feature->id,index+1);
    if(!error){
        part_text_add(out,L"，图元 %d 个，构造图元 %d 个",count,construction_count);
        for(int i=0;i<construction_count;i++)part_text_add(out,L" ID %d",ids[i]);
        part_text_add(out,L"，投影图元 %d 个",projection_count);
        if(!ProSecdimIdsGet(section,&dimension_ids,&dimensions))part_text_add(out,L"，尺寸 %d 个",dimensions);
        if(!ProSectionConstraintsIdsGet(section,&constraint_ids,&constraints))part_text_add(out,L"，约束 %d 个",constraints);
        if(!ProSectionShapeGet(section,&shape))part_text_add(out,L"，轮廓类型 %d",shape);
    }
    else part_text_add(out,L"，图元读取返回 Creo %d",error);
    part_text_add(out,L"。\n");
    if(ids)ProArrayFree((ProArray *)&ids);
    if(dimension_ids)ProArrayFree((ProArray *)&dimension_ids);
    if(constraint_ids)ProArrayFree((ProArray *)&constraint_ids);
    if(section)ProSectionFree(&section);
}

static void replay_preview_members(ProFeature *members,int skip,PartText *out)
{
    int count=0,shown=0;
    if(members)ProArraySizeGet((ProArray)members,&count);
    for(int i=0;i<count;i++){
        if(members[i].id==skip)continue;
        ProName name=L"";
        ProModelitemNameGet(&members[i],name);
        part_text_add(out,L"%ls%ls%lsID %d",shown++?L"、":L"",
            name[0]?name:L"",name[0]?L" ":L"",members[i].id);
    }
    if(!shown)part_text_add(out,L"无");
    part_text_add(out,L"。\n");
}

static void replay_preview_structure(ProFeature *feature,ProFeattype type,ProPatternStatus pattern,
                                     int sections,PartText *out)
{
    for(int i=0;i<sections;i++)replay_preview_section(feature,i,out);
    if(pattern==PRO_PATTERN_LEADER){
        ProPattern definition;
        ProFeature header,*members=NULL;
        ProError error=ProFeaturePatternGet(feature,replay_pattern_class(type),&definition);
        if(!error)error=ProPatternMembersGet(&definition,&members);
        if(!error){
            part_text_add(out,L"  阵列");
            if(!ProPatternHeaderGet(&definition,&header))part_text_add(out,L" ID %d",header.id);
            part_text_add(out,L"，种子 ID %d，从属实例 ",feature->id);
            replay_preview_members(members,feature->id,out);
        }else part_text_add(out,L"  阵列成员读取返回 Creo %d。\n",error);
        if(members)ProArrayFree((ProArray *)&members);
    }
    if(type==PRO_FEAT_GROUP_HEAD){
        ProFeature *members=NULL;
        const ProError error=ProGroupFeaturesCollect((ProGroup *)feature,&members);
        if(!error){
            part_text_add(out,L"  组 ID %d 的成员顺序 ",feature->id);
            replay_preview_members(members,feature->id,out);
        }else part_text_add(out,L"  组成员读取返回 Creo %d。\n",error);
        if(members)ProArrayFree((ProArray *)&members);
    }
}

static void replay_preview_embedded(ProFeature *feature,PartText *out)
{
    ProName name=L"";
    ProModelitemNameGet(feature,name);
    int count=0,*children=NULL;
    const ProError error=ProFeatureChildrenGet(feature,&children,&count);
    part_text_add(out,L"  内嵌特征 %ls%lsID %d",name,name[0]?L" ":L"",feature->id);
    if(!error&&count==1)part_text_add(out,L"，所属特征 ID %d",children[0]);
    else part_text_add(out,L"，所属特征未能唯一读取");
    part_text_add(out,L"。\n");
    if(children)ProArrayFree((ProArray *)&children);
}
