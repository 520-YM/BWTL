#define QA_DEP_MAX_NODES 128
#define QA_DEP_MAX_LINKS 240
typedef struct {int id;int level;} QualityDependency;

static void qa_feature_label(ProFeature *feature,ProName label){
    if(ProModelitemNameGet((ProModelitem *)feature,label)||!label[0])
        swprintf_s(label,PRO_NAME_SIZE,L"ID:%d",feature->id);
}

static ProError qa_dependency_walk(ProMdl model,ProFeature root,int parents,int depth,PartText *out){
    QualityDependency queue[QA_DEP_MAX_NODES]={{root.id,0}};
    int length=1,links=0,truncated=0;
    part_text_add(out,L"%ls（向%ls最多%d层）\n",parents?L"上游参照":L"下游依赖",parents?L"上":L"下",depth);
    for(int head=0;head<length&&links<QA_DEP_MAX_LINKS;head++){
        QualityDependency node=queue[head];
        if(node.level==depth)continue;
        ProFeature feature;
        ProName name;
        ProError error=ProFeatureInit((ProSolid)model,node.id,&feature);
        if(error)return error;
        qa_feature_label(&feature,name);
        int *ids=NULL,count=0;
        error=parents?ProFeatureParentsGet(&feature,&ids,&count):ProFeatureChildrenGet(&feature,&ids,&count);
        if(error&&error!=PRO_TK_E_NOT_FOUND){
            if(ids)ProArrayFree((ProArray *)&ids);
            return error;
        }
        for(int i=0;i<count;i++){
            if(links==QA_DEP_MAX_LINKS){truncated=1;break;}
            ProFeature related;
            ProName related_name;
            ProFeatStatus state;
            error=ProFeatureInit((ProSolid)model,ids[i],&related);
            if(!error)error=ProFeatureStatusGet(&related,&state);
            if(error){truncated=1;continue;}
            qa_feature_label(&related,related_name);
            part_text_add(out,L"第%d层：%ls [%d] → %ls [%d]，关联特征状态 %d\n",
                node.level+1,name,node.id,related_name,ids[i],state);
            links++;
            int known=0;
            for(int j=0;j<length;j++)if(queue[j].id==ids[i]){known=1;break;}
            if(!known&&length<QA_DEP_MAX_NODES){
                QualityDependency next={ids[i],node.level+1};
                queue[length++]=next;
            }else if(!known)truncated=1;
        }
        if(ids)ProArrayFree((ProArray *)&ids);
    }
    part_text_add(out,L"已读关联 %d 条。%ls\n",links,truncated||links==QA_DEP_MAX_LINKS?L"有无法读取或超出上限的分支。":L"");
    return PRO_TK_NO_ERROR;
}

static ProError qa_deps(ProMdl model,const PartCommand *cmd,PartText *out){
    ProFeature root;
    ProError error;
    int id,depth;
    if(qa_ref(cmd->arg[2],L"ID:")){
        part_integer(cmd->arg[2]+3,&id);
        error=ProFeatureInit((ProSolid)model,id,&root);
    }else error=ux_named_feature(model,cmd->arg[2],&root);
    if(error)return error;
    part_integer(cmd->arg[4],&depth);
    ProName name;
    qa_feature_label(&root,name);
    part_text_add(out,L"起点 %ls [%d]。范围为当前零件原生父子依赖。\n",name,root.id);
    if(wcscmp(cmd->arg[3],L"DOWN")&&(error=qa_dependency_walk(model,root,1,depth,out)))return error;
    if(wcscmp(cmd->arg[3],L"UP"))return qa_dependency_walk(model,root,0,depth,out);
    return PRO_TK_NO_ERROR;
}
