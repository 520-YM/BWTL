typedef struct {
    wchar_t label[PRO_NAME_SIZE+12];
    int files,last_file;
} ImitationBatchBlocker;

typedef struct {
    ImitationBatchBlocker items[IMITATION_BATCH_MAX*12];
    int count;
} ImitationBatchBlockers;

static void imitation_batch_blocker(ImitationBatchBlockers *list,const wchar_t *label,int file){
    int i=0;
    while(i<list->count&&_wcsicmp(list->items[i].label,label))i++;
    if(i==list->count){
        if(i==_countof(list->items)||wcscpy_s(list->items[i].label,_countof(list->items[i].label),label))return;
        list->count++;
    }
    if(list->items[i].last_file!=file){
        list->items[i].last_file=file;
        list->items[i].files++;
    }
}

static void imitation_batch_missing_refs(ProMdl source,ImitationBatchBlockers *blockers,int file){
    ProMdlnameShortdata *names=NULL;
    ProMdlfileType *types=NULL;
    int count=0;
    ProError error=ProMdlDependenciesDataList(source,&names,&types,&count);
    if((!error||error==PRO_TK_BAD_CONTEXT)&&names&&types){
        for(int i=0;i<count;i++){
            const int part=types[i]==PRO_MDLFILE_PART;
            if(!names[i].name[0]||(!part&&types[i]!=PRO_MDLFILE_ASSEMBLY))continue;
            ProMdl model=NULL;
            if(ProMdlnameInit(names[i].name,part?PRO_MDL_PART:PRO_MDL_ASSEMBLY,&model)!=PRO_TK_E_NOT_FOUND)continue;
            wchar_t label[PRO_NAME_SIZE+12];
            if(swprintf_s(label,_countof(label),L"%ls.%ls",names[i].name,part?L"prt":L"asm")>=0)
                imitation_batch_blocker(blockers,label,file);
        }
    }
    if(names)ProArrayFree((ProArray *)&names);
    if(types)ProArrayFree((ProArray *)&types);
}

static void imitation_batch_blocker_report(ImitationBatchBlockers *list,PartText *out){
    if(!list->count)return;
    part_text_add(out,L"首个阻断原因和未加载的直接依赖，按涉及文件数排列。\n");
    for(int i=0;i<list->count&&i<12;i++){
        int largest=i;
        for(int j=i+1;j<list->count;j++)if(list->items[j].files>list->items[largest].files)largest=j;
        if(largest!=i){
            ImitationBatchBlocker item=list->items[largest];
            list->items[largest]=list->items[i];
            list->items[i]=item;
        }
        part_text_add(out,L"%ls，%d 个文件。\n",list->items[i].label,list->items[i].files);
    }
}

static ProError imitation_batch_preview_files(const wchar_t *files,PartText *out){
    ImitationFileSelection selection={0};
    ProError error=imitation_file_selection(files,&selection);
    if(error)return error;
    DeliveryOutput scratch={0};
    error=fab_scratch(&scratch);
    if(!error)error=dv_stage_enter(&scratch);
    if(error)return fab_discard(&scratch,error,out);

    ImitationBatchBlockers blockers={0};
    int ready=0;
    const wchar_t *leaf=selection.first;
    for(int i=0;i<selection.count;i++,leaf+=wcslen(leaf)+1){
        ProPath path=L"";
        const wchar_t *label=imitation_selection_label(leaf);
        ProMdl source=NULL;
        int opened=0,external=0;
        error=imitation_selection_path(&selection,leaf,path);
        if(!error)error=imitation_load_source(path,&source,&opened);
        if(error){
            const wchar_t *reason=error==PRO_TK_E_NOT_FOUND?L"源文件缺失或 Creo 无法定位":L"打开源文件";
            part_text_add(out,L"%ls，%ls（Creo %d）。\n",label,reason,error);
            imitation_batch_blocker(&blockers,reason,i+1);
            continue;
        }

        error=replay_source_dependencies(source,&external);
        if(error){
            part_text_add(out,L"%ls，外部参照或族表预检未通过（Creo %d）。\n",label,error);
            imitation_dependency_names(source,out);
            imitation_batch_blocker(&blockers,L"外部参照或族表预检",i+1);
            imitation_batch_missing_refs(source,&blockers,i+1);
        }else{
            ReplayPreview preview={0};
            error=ProSolidFeatVisit((ProSolid)source,replay_preview_feature,NULL,&preview);
            ProError cleanup=replay_pattern_source_close(&preview.patterns);
            if(!error)error=cleanup;
            if(error){
                part_text_add(out,L"%ls，特征树读取失败（Creo %d）。\n",label,error);
                imitation_batch_blocker(&blockers,L"特征树读取",i+1);
            }else if(preview.first_block_step){
                part_text_add(out,L"%ls，第 %d 步 %ls%ls%ls 需处理，源特征 %d。\n",
                    label,preview.first_block_step,preview.first_block_operation,
                    preview.first_block_name[0]?L" ":L"",preview.first_block_name,preview.first_block_id);
                imitation_batch_blocker(&blockers,preview.first_block_operation,i+1);
            }else{
                ready++;
                part_text_add(out,L"%ls，通过预检，%d 道建模步骤。\n",label,preview.top);
            }
        }
        if(opened)ProMdlErase(source);
    }
    imitation_batch_blocker_report(&blockers,out);
    part_text_add(out,L"批量预检 %d / %d 个可尝试重建。\n",ready,selection.count);
    return fab_discard(&scratch,ready==selection.count?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE,out);
}

static ProError imitation_batch_preview_run(PartText *out){
    wchar_t files[8192]=L"";
    ProError error=imitation_choose(files,_countof(files),L"选择要预检的 Creo 零件（最多 16 个）",1);
    return error?error:imitation_batch_preview_files(files,out);
}

