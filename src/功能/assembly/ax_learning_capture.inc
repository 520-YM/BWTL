typedef struct {
    int subassemblies,placed,missing,poses,constraints,incomplete;
} AxLearningStats;

static ProMdl ax_learning_drawing_model(ProDrawing drawing){
    ProSolid current=NULL;
    ProMdlType type;
    if(!ProDrawingCurrentsolidGet(drawing,&current)&&current&&!ProMdlTypeGet(current,&type)&&type==PRO_MDL_ASSEMBLY)
        return (ProMdl)current;

    ProSolid *solids=NULL;
    ProMdl assembly=NULL;
    int count=0;
    if(!ProDrawingSolidsCollect(drawing,&solids)&&solids&&!ProArraySizeGet((ProArray)solids,&count)){
        for(int i=0;i<count;i++){
            if(ProMdlTypeGet(solids[i],&type)||type!=PRO_MDL_ASSEMBLY)continue;
            if(assembly&&assembly!=(ProMdl)solids[i]){assembly=NULL;break;}
            assembly=(ProMdl)solids[i];
        }
    }
    if(solids)ProArrayFree((ProArray *)&solids);
    return assembly;
}

static int ax_learning_reference(ProSelection selection,wchar_t *label,size_t capacity){
    ProModelitem item;
    ProMdlName model=L"";
    ProName feature_name=L"";
    ProFeature feature;
    ProAsmcomppath path;
    wchar_t location[AX_PATH_DEPTH*12+8]=L"所在模型";
    if(!selection||ProSelectionVerify(selection)||ProSelectionModelitemGet(selection,&item)){
        wcscpy_s(label,capacity,L"参照缺失或无法解析");
        return 0;
    }
    if(!ProSelectionAsmcomppathGet(selection,&path)&&path.table_num>0&&path.table_num<=AX_PATH_DEPTH)
        ax_path_label(path.comp_id_table,path.table_num,location,_countof(location));
    if(item.owner)ProMdlMdlnameGet(item.owner,model);
    if(item.type==PRO_SURFACE||item.type==PRO_AXIS||item.type==PRO_CSYS||item.type==PRO_EDGE){
        if(!ProGeomitemFeatureGet(&item,&feature))ProModelitemNameGet(&feature,feature_name);
    }else ProModelitemNameGet(&item,feature_name);
    ax_learning_clean(model);
    ax_learning_clean(feature_name);
    const wchar_t *kind=item.type==PRO_SURFACE?L"SURFACE":item.type==PRO_AXIS?L"AXIS":
        item.type==PRO_CSYS?L"CSYS":item.type==PRO_EDGE?L"EDGE":L"ITEM";
    swprintf_s(label,capacity,L"%ls %ls %ls:%d%ls%ls",location,model,kind,item.id,
        feature_name[0]?L" 所属命名特征=":L"",feature_name);
    return 1;
}

static void ax_learning_constraints(FILE *file,ProAsmcomp *component,ProAsmcomppath *owner,double mm,AxLearningStats *stats){
    ProAsmcompconstraint *rows=NULL;
    int count=0;
    ProError error=ax_read_constraints(component,owner,&rows,&count);
    if(error){
        fwprintf(file,L"  定位约束读取失败（Creo %d）。\n",error);
        stats->incomplete++;
    }else{
        fwprintf(file,L"  定位约束 %d 条。\n",count);
        for(int i=0;i<count;i++){
            ProAsmcompConstrType type;
            if(ProAsmcompconstraintTypeGet(rows[i],&type)){
                fwprintf(file,L"  第 %d 条约束类型未读到。\n",i+1);
                stats->incomplete++;
                continue;
            }
            stats->constraints++;
            fwprintf(file,L"  约束 %d %ls（原生类型 %d）",i+1,type==PRO_ASM_DEF_PLACEMENT?L"默认定位":ax_constraint_label(type),type);
            if(type!=PRO_ASM_FIX&&type!=PRO_ASM_DEF_PLACEMENT){
                ProSelection base=NULL,part=NULL;
                ProDatumside base_side=0,part_side=0;
                ProError a=ProAsmcompconstraintAsmreferenceGet(rows[i],&base,&base_side);
                ProError b=ProAsmcompconstraintCompreferenceGet(rows[i],&part,&part_side);
                wchar_t from[380],to[380];
                int have_base=ax_learning_reference(a?NULL:base,from,_countof(from));
                int have_part=ax_learning_reference(b?NULL:part,to,_countof(to));
                fwprintf(file,L" 基准 %ls（侧 %d） ↔ 组件 %ls（侧 %d）",from,base_side,to,part_side);
                if(!have_base||!have_part)stats->incomplete++;
            }
            if(ax_offset_type(type)){
                double offset;
                if(mm>0&&!ProAsmcompconstraintOffsetGet(rows[i],&offset)&&isfinite(offset))
                    fwprintf(file,L" 有符号偏距 %.10g mm",offset*mm);
                else{
                    fwprintf(file,L" 偏距未读到");
                    stats->incomplete++;
                }
            }
            fputwc(L'\n',file);
        }
    }
    if(rows)ProAsmcompconstraintArrayFree(rows);
}

static void ax_learning_occurrence(FILE *file,ProMdl assembly,const AxScan *scan,int index,double mm,AxLearningStats *stats){
    const AxOccurrence *item=&scan->occurrences[index];
    const int owner_depth=item->depth-1;
    ProMdl owner=assembly;
    for(int i=index-1;i>=0&&owner_depth>0;i--){
        const AxOccurrence *possible=&scan->occurrences[i];
        if(possible->depth==owner_depth&&!memcmp(possible->ids,item->ids,owner_depth*sizeof(int))){
            owner=possible->model;
            break;
        }
    }
    wchar_t path[AX_PATH_DEPTH*12+8],parent[AX_PATH_DEPTH*12+8]=L"ROOT";
    ProFamilyMdlName model;
    wcscpy_s(model,_countof(model),item->name);
    ax_learning_clean(model);
    ax_path_label(item->ids,item->depth,path,_countof(path));
    if(owner_depth)ax_path_label(item->ids,owner_depth,parent,_countof(parent));
    if(item->type==PRO_MDL_ASSEMBLY)stats->subassemblies++;
    fwprintf(file,L"实例 %ls\t%ls 父装配 %ls 模型树序号 %d %ls %ls\n",
        model[0]?model:L"名称不可用",path,parent,index+1,
        item->type==PRO_MDL_ASSEMBLY?L"子装配":item->type==PRO_MDL_PART?L"零件":L"组件",ax_state_label(item->state));
    if(!item->model||item->state!=PRO_FEAT_ACTIVE){
        stats->missing++;
        fwprintf(file,L"  当前表示未提供可核实的定位和下级数据。\n");
        return;
    }
    ProIdTable ids={0};
    ProAsmcomppath occurrence,parent_path;
    ProMatrix pose;
    memcpy(ids,item->ids,item->depth*sizeof(int));
    if(!ProAsmcomppathInit((ProSolid)assembly,ids,item->depth,&occurrence)&&
       !ProAsmcomppathTrfGet(&occurrence,PRO_B_TRUE,pose)){
        stats->poses++;
        fwprintf(file,L"  总装实测原点 (%.9g, %.9g, %.9g) mm，X (%.8g, %.8g, %.8g)，Y (%.8g, %.8g, %.8g)，Z (%.8g, %.8g, %.8g)。\n",
            pose[3][0]*mm,pose[3][1]*mm,pose[3][2]*mm,pose[0][0],pose[0][1],pose[0][2],
            pose[1][0],pose[1][1],pose[1][2],pose[2][0],pose[2][1],pose[2][2]);
    }else{
        fwprintf(file,L"  实际姿态未读到。\n");
        stats->incomplete++;
    }
    ProAsmcomp component;
    if(!owner||ax_component_feature(owner,ids[owner_depth],&component)||
       ProAsmcomppathInit((ProSolid)assembly,ids,owner_depth,&parent_path)){
        stats->incomplete++;
        fwprintf(file,L"  无法解析该实例的父装配和定位特征。\n");
        return;
    }
    ProBoolean packaged,under;
    if(ProAsmcompIsPackaged(&component,&packaged)||ProAsmcompIsUnderconstrained(&component,&under)){
        stats->incomplete++;
        fwprintf(file,L"  定位状态未读到。\n");
    }else{
        if(!packaged&&!under)stats->placed++;
        fwprintf(file,L"  定位状态 %ls。\n",packaged?L"封装，尚未定位":under?L"欠约束":L"Creo 未报告欠约束");
    }
    int *parents=NULL,count=0;
    ProError dependencies=ProFeatureParentsGet(&component,&parents,&count);
    if(!dependencies&&count){
        fwprintf(file,L"  父装配内的原生依赖特征 ID");
        for(int i=0;i<count;i++)fwprintf(file,L" %d",parents[i]);
        fputwc(L'\n',file);
    }else if(dependencies&&dependencies!=PRO_TK_E_NOT_FOUND){
        fwprintf(file,L"  父装配内的依赖未读到（Creo %d）。\n",dependencies);
        stats->incomplete++;
    }
    if(parents)ProArrayFree((ProArray *)&parents);
    double owner_mm=0;
    qa_mm_per_unit(owner,&owner_mm);
    ax_learning_constraints(file,&component,&parent_path,owner_mm,stats);
}

static ProError ax_learning_save(ProMdl assembly,PartText *out){
    ProMdlType type;
    double mm;
    ProMdlName source;
    ProError error=assembly?ProMdlTypeGet(assembly,&type):PRO_TK_E_NOT_FOUND;
    if(!error&&type==PRO_MDL_DRAWING){
        assembly=ax_learning_drawing_model((ProDrawing)assembly);
        error=assembly?ProMdlTypeGet(assembly,&type):PRO_TK_E_NOT_FOUND;
    }
    if(error||type!=PRO_MDL_ASSEMBLY){
        part_text_add(out,L"请先打开 Creo 总装 .asm，或打开引用目标总装的 Creo 工程图 .drw。\n");
        return PRO_TK_BAD_CONTEXT;
    }
    if((error=ProMdlMdlnameGet(assembly,source))||(error=qa_mm_per_unit(assembly,&mm)))return error;
    AxScan *scan=NULL;
    if((error=ax_scan(assembly,0,&scan)))return error;
    if(!scan->count){
        free(scan);
        part_text_add(out,L"这个装配里还没有可学习的组件实例。\n");
        return PRO_TK_E_NOT_FOUND;
    }

    wchar_t directory[MAX_PATH],path[MAX_PATH],temp[MAX_PATH];
    SYSTEMTIME time;
    GetLocalTime(&time);
    static unsigned serial;
    FILE *file=NULL;
    if(ax_learning_directory(directory,1)&&
       swprintf_s(path,MAX_PATH,L"%ls\\%04u%02u%02u-%02u%02u%02u-%03u-%08lx-%u%ls",directory,
           time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,
           GetCurrentProcessId(),++serial,AX_LEARNING_SUFFIX)>=0&&
       swprintf_s(temp,MAX_PATH,L"%ls.tmp",path)>=0)file=_wfopen(temp,L"wtx, ccs=UTF-8");
    if(!file){
        free(scan);
        part_text_add(out,L"无法在当前实例的数据目录写入装配方法库。\n");
        return PRO_TK_CANT_WRITE;
    }
    ax_learning_clean(source);
    fwprintf(file,L"%ls%ls\n",AX_LEARNING_HEADER,source);
    for(int i=0;i<scan->count;i++){
        const AxOccurrence *item=&scan->occurrences[i];
        if(item->type!=PRO_MDL_ASSEMBLY||!item->name[0])continue;
        int first=1;
        for(int j=0;j<i;j++){
            const AxOccurrence *previous=&scan->occurrences[j];
            if(previous->type==PRO_MDL_ASSEMBLY&&!_wcsicmp(item->name,previous->name)){first=0;break;}
        }
        if(first){
            ProFamilyMdlName model;
            wcscpy_s(model,_countof(model),item->name);
            ax_learning_clean(model);
            fwprintf(file,L"模块名 %ls\n",model);
        }
    }
    fwprintf(file,L"样件装配 %ls，按 Creo 模型树记录 %d 个实例。%ls\n",source,scan->count,
        scan->limited?L"实例数或层级达到扫描上限，以下是可读取部分。":L"");
    AxLearningStats stats={0};
    for(int i=0;i<scan->count&&!ferror(file);i++)ax_learning_occurrence(file,assembly,scan,i,mm,&stats);
    fwprintf(file,L"汇总：子装配 %d，实测姿态 %d，定位约束 %d，Creo 未报告欠约束 %d，未读取定位的实例 %d，读取缺项 %d。\n",
        stats.subassemblies,stats.poses,stats.constraints,stats.placed,stats.missing,stats.incomplete);
    int written=!ferror(file);
    if(fclose(file))written=0;
    if(!written||!MoveFileExW(temp,path,MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(temp);
        free(scan);
        part_text_add(out,L"装配案例没有写入成功，请核对当前实例的数据目录。\n");
        return PRO_TK_CANT_WRITE;
    }
    part_text_add(out,L"已将 %ls 学成独立装配案例。记录了 %d 个组件实例（含 %d 个子装配实例）、%d 个实测姿态和 %d 条定位约束。\n",
        source,scan->count,stats.subassemblies,stats.poses,stats.constraints);
    if(stats.missing||stats.incomplete||scan->limited)
        part_text_add(out,L"其中 %d 个实例的定位当前不可读取，另有 %d 处读取缺项。%ls下次装配时会按目标的实际参照重新核对。\n",
            stats.missing,stats.incomplete,scan->limited?L"装配树已达到读取上限。":L"");
    part_text_add(out,L"装配记忆已自动保存：%ls\n打开目标装配后，可以说「按上次学的装配方法，把需要的组件装起来」。\n",path);
    free(scan);
    return PRO_TK_NO_ERROR;
}
