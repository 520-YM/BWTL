typedef struct {
    ProMdl wanted;
    ProAsmcomppath matches[16];
    int count;
} ImitationOccurrences;

static ProError imitation_component_path(ProAsmcomppath *path,ProSolid solid,ProBoolean down,ProAppData data)
{
    ImitationOccurrences *scan=data;
    if(!down||(ProMdl)solid!=scan->wanted)return PRO_TK_NO_ERROR;
    if(scan->count<_countof(scan->matches))scan->matches[scan->count]=*path;
    scan->count++;
    return PRO_TK_NO_ERROR;
}

static void imitation_path_text(const ProAsmcomppath *path,PartText *out)
{
    ProMdlName name=L"";
    if(!path->owner||path->table_num<0||path->table_num>_countof(path->comp_id_table))return;
    if(!ProMdlMdlnameGet((ProMdl)path->owner,name))part_text_add(out,L"%ls",name);
    for(int i=0;i<path->table_num;i++)part_text_add(out,L"/%d",path->comp_id_table[i]);
}

static void imitation_assembly_occurrences(ProMdl source,ProSolid root,PartText *out)
{
    ImitationOccurrences scan={.wanted=source};
    ProError error=ProSolidDispCompVisit(root,imitation_component_path,NULL,&scan);
    if(error&&error!=PRO_TK_E_NOT_FOUND){
        part_text_add(out,L"装配中的实例读取返回 Creo %d。\n",error);
        return;
    }
    part_text_add(out,L"当前装配中同一零件有 %d 个可见实例。\n",scan.count);
    for(int i=0;i<scan.count&&i<_countof(scan.matches);i++){
        ProMatrix pose;
        part_text_add(out,L"  实例路径 ");
        imitation_path_text(&scan.matches[i],out);
        if(!ProAsmcomppathTrfGet(&scan.matches[i],PRO_B_TRUE,pose))
            part_text_add(out,L"，装配中的原点 (%.8g, %.8g, %.8g)，X 轴 (%.6g, %.6g, %.6g)",
                pose[3][0],pose[3][1],pose[3][2],pose[0][0],pose[0][1],pose[0][2]);
        part_text_add(out,L"。\n");
    }
    if(scan.count>_countof(scan.matches))part_text_add(out,L"已显示前 %d 个实例。\n",_countof(scan.matches));
}

static const wchar_t *imitation_external_state(ProRefState state)
{
    switch(state){
    case PRO_REF_ACTIVE:return L"可用";
    case PRO_REF_FROZEN:return L"冻结（上游模型未在会话）";
    case PRO_REF_FROZEN_PLACE:return L"冻结（上游位置无法更新）";
    case PRO_REF_NOT_FOUND:case PRO_REF_MISSING:return L"缺失";
    case PRO_REF_INVALID:return L"失效（上游几何类型或定义改变）";
    case PRO_REF_EXCLUDED:return L"上游被简化表示或族表排除";
    case PRO_REF_SUPPRESSED:return L"上游特征已抑制";
    case PRO_REF_ALTERNATE:return L"替代参照";
    case PRO_REF_WARNING:return L"参照有警告";
    default:return L"受限";
    }
}

typedef struct {
    ProMdl seen[24];
    PartText *out;
    int count;
} ImitationDependencyTrace;

static const wchar_t *imitation_dependency_kind(ProExtRefType type,ProSolid provider,ProFeature *upstream)
{
    ProFeattype feature;
    if(upstream->id>0&&!ProFeatureTypeGet(upstream,&feature)&&
       (feature==PRO_FEAT_GEOM_COPY||feature==PRO_FEAT_MERGE))return L"复制或合并几何";
    ProBoolean skeleton;
    if(!ProMdlIsSkeleton((ProMdl)provider,&skeleton)&&skeleton)return L"骨架";
    if(type==PRO_EXT_REL_REF)return L"外部关系";
    if(type==PRO_MERGE_REF)return L"合并参照";
    return L"外部几何";
}

static void imitation_dependency_trace(ImitationDependencyTrace *trace,ProMdl model,int depth)
{
    for(int i=0;i<trace->count;i++)if(trace->seen[i]==model)return;
    if(depth>6||trace->count==_countof(trace->seen))return;
    trace->seen[trace->count++]=model;
    ProExtRefInfo *groups=NULL;
    int count=0;
    ProError error=ProSolidExternParentsGet((ProSolid)model,PRO_ALL_EXT_REF_TYPES,&groups,&count);
    for(int i=0;!error&&i<count;i++)for(int j=0;j<groups[i].n_refs&&trace->count<_countof(trace->seen);j++){
        ProExtFeatRef reference=groups[i].ext_refs[j];
        ProSolid provider=NULL;
        ProFeature used={0},upstream={0};
        ProRefState state;
        if(ProExtRefStateGet(reference,&state)||state!=PRO_REF_ACTIVE||
           ProExtRefMdlGet(reference,&provider)||!provider||provider==(ProSolid)model)continue;
        if(depth){
            ProMdlName from=L"",to=L"";
            ProMdlMdlnameGet(model,from);
            ProMdlMdlnameGet((ProMdl)provider,to);
            ProExtRefOwnFeatGet(reference,&used);
            ProExtRefFeatGet(reference,&upstream);
            part_text_add(trace->out,L"  上游第 %d 层 %ls 特征 ID %d → %ls 特征 ID %d（%ls）。\n",
                depth+1,from,used.id,to,upstream.id,imitation_dependency_kind(groups[i].type,provider,&upstream));
        }
        imitation_dependency_trace(trace,(ProMdl)provider,depth+1);
    }
    if(groups)ProExtRefInfoFree(&groups);
}
