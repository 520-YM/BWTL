typedef struct {PartText *out;int feature,reported;} ImitationMissing;

static ProError imitation_missing_element(ProElement tree,ProElement element,ProElempath path,ProAppData data){
    ImitationMissing *scan=data;
    if(scan->reported==16)return PRO_TK_NO_ERROR;
    ProReference reference=NULL;
    ProRefState state;
    if(!ProElementReferenceGet(element,NULL,&reference)&&reference){
        int item=0;
        ProFamilyMdlName model=L"";
        if(ProReferenceIdGet(reference,&item))ProReferenceOriginalidGet(reference,&item);
        if(ProReferenceOwnerMdlnameGet(reference,model))ProReferenceOriginalownerMdlnameGet(reference,model);
        if(!ProReferenceStatusGet(reference,&state)&&
           (state==PRO_REF_MISSING||state==PRO_REF_INVALID||(state==PRO_REF_NOT_FOUND&&item>0))){
            ProElemId id=0;
            ProElementIdGet(element,&id);
            ProElement conversion=NULL;
            int kind=0;
            if(id==PRO_E_SMT_CONV_BODY&&item<0&&
               !mc_tree_element(tree,(ProElemId[]){PRO_E_DTM_SRF_TYPE},1,&conversion)&&
               !ProElementIntegerGet(conversion,NULL,&kind)&&kind==PRO_DTM_SRF_EMPTY_BODY_CONV){
                ProReferenceFree(reference);
                return PRO_TK_NO_ERROR;
            }
            part_text_add(scan->out,L"特征 ID %d，定义元素 %d，参照模型 %ls，项目 ID %d，状态 %d。\n",
                scan->feature,id,model[0]?model:L"Creo 未返回名称",item,state);
            scan->reported++;
        }
        ProReferenceFree(reference);
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_missing_feature(ProFeature *feature,ProError status,ProAppData data){
    ImitationMissing *scan=data;
    if(status||scan->reported==16)return PRO_TK_NO_ERROR;
    ProElement tree=NULL;
    if(!ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)){
        scan->feature=feature->id;
        ProElemtreeElementVisit(tree,NULL,NULL,imitation_missing_element,scan);
        ProFeatureElemtreeFree(feature,tree);
    }
    return PRO_TK_NO_ERROR;
}

static void imitation_dependency_names(ProMdl model,PartText *out){
    ProMdlnameShortdata *dependencies=NULL;
    ProMdlfileType *types=NULL;
    int count=0,shown=0;
    ProError error=ProMdlDependenciesDataList(model,&dependencies,&types,&count);
    if((!error||error==PRO_TK_BAD_CONTEXT)&&dependencies&&types){
        for(int i=0;i<count&&shown<12;i++){
            if(!dependencies[i].name[0])continue;
            if(!shown)part_text_add(out,L"Creo 记录的直接依赖：");
            const wchar_t *extension=types[i]==PRO_MDLFILE_ASSEMBLY?L".asm":
                types[i]==PRO_MDLFILE_PART?L".prt":L"";
            part_text_add(out,L"%ls%ls%ls",shown?L"、":L"",dependencies[i].name,extension);
            shown++;
        }
        if(shown)part_text_add(out,L"。\n");
    }
    if(dependencies)ProArrayFree((ProArray *)&dependencies);
    if(types)ProArrayFree((ProArray *)&types);
}


#include "imitation_external_context.inc"

static void imitation_reference_report(ProMdl model,PartText *out){
    ProExtRefInfo *refs=NULL;
    int groups=0,shown=0,total=0;
    ProSolid roots[16]={0};
    int root_count=0;
    int blocking=0;
    ProError error=ProSolidExternParentsGet((ProSolid)model,PRO_ALL_EXT_REF_TYPES,&refs,&groups);
    for(int i=0;!error&&i<groups;i++){
        total+=refs[i].n_refs;
        for(int j=0;j<refs[i].n_refs;j++){
            ProExtFeatRef reference=refs[i].ext_refs[j];
            ProFeature owner, upstream;
            ProSolid referenced=NULL;
            ProMdlName name=L"";
            ProRefState state;
            ProError state_error=ProExtRefStateGet(reference,&state);
            blocking+=state_error||state!=PRO_REF_ACTIVE;
            if(shown>=16)continue;
            int owner_id=ProExtRefOwnFeatGet(reference,&owner)?0:owner.id;
            if(!ProExtRefMdlGet(reference,&referenced))ProMdlMdlnameGet((ProMdl)referenced,name);
            ProModelitem item;
            ProError item_error=ProExtRefModelitemGet(reference,&item);
            int upstream_id=ProExtRefFeatGet(reference,&upstream)?0:upstream.id;
            part_text_add(out,L"特征 ID %d，外部模型 %ls，参照类型 %d，状态 %ls（%d）",
                owner_id,name[0]?name:L"Creo 未返回模型名",refs[i].type,state_error?L"无法读取":imitation_external_state(state),
                state_error?state_error:(int)state);
            if(upstream_id)part_text_add(out,L"，上游特征 %d",upstream_id);
            if(!item_error)part_text_add(out,L"，几何项目 %d（类型 %d）",item.id,item.type);
            ProAsmcomppath consumer={0},provider={0};
            if(!ProExtRefAsmcompsGet(reference,&consumer,&provider)){
                if(consumer.owner&&consumer.table_num>=0&&consumer.table_num<=_countof(consumer.comp_id_table)){
                    part_text_add(out,L"，使用者路径 ");
                    imitation_path_text(&consumer,out);
                    ProMdlType root_type;
                    int i=0;
                    while(i<root_count&&roots[i]!=consumer.owner)i++;
                    if(i==root_count&&root_count<_countof(roots)&&
                        !ProMdlTypeGet((ProMdl)consumer.owner,&root_type)&&root_type==PRO_MDL_ASSEMBLY)
                        roots[root_count++]=consumer.owner;
                }
                if(provider.owner&&provider.table_num>0&&provider.table_num<=_countof(provider.comp_id_table)){
                    part_text_add(out,L"，参照路径 ");
                    imitation_path_text(&provider,out);
                }
            }
            part_text_add(out,L"。\n");
            shown++;
        }
    }
    if(refs)ProExtRefInfoFree(&refs);
    if(error&&error!=PRO_TK_E_NOT_FOUND)
        part_text_add(out,L"Creo 外部参照读取返回 %d。\n",error);
    else part_text_add(out,L"Creo 返回 %d 条外部参照，已显示 %d 条。\n",total,shown);
    if(total){
        if(blocking)part_text_add(out,L"预检发现 %d 条外部参照当前无法直接绑定。\n",blocking);
        for(int i=0;i<root_count;i++)imitation_assembly_occurrences(model,roots[i],out);
        imitation_dependency_names(model,out);
        ImitationDependencyTrace chain={.out=out};
        imitation_dependency_trace(&chain,model,0);
    }
    ProFamtable table;
    if(!ProFamtableInit(model,&table)&&!ProFamtableCheck(&table))
        part_text_add(out,L"该零件含族表依赖。\n");
    ImitationMissing scan={out,0,0};
    ProSolidFeatVisit((ProSolid)model,imitation_missing_feature,NULL,&scan);
}

static ProError imitation_sheet_compare(ProMdl source,ProMdl target,PartText *out){
    ProMdlsubtype a,b;
    ProError error=ProMdlSubtypeGet(source,&a);
    if(!error)error=ProMdlSubtypeGet(target,&b);
    if(error)return error;
    if(a!=b)return PRO_TK_INCOMPLETE;
    if(a!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_NO_ERROR;
    double mm_a,mm_b,t_a,t_b;
    if((error=fab_thickness(source,&mm_a,&t_a))||(error=fab_thickness(target,&mm_b,&t_b)))return error;
    FabBends x={0},y={0};
    if((error=smy_bends_load(source,&x))||(error=smy_bends_load(target,&y)))return error;
    part_text_add(out,L"板厚 %.8g / %.8g mm，折弯面 %d / %d。\n",t_a,t_b,x.count,y.count);
    if(!imitation_metric_same(t_a,t_b)||x.count!=y.count)return PRO_TK_INCOMPLETE;
    int matched[_countof(x.rows)]={0};
    for(int i=0;i<x.count;i++){
        int j;
        for(j=0;j<y.count;j++)if(!matched[j]&&
            imitation_metric_same(fabs(x.rows[i].angle),fabs(y.rows[j].angle))&&
            imitation_metric_same(x.rows[i].radius,y.rows[j].radius)&&
            imitation_metric_same(x.rows[i].length,y.rows[j].length)&&
            imitation_metric_same(x.rows[i].developed,y.rows[j].developed)&&
            x.rows[i].method==y.rows[j].method){matched[j]=1;break;}
        if(j==y.count){
            const FabBend *bend=&x.rows[i];
            part_text_add(out,L"原件折弯 %ls（SURFACE:%d）未找到对应定义。角度 %.6g°，内半径 %.6g mm，折弯长 %.6g mm，展开弧长 %.6g mm，算法 %d。\n",
                bend->name,bend->surface,bend->angle,bend->radius,bend->length,bend->developed,bend->method);
            return PRO_TK_INCOMPLETE;
        }
    }
    if(x.count)part_text_add(out,L"已逐段核对 %d 个折弯面的角度、内半径、长度、展开弧长及折弯算法。\n",x.count);
    return PRO_TK_NO_ERROR;
}

static ProError imitation_reopen_candidate(ProMdl *copy,const DeliveryOutput *work,const wchar_t *name){
    static volatile LONG serial;
    ProPath leaf,path,preserved;
    ProError error=ProMdlnameBackup(*copy,(wchar_t *)work->stage);
    if(!error)error=dv_native_revision(work->stage,name,leaf);
    if(!error&&swprintf_s(path,_countof(path),L"%ls\\%ls",work->stage,leaf)<0)error=PRO_TK_BAD_INPUTS;
    ProMdlName temporary;
    swprintf_s(temporary,_countof(temporary),L"bwtl_chk_%08lx_%08lx",GetCurrentProcessId(),
               (unsigned long)InterlockedIncrement(&serial));
    if(!error&&swprintf_s(preserved,_countof(preserved),L"%ls\\%ls.bak",work->stage,temporary)<0)
        error=PRO_TK_BAD_INPUTS;
    if(!error&&!CopyFileW(path,preserved,TRUE))error=PRO_TK_CANT_WRITE;
    /* Creo can defer erasure of an active model and rename its on-disk
       revision. Preserve the saved revision before freeing the session name. */
    if(!error)error=ProMdlnameRename(*copy,temporary);
    if(!error&&!MoveFileExW(preserved,path,MOVEFILE_REPLACE_EXISTING))error=PRO_TK_CANT_WRITE;
    if(error)return error;
    ProMdl reopened=NULL;
    error=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&reopened);
    if(error)trace("Load saved candidate after freeing name",error);
    if(error)return error;
    if(!reopened||reopened==*copy)return PRO_TK_INCOMPLETE;
    error=ProMdlErase(*copy);
    if(error)trace("Erase renamed scratch after candidate reopen",error);
    if(error){ProMdlErase(reopened);return error;}
    *copy=reopened;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_edit_trial(ProMdl copy,PartText *out){
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!snapshot)return PRO_TK_OUT_OF_MEMORY;
    ImitationShapeMetrics before,after;
    ProError error=ProSolidDimensionVisit((ProSolid)copy,PRO_B_FALSE,part_dimension_visit,NULL,snapshot);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error)error=imitation_shape_read(copy,&before);
    int tested=0,verified=0;
    for(int pass=0;!error&&!verified&&pass<2;pass++){
        for(int i=0;!error&&!verified&&i<snapshot->dimensions&&tested<12;i++){
            const PartDimension *d=&snapshot->dimension[i];
            if(d->type!=(pass?PRODIMTYPE_LINEAR:PRODIMTYPE_DIAMETER)||d->negative||d->related||d->value<=0)continue;
            ProDimension dim;
            if(ProModelitemInit(copy,d->handle.id,PRO_DIMENSION,&dim))continue;
            tested++;
            double next=d->value+fmax(0.01/before.mm,d->value*0.005);
            if(ProDimensionValueSet(&dim,next))continue;
            ProError trial=part_regenerate(copy);
            if(!trial)trial=imitation_shape_read(copy,&after);
            if(!trial&&(imitation_metric_same(before.volume,after.volume)&&imitation_metric_same(before.area,after.area)))
                trial=PRO_TK_INCOMPLETE;
            error=ProDimensionValueSet(&dim,d->value);
            if(!error)error=part_regenerate(copy);
            if(!error)error=imitation_shape_read(copy,&after);
            double restored=0;
            if(!error)error=ProDimensionValueGet(&dim,&restored);
            if(!error&&(!part_close(restored,d->value)||!imitation_metric_same(before.volume,after.volume)||
                        !imitation_metric_same(before.area,after.area)))error=PRO_TK_INCOMPLETE;
            if(!error&&!trial){
                verified=1;
                part_text_add(out,L"临时改动尺寸 ID %d，%.8g → %.8g mm，几何随动；已恢复原尺寸并再生。\n",
                    dim.id,d->value*before.mm,next*before.mm);
            }
        }
    }
    part_snapshot_clear(snapshot);
    free(snapshot);
    if(!error&&!verified){
        part_text_add(out,L"试验了 %d 个候选尺寸，未确认可随尺寸变化的几何。\n",tested);
        return PRO_TK_INCOMPLETE;
    }
    return error;
}
