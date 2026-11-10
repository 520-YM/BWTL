#define AX_LAYOUT_GRAPH_MAX 192
typedef struct {
    ProMdl models[AX_LAYOUT_GRAPH_MAX];
    unsigned char edges[AX_LAYOUT_GRAPH_MAX][AX_LAYOUT_GRAPH_MAX];
    int count,refs,placements,missing,unavailable,limited,shown;
} AxLayoutGraph;

static const wchar_t *ax_layout_ref_state(ProRefState state){
    switch(state){
    case PRO_REF_ACTIVE:return L"活动";
    case PRO_REF_EXCLUDED:return L"被排除";
    case PRO_REF_FROZEN:return L"冻结或源未加载";
    case PRO_REF_SUPPRESSED:return L"源特征已抑制";
    case PRO_REF_MISSING:return L"源项目丢失";
    case PRO_REF_FROZEN_PLACE:return L"位置冻结";
    case PRO_REF_INVALID:return L"参照无效";
    case PRO_REF_NOT_FOUND:return L"未找到参照";
    default:return L"需核对";
    }
}

static int ax_layout_graph_node(AxLayoutGraph *graph,ProMdl model){
    for(int i=0;i<graph->count;i++)if(graph->models[i]==model)return i;
    if(graph->count==AX_LAYOUT_GRAPH_MAX){graph->limited=1;return -1;}
    int index=graph->count++;
    graph->models[index]=model;
    return index;
}

static void ax_layout_graph_model(AxLayoutGraph *graph,int index,PartText *out,int list){
    ProMdl model=graph->models[index];
    ProExtRefInfo *groups=NULL;
    int count=0;
    ProError error=ProSolidExternParentsGet((ProSolid)model,PRO_ALL_EXT_REF_TYPES,&groups,&count);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(error){graph->unavailable++;return;}
    ProMdlName name=L"";
    ProMdlMdlnameGet(model,name);
    for(int i=0;i<count;i++)for(int j=0;j<groups[i].n_refs;j++){
        ProExtFeatRef ref=groups[i].ext_refs[j];
        ProSolid other=NULL;
        ProRefState state=PRO_REF_NOT_FOUND;
        ProError state_error=ProExtRefStateGet(ref,&state);
        ProError model_error=ProExtRefMdlGet(ref,&other);
        int unresolved=model_error||state_error;
        if(other==(ProSolid)model)continue;
        int missing=!state_error&&(state==PRO_REF_MISSING||state==PRO_REF_NOT_FOUND||
            state==PRO_REF_INVALID||state==PRO_REF_SUPPRESSED);
        graph->refs++;
        graph->missing+=missing;
        graph->unavailable+=unresolved&&!missing;
        if(!unresolved&&other){
            int neighbor=ax_layout_graph_node(graph,(ProMdl)other);
            if(neighbor>=0)graph->edges[index][neighbor]=1;
        }
        if((list||missing||unresolved)&&graph->shown<120){
            ProMdlName source=L"源模型当前不可读取";
            ProFeature own_feature={0},ref_feature={0};
            if(other)ProMdlMdlnameGet((ProMdl)other,source);
            ProExtRefOwnFeatGet(ref,&own_feature);
            ProExtRefFeatGet(ref,&ref_feature);
            part_text_add(out,L"%ls 特征 %d → %ls 特征 %d，类型 %d，%ls。\n",name,own_feature.id,source,ref_feature.id,
                groups[i].type,state_error?L"状态不可读取":ax_layout_ref_state(state));
            graph->shown++;
        }
    }
    if(groups)ProExtRefInfoFree(&groups);
}

static void ax_layout_placement_refs(ProMdl assembly,AxOccurrence *entry,AxLayoutGraph *graph,PartText *out,int list){
    ProAsmcomppath owner_path;
    ProIdTable ids={0};
    ProSolid owner=NULL;
    ProAsmcomp feature;
    ProAsmcompconstraint *rows=NULL;
    int count=0,depth=entry->depth-1;
    memcpy(ids,entry->ids,(size_t)depth*sizeof(ids[0]));
    ProError error=ProAsmcomppathInit((ProSolid)assembly,ids,depth,&owner_path);
    if(!error)error=ProAsmcomppathMdlGet(&owner_path,&owner);
    if(!error)error=ax_component_feature((ProMdl)owner,entry->ids[depth],&feature);
    if(!error)error=ax_read_constraints(&feature,&owner_path,&rows,&count);
    if(error){graph->unavailable++;goto done;}
    wchar_t target[256];
    if(!ax_path_label(entry->ids,entry->depth,target,_countof(target)))goto done;
    for(int i=0;i<count;i++){
        ProAsmcompConstrType kind;
        if(ProAsmcompconstraintTypeGet(rows[i],&kind)){graph->unavailable++;continue;}
        if(kind==PRO_ASM_FIX)continue;
        ProDatumside side;
        ProSelection reference=NULL;
        ProModelitem item={0};
        ProFeature parent={0};
        ProFeatStatus state=PRO_FEAT_ACTIVE;
        ProMdlName model=L"源模型不可读取";
        error=ProAsmcompconstraintAsmreferenceGet(rows[i],&reference,&side);
        if(!error&&reference)error=ProSelectionModelitemGet(reference,&item);
        if(!error&&item.owner)ProMdlMdlnameGet((ProMdl)item.owner,model);
        if(!error&&reference){
            error=ProSelectionVerify(reference);
            if(!error&&!ProGeomitemFeatureGet(&item,&parent))error=ProFeatureStatusGet(&parent,&state);
        }
        int broken=!reference||error||state!=PRO_FEAT_ACTIVE;
        graph->placements++;
        graph->missing+=broken;
        if((list||broken)&&graph->shown<120){
            part_text_add(out,L"%ls %ls 的装配定位 → %ls 参照 %d，%ls。\n",target,entry->name,model,item.id,
                broken?L"参照不可用":L"活动");
            graph->shown++;
        }
    }
done:
    if(rows)ProAsmcompconstraintArrayFree(rows);
}

static void ax_layout_graph_close(AxLayoutGraph *graph){
    for(int via=0;via<graph->count;via++)
        for(int from=0;from<graph->count;from++)if(graph->edges[from][via])
            for(int to=0;to<graph->count;to++)graph->edges[from][to]|=graph->edges[via][to];
}

static ProError ax_layout_refs(ProMdl assembly,const PartCommand *cmd,int kind,PartText *out){
    AxLayoutGraph *graph=calloc(1,sizeof(*graph));
    if(!graph)return PRO_TK_OUT_OF_MEMORY;
    AxScan *scan=NULL;
    ProError error=PRO_TK_NO_ERROR;
    int all=!wcscmp(cmd->arg[2],L"ALL"),list=kind==AX_LAYOUT_REFS;
    if(all){
        ax_layout_graph_node(graph,assembly);
        error=ax_scan(assembly,0,&scan);
        if(error)goto done;
        graph->limited=scan->limited;
        graph->unavailable=scan->unavailable+scan->suppressed;
        for(int i=0;i<scan->count;i++)if(scan->occurrences[i].model)
            ax_layout_graph_node(graph,scan->occurrences[i].model);
    }else{
        AxComponent scope={0};
        error=ax_ref_occurrence(assembly,cmd->arg[2],&scope);
        if(error)goto done;
        ax_layout_graph_node(graph,scope.model);
    }
    int roots=graph->count;
    for(int i=0;i<(list?roots:graph->count);i++)ax_layout_graph_model(graph,i,out,list);
    if(!scan){
        error=ax_scan(assembly,0,&scan);
        if(error)goto done;
    }
    ProIdTable scope_ids={0};
    int scope_depth=ax_path_parse(cmd->arg[2],scope_ids);
    for(int i=0;i<scan->count;i++){
        AxOccurrence *entry=&scan->occurrences[i];
        if(entry->model&&entry->state==PRO_FEAT_ACTIVE&&(all||
           (scope_depth==entry->depth&&!memcmp(entry->ids,scope_ids,(size_t)scope_depth*sizeof(scope_ids[0])))))
            ax_layout_placement_refs(assembly,entry,graph,out,list);
    }
    int cycles=0;
    if(!list){
        ax_layout_graph_close(graph);
        for(int i=0;i<graph->count;i++)if(graph->edges[i][i]){
            ProMdlName name=L"";
            ProMdlMdlnameGet(graph->models[i],name);
            cycles++;
            if(graph->shown<120){part_text_add(out,L"可达循环成员 %ls。\n",name);graph->shown++;}
        }
        part_text_add(out,L"可达引用图中，参与循环的模型 %d 个。",cycles);
    }
    part_text_add(out,L"检查 %d 个模型，读取 %d 条特征外部参照及 %d 条装配定位参照，丢失/无效/被抑制 %d 条，状态不可确认或组件未纳入 %d 项。\n",
        list?roots:graph->count,graph->refs,graph->placements,graph->missing,graph->unavailable);
    if(graph->limited||graph->unavailable||graph->shown>=120)
        part_text_add(out,L"结果限于可读取的范围；部分组件、依赖或明细未纳入。\n");
done:
    free(scan);free(graph);
    return error;
}

static int ax_layout_same_path(AxOccurrence *a,AxOccurrence *b){
    return a->depth==b->depth&&!memcmp(a->ids,b->ids,(size_t)a->depth*sizeof(a->ids[0]));
}

static double ax_layout_shift(ProMatrix a,double a_mm,ProMatrix b,double b_mm){
    ProVector delta;
    for(int i=0;i<3;i++)delta[i]=a[3][i]*a_mm-b[3][i]*b_mm;
    return sqrt(ad_dot(delta,delta));
}

static double ax_layout_angle(ProMatrix a,ProMatrix b){
    double sum=0;
    for(int i=0;i<3;i++)sum+=ad_dot(a[i],b[i]);
    return acos(qa_clamp((sum-1)*0.5))*180/AS_PI;
}

static ProError ax_layout_local_interface(ProMdl model,const wchar_t *token,ProMatrix pose){
    ProSelection selection=NULL;
    ProCsysdata frame;
    ProError error=ax_csys_selection(model,token,0,&selection,&frame);
    if(selection)ProSelectionFree(&selection);
    if(error)return error;
    memset(pose,0,sizeof(ProMatrix));
    double *data[]={frame.x_vector,frame.y_vector,frame.z_vector,frame.origin};
    for(int i=0;i<4;i++)memcpy(pose[i],data[i],sizeof(ProVector));
    pose[3][3]=1;
    return PRO_TK_NO_ERROR;
}

/* Replacement creates a new Creo occurrence ID. Match an unambiguous station
   only when neither occurrence still has its original path in the other model. */
static void ax_config_station_pairs(ProMdl assembly,ProMdl old,AxScan *a,AxScan *b,double mm,double old_mm,int pairs[128]){
    if(a->count>128||b->count>128||a->limited||b->limited)return;
    ProMatrix current[128],prior[128];
    int a_ready[128]={0},b_ready[128]={0},a_hits[128]={0},b_hits[128]={0};
    for(int i=0;i<a->count;i++){
        AxOccurrence *item=&a->occurrences[i];
        if(item->depth==1&&item->state==PRO_FEAT_ACTIVE&&item->model)
            a_ready[i]=!ax_layout_measure(assembly,item,current[i]);
    }
    for(int j=0;j<b->count;j++){
        AxOccurrence *item=&b->occurrences[j];
        if(item->depth==1&&item->state==PRO_FEAT_ACTIVE&&item->model)
            b_ready[j]=!ax_layout_measure(old,item,prior[j]);
    }
    for(int i=0;i<a->count;i++)for(int j=0;j<b->count;j++)
        if(ax_layout_same_path(&a->occurrences[i],&b->occurrences[j]))a_ready[i]=b_ready[j]=0;
    for(int i=0;i<a->count;i++)if(a_ready[i])
        for(int j=0;j<b->count;j++)if(b_ready[j]&&
            ax_layout_shift(current[i],mm,prior[j],old_mm)<0.001&&ax_layout_angle(current[i],prior[j])<0.01){
            a_hits[i]++;
            b_hits[j]++;
            pairs[i]=j;
        }
    for(int i=0;i<a->count;i++)
        if(a_hits[i]!=1||b_hits[pairs[i]]!=1)pairs[i]=-1;
}

static ProError ax_layout_compare(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    ProMdl old=NULL;
    AxScan *a=NULL,*b=NULL;
    unsigned char *matched=NULL;
    double old_mm,threshold=as_arg(cmd,4);
    int loaded=0,added=0,removed=0,substituted=0,moved=0,interfaces=0,unknown=0,shown=0;
    ProError error;
#define LC(call) do{if((error=(call))){trace("layout compare " #call,error);goto done;}}while(0)
    LC(ax_load_subassembly(cmd->arg[2],&old,&loaded));
    if(old==assembly){error=PRO_TK_BAD_INPUTS;goto done;}
    LC(ax_master(old));
    LC(qa_mm_per_unit(old,&old_mm));
    LC(ax_scan(assembly,0,&a));LC(ax_scan(old,0,&b));
    matched=calloc(b->count?b->count:1,1);
    if(!matched){error=PRO_TK_OUT_OF_MEMORY;goto done;}
    int pairs[128];
    for(int i=0;i<_countof(pairs);i++)pairs[i]=-1;
    int config_compare=ax_kind(cmd->arg[0])==AX_V_COMPARE;
    if(config_compare)ax_config_station_pairs(assembly,old,a,b,mm,old_mm,pairs);
    for(int i=0;i<a->count;i++){
        AxOccurrence *now=&a->occurrences[i],*prior=NULL;
        for(int j=0;j<b->count;j++)if(ax_layout_same_path(now,&b->occurrences[j])){
            matched[j]=1;prior=&b->occurrences[j];break;
        }
        wchar_t path[256];
        if(!ax_path_label(now->ids,now->depth,path,_countof(path)))continue;
        if(!prior&&i<_countof(pairs)&&pairs[i]>=0){
            prior=&b->occurrences[pairs[i]];
            matched[pairs[i]]=1;
            if(shown++<120)part_text_add(out,L"%ls 与另一装配 ID:%d 按唯一同位姿工位对应。\n",path,prior->ids[0]);
        }
        if(!prior){
            added++;
            if(shown++<120)part_text_add(out,L"%ls 新增 %ls。\n",path,now->name);
            continue;
        }
        int changed_model=_wcsicmp(now->name,prior->name)!=0||now->type!=prior->type;
        substituted+=changed_model;
        if(changed_model&&shown++<120)part_text_add(out,L"%ls 型号变更 %ls → %ls。\n",path,prior->name,now->name);
        if(!now->model||!prior->model||now->state!=PRO_FEAT_ACTIVE||prior->state!=PRO_FEAT_ACTIVE){unknown++;continue;}
        ProMatrix recent,previous;
        if(ax_layout_measure(assembly,now,recent)||ax_layout_measure(old,prior,previous)){unknown++;continue;}
        double shift=ax_layout_shift(recent,mm,previous,old_mm),angle=ax_layout_angle(recent,previous);
        if(shift>threshold||angle>0.01){
            moved++;
            if(shown++<120)part_text_add(out,L"%ls %ls 原点位移 %.8g mm，方向变化 %.8g 度。\n",path,now->name,shift,angle);
        }
        if(wcscmp(cmd->arg[3],L"NONE")){
            ProError e1=ax_layout_local_interface(now->model,cmd->arg[3],recent);
            ProError e2=ax_layout_local_interface(prior->model,cmd->arg[3],previous);
            if(e1&&e1!=PRO_TK_E_NOT_FOUND){unknown++;continue;}
            if(e2&&e2!=PRO_TK_E_NOT_FOUND){unknown++;continue;}
            double m1,m2;
            if(!e1&&!e2&&(qa_mm_per_unit(now->model,&m1)||qa_mm_per_unit(prior->model,&m2))){unknown++;continue;}
            if((!e1)!=(!e2)||(!e1&&!e2&&(ax_layout_shift(recent,m1,previous,m2)>threshold||ax_layout_angle(recent,previous)>0.01))){
                interfaces++;
                if(shown++<120)part_text_add(out,L"%ls %ls 的局部接口 %ls 已变化。\n",path,now->name,cmd->arg[3]);
            }
        }
    }
    for(int j=0;j<b->count;j++)if(!matched[j]){
        wchar_t path[256];
        AxOccurrence *prior=&b->occurrences[j];
        removed++;
        if(shown++<120&&ax_path_label(prior->ids,prior->depth,path,_countof(path)))
            part_text_add(out,L"%ls 移除 %ls。\n",path,prior->name);
    }
    part_text_add(out,L"%ls：新增 %d，移除 %d，型号变更 %d，位姿变化 %d，接口变化 %d，无法完整对比 %d。\n",
        config_compare?L"配置对比":L"按实例路径对比",added,removed,substituted,moved,interfaces,unknown);
    if(shown>120||a->limited||b->limited)part_text_add(out,L"部分明细或实例超出本次显示范围。\n");
done:
    free(matched);free(a);free(b);
    if(loaded&&old)ProMdlErase(old);
    return error;
#undef LC
}