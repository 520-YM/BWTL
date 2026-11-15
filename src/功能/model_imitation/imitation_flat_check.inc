typedef struct {ProMdl model;int id;double area,mm;int tied;} ImitationFlatFace;

static const wchar_t *imitation_flat_layer(int layer)
{
    return layer==1?L"外轮廓":layer==2?L"孔槽":L"折弯线";
}

static ProError imitation_flat_face(ProSurface surface,ProError status,ProAppData data)
{
    if(status)return status;
    ImitationFlatFace *face=data;
    ProSmtSurfType side;
    ProSrftype type;
    ProError error=ProSmtSurfaceTypeGet((ProPart)face->model,surface,&side);
    if(error||side!=PRO_SMT_SURF_FACE)return error;
    error=ProSurfaceTypeGet(surface,&type);
    if(error||type!=PRO_SRF_PLANE)return error;
    double area;
    error=ProSurfaceAreaEval(surface,&area);
    if(error)return error;
    area*=face->mm*face->mm;
    if(area>face->area+0.001){
        face->area=area;
        face->tied=0;
        return ProSurfaceIdGet(surface,&face->id);
    }
    if(fabs(area-face->area)<=0.001)face->tied=1;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_flat_compare(ProMdl source,ProMdl target,PartText *out)
{
    ProMdl models[]={source,target};
    wchar_t tokens[2][48];
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<2;i++){
        ProMdlsubtype subtype;
        error=ProMdlSubtypeGet(models[i],&subtype);
        if(error)return error;
        if(subtype!=PROMDLSTYPE_PART_SHEETMETAL)return PRO_TK_BAD_CONTEXT;
        ImitationFlatFace face={.model=models[i]};
        error=qa_mm_per_unit(models[i],&face.mm);
        if(!error)error=ProSolidSurfaceVisit((ProSolid)models[i],imitation_flat_face,NULL,&face);
        if(!error&&(!face.id||face.tied)){
            part_text_add(out,L"%ls的最大平面板壁无法唯一确定，请先使用钣金下料功能指定固定面。\n",i?L"结果件":L"原件");
            error=PRO_TK_E_AMBIGUOUS;
        }
        if(!error)swprintf_s(tokens[i],_countof(tokens[i]),L"SURFACE:%d",face.id);
    }
    if(error)return error;
    FabDxf *flat[2]={calloc(1,sizeof(FabDxf)),calloc(1,sizeof(FabDxf))};
    DeliveryOutput work={0};
    ProPath path;
    if(!flat[0]||!flat[1])error=PRO_TK_OUT_OF_MEMORY;
    if(!error)error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    for(int i=0;!error&&i<2;i++){
        error=fab_flat_read(models[i],work.stage,i?L"rebuilt_flat":L"source_flat",tokens[i],flat[i],path);
        if(error)part_text_add(out,L"%ls以 %ls 固定展开失败，Creo %d。\n",i?L"结果件":L"原件",tokens[i],error);
    }
    if(!error){
        unsigned char used[FAB_DXF_MAX]={0};
        int differences=0;
        for(int i=0;i<flat[0]->count;i++){
            const FabEntity *entity=&flat[0]->entities[i];
            int j=0;
            while(j<flat[1]->count&&(used[j]||!fab_same_entity(entity,&flat[1]->entities[j],0.001)))j++;
            if(j<flat[1]->count)used[j]=1;
            else if(++differences<=8){
                const double *p=entity->type==1?entity->a:entity->center;
                part_text_add(out,L"原件展开的%ls在 (%.6g, %.6g) mm 附近无对应图元。\n",
                    imitation_flat_layer(entity->layer),p[0],p[1]);
            }
        }
        for(int j=0;j<flat[1]->count;j++)if(!used[j]&&++differences<=8){
            const FabEntity *entity=&flat[1]->entities[j];
            const double *p=entity->type==1?entity->a:entity->center;
            part_text_add(out,L"结果件展开的%ls在 (%.6g, %.6g) mm 附近无对应图元。\n",
                imitation_flat_layer(entity->layer),p[0],p[1]);
        }
        part_text_add(out,L"以最大平面板壁 %ls / %ls 固定展开。下料包络 %.6g × %.6g / %.6g × %.6g mm，外轮廓 %d / %d、孔槽 %d / %d、折弯线 %d / %d，未匹配图元 %d 个。\n",
            tokens[0],tokens[1],flat[0]->high[0]-flat[0]->low[0],flat[0]->high[1]-flat[0]->low[1],
            flat[1]->high[0]-flat[1]->low[0],flat[1]->high[1]-flat[1]->low[1],flat[0]->outline,flat[1]->outline,
            flat[0]->holes,flat[1]->holes,flat[0]->bends,flat[1]->bends,differences);
        if(differences)error=PRO_TK_INCOMPLETE;
    }
    free(flat[0]);
    free(flat[1]);
    return fab_discard(&work,error,out);
}
