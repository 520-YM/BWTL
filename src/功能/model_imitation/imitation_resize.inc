#define IMITATION_SIZE_TOL 0.002

static ProError imitation_extents(ProMdl model, double mm, double size[3]){
    Pro3dPnt box[2];
    ProError error=ProSolidOutlineGet((ProSolid)model,box);
    if(!error)for(int axis=0;axis<3;axis++)size[axis]=(box[1][axis]-box[0][axis])*mm;
    return error;
}

typedef struct {double mm,old_size,new_size;int old_count,new_count;} ImitationDiameterRead;

static ProError imitation_cylinder_diameter(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    ImitationDiameterRead *read=data;
    ProSrftype type;
    ProError error=ProSurfaceTypeGet(surface,&type);
    if(error||type!=PRO_SRF_CYL)return error;
    ProGeomitemdata *raw=NULL;
    error=ProSurfaceDataGet(surface,&raw);
    if(!error){
        double actual=2*fabs(raw->data.p_surface_data->srf_shape.cylinder.radius)*read->mm;
        read->old_count+=fabs(actual-read->old_size)<=IMITATION_SIZE_TOL;
        read->new_count+=fabs(actual-read->new_size)<=IMITATION_SIZE_TOL;
        ProGeomitemdataFree(&raw);
    }
    return error;
}

static ProError imitation_verify_diameters(ProMdl source,ProMdl copy,double mm,double old,double next,PartText *out){
    ImitationDiameterRead a={mm,old,next,0,0},b={mm,old,next,0,0};
    ProSolidBody body;
    ProError error=qa_single_body(source,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,imitation_cylinder_diameter,&a);
    if(!error)error=qa_single_body(copy,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,imitation_cylinder_diameter,&b);
    if(error)return error;
    int changed=a.old_count-b.old_count;
    if(changed<=0||b.new_count-a.new_count!=changed)return PRO_TK_INCOMPLETE;
    part_text_add(out,L"实际实体有 %d 个圆柱面从 Ø%.8g 变为 Ø%.8g mm。",changed,old,next);
    return PRO_TK_NO_ERROR;
}

/* Only test original, writable linear dimensions on the independent copy.
   A usable driver changes one envelope axis by exactly the trial increment. */
static ProError imitation_size_drivers(ProMdl copy,const PartSnapshot *snapshot,const double start[3],
                                        const double goal[3],int driver[3]){
    int count[3]={0},tested=0;
    const double mm=snapshot->mm_per_native;
    for(int i=0;i<snapshot->dimensions;i++){
        const PartDimension *d=&snapshot->dimension[i];
        if(d->type!=PRODIMTYPE_LINEAR||d->related||d->negative||d->value<=0)continue;
        ProDimension dim;
        if(ProModelitemInit(copy,d->handle.id,PRO_DIMENSION,&dim))continue;
        int candidate=0;
        for(int axis=0;axis<3;axis++)if(fabs(goal[axis]-start[axis])>IMITATION_SIZE_TOL&&
            fabs(d->value*mm-start[axis])<=IMITATION_SIZE_TOL)candidate=1;
        if(!candidate)continue;
        if(++tested>12)return PRO_TK_OUT_OF_RANGE;
        const double increment=fmax(0.05,d->value*mm*0.0025);
        double changed[3],restored[3];
        ProError trial=ProDimensionValueSet(&dim,d->value+increment/mm);
        if(trial)continue;
        trial=part_regenerate(copy);
        if(!trial)trial=imitation_extents(copy,mm,changed);
        ProError restore=ProDimensionValueSet(&dim,d->value);
        if(!restore)restore=part_regenerate(copy);
        if(!restore)restore=imitation_extents(copy,mm,restored);
        if(restore)return restore;
        for(int axis=0;axis<3;axis++)
            if(fabs(restored[axis]-start[axis])>IMITATION_SIZE_TOL)return PRO_TK_INCOMPLETE;
        if(trial)continue;
        for(int axis=0;axis<3;axis++){
            if(fabs(goal[axis]-start[axis])<=IMITATION_SIZE_TOL||
               fabs(changed[axis]-start[axis]-increment)>IMITATION_SIZE_TOL||
               fabs(changed[(axis+1)%3]-start[(axis+1)%3])>IMITATION_SIZE_TOL||
               fabs(changed[(axis+2)%3]-start[(axis+2)%3])>IMITATION_SIZE_TOL)continue;
            driver[axis]=i;
            count[axis]++;
        }
    }
    for(int axis=0;axis<3;axis++)
        if(fabs(goal[axis]-start[axis])>IMITATION_SIZE_TOL&&count[axis]!=1)return PRO_TK_INCOMPLETE;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_resize_command(const PartCommand *command,const PartSnapshot *snapshot,
                                         wchar_t *text,size_t capacity){
    DeliveryOutput work={0};
    PartSnapshot *rebuilt=NULL;
    PartText out={text,capacity,0};
    ProMdl source=NULL,copy=NULL;
    ProMdlName name;
    ProPath destination;
    double requested[3],before[3],after[3];
    double original_thickness=0,copy_thickness=0,unit=0;
    double requested_thickness=0,old_hole=0,new_hole=0;
    int hole_count=0,specified[3];
    wchar_t diameter_report[240]=L"";
    PartText diameter_out={diameter_report,_countof(diameter_report),0};
    int drivers[3]={-1,-1,-1};
    int steps=0,old_background=model_background_mode;
    const int sheet=snapshot->subtype==PROMDLSTYPE_PART_SHEETMETAL;
    const double mm=snapshot->mm_per_native;
    const wchar_t *failure=L"请先打开并激活要仿照的原生零件。";
    ProError error=part_check_binding(command,snapshot,&source);
    text[0]=0;
    if(error||snapshot->type!=PRO_MDL_PART||snapshot->in_assembly||!(mm>0)){
        if(!error)error=PRO_TK_BAD_CONTEXT;
        goto done;
    }
    for(int axis=0;axis<3;axis++){
        part_number(command->arg[axis+2],&requested[axis]);
        specified[axis]=requested[axis]>0;
    }
    for(int i=5;i<command->count;){
        if(!wcscmp(command->arg[i],L"THICK")){
            part_number(command->arg[i+1],&requested_thickness);
            i+=2;
        }else{
            part_number(command->arg[i+1],&old_hole);
            part_number(command->arg[i+2],&new_hole);
            i+=3;
        }
    }
    if(requested_thickness&&!sheet){
        failure=L"板厚调整需要原生钣金零件。";
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    failure=L"源件存在外部参照、族表依赖或失败特征，无法验证独立的改尺寸副本。";
    if((error=part_health(source))||(error=dv_no_external_dependencies(source)))goto done;
    failure=L"无法读取源件的实体外形或板厚。";
    if((error=qa_single_body(source,NULL))||(error=imitation_extents(source,mm,before)))goto done;
    if(sheet&&(error=fab_thickness(source,&unit,&original_thickness)))goto done;
    for(int axis=0;axis<3;axis++)if(!specified[axis])requested[axis]=before[axis];
    failure=L"未能创建独立工作副本，请核对 Creo 工作目录。";
    if((error=fab_scratch(&work))||
       (error=imitation_result_name(source,work.parent,name,destination))||
       (error=dv_stage_enter(&work)))goto done;
    failure=L"按源特征树从空白零件重建未完成。";
    model_background_mode=1;
    error=imitation_replay_new(source,snapshot->subtype,&copy,&steps,&out);
    model_background_mode=old_background;
    if(error)goto done;
    ImitationShapeMetrics base;
    if((error=imitation_material_replay(source,copy,snapshot->subtype,work.stage,&out))||
       (error=imitation_material_verify(source,copy,snapshot->subtype))||
       (error=imitation_shape_read(source,&base))||
       (error=imitation_shape_verify_mode(source,copy,&base,0,&out))||
       (error=ProMdlnameRename(copy,name)))goto done;
    rebuilt=calloc(1,sizeof(*rebuilt));
    if(!rebuilt){error=PRO_TK_OUT_OF_MEMORY;goto done;}
    rebuilt->mm_per_native=mm;
    error=ProSolidDimensionVisit((ProSolid)copy,PRO_B_FALSE,part_dimension_visit,NULL,rebuilt);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error&&rebuilt->truncated)error=PRO_TK_OUT_OF_RANGE;
    if(error)goto done;
    failure=L"无法唯一确定控制目标外形的驱动尺寸。请告诉我对应的特征或尺寸。";
    if((error=imitation_size_drivers(copy,rebuilt,before,requested,drivers)))goto done;
    failure=L"新尺寸未通过 Creo 再生或外形验收，请核对尺寸和特征关系。";
    for(int axis=0;axis<3;axis++)if(drivers[axis]>=0){
        const PartDimension *d=&rebuilt->dimension[drivers[axis]];
        ProDimension dimension;
        if((error=ProModelitemInit(copy,d->handle.id,PRO_DIMENSION,&dimension))||
           (error=ProDimensionValueSet(&dimension,d->value+(requested[axis]-before[axis])/mm)))goto done;
    }
    if(old_hole){
        failure=L"未找到指定孔径的可编辑切除尺寸，或该尺寸由关系式控制。";
        for(int i=0;i<rebuilt->dimensions;i++){
            const PartDimension *d=&rebuilt->dimension[i];
            if(d->type!=PRODIMTYPE_DIAMETER||fabs(d->value*mm-old_hole)>IMITATION_SIZE_TOL)continue;
            ProFeature feature;
            ProFeattype type;
            if(ProFeatureInit((ProSolid)copy,d->feature,&feature)||ProFeatureTypeGet(&feature,&type)||
               (type!=PRO_FEAT_HOLE&&type!=PRO_FEAT_CUT&&type!=PRO_FEAT_CUT_SMT))continue;
            ProDimension dimension;
            if(d->negative||d->related){error=PRO_TK_BAD_CONTEXT;goto done;}
            if((error=ProModelitemInit(copy,d->handle.id,PRO_DIMENSION,&dimension))||
               (error=ProDimensionValueSet(&dimension,new_hole/mm)))goto done;
            hole_count++;
        }
        if(!hole_count){error=PRO_TK_E_NOT_FOUND;goto done;}
    }
    if(requested_thickness&&fabs(original_thickness-requested_thickness)>0.0001){
        failure=L"副本板厚改动未通过原生回读。";
        if((error=smx_change_thickness(copy,original_thickness,requested_thickness,mm)))goto done;
    }
    failure=L"新尺寸未通过 Creo 再生或外形验收，请核对尺寸和特征关系。";
    if((error=part_regenerate(copy))||(error=qa_single_body(copy,NULL))||
       (error=imitation_extents(copy,mm,after))||(error=dv_no_external_dependencies(copy)))goto done;
    for(int axis=0;axis<3;axis++)if((specified[axis]||!requested_thickness)&&fabs(after[axis]-requested[axis])>IMITATION_SIZE_TOL){
        error=PRO_TK_INCOMPLETE;goto done;
    }
    if(sheet){
        error=fab_thickness(copy,&unit,&copy_thickness);
        if(!error&&fabs(copy_thickness-(requested_thickness?requested_thickness:original_thickness))>IMITATION_SIZE_TOL)
            error=PRO_TK_INCOMPLETE;
        if(error)goto done;
    }
    failure=L"孔径或圆弧半径回读与要求不符。";
    int changed_holes=0;
    for(int i=0;i<rebuilt->dimensions;i++){
        const PartDimension *d=&rebuilt->dimension[i];
        if(d->type!=PRODIMTYPE_DIAMETER&&d->type!=PRODIMTYPE_RADIUS)continue;
        ProDimension dimension;
        double actual;
        if((error=ProModelitemInit(copy,d->handle.id,PRO_DIMENSION,&dimension))||
           (error=ProDimensionValueGet(&dimension,&actual)))goto done;
        if(fabs(actual-d->value)*mm<=IMITATION_SIZE_TOL)continue;
        if(!old_hole||d->type!=PRODIMTYPE_DIAMETER||fabs(d->value*mm-old_hole)>IMITATION_SIZE_TOL||
           fabs(actual*mm-new_hole)>IMITATION_SIZE_TOL){error=PRO_TK_INCOMPLETE;goto done;}
        changed_holes++;
    }
    if(changed_holes!=hole_count){error=PRO_TK_INCOMPLETE;goto done;}
    if(hole_count&&(error=imitation_verify_diameters(source,copy,mm,old_hole,new_hole,&diameter_out)))goto done;
    failure=L"原件或工作目录在任务中发生变化。";
    if((error=ProDirectoryChange(work.parent)))goto done;
    work.in_stage=0;
    if((error=part_check_binding(command,snapshot,&source)))goto done;
done:
    free(rebuilt);
    if(error&&copy){
        if(ProMdlErase(copy))trace("Erase rejected size copy",error);
        copy=NULL;
    }
    if(error)part_text_add(&out,L"仿照改尺寸未完成。%ls（Creo %d）",failure,error);
    error=fab_discard(&work,error,&out);
    if(error)return error;
    error=dv_display_new(copy);
    if(error){
        part_text_add(&out,L"已在会话中生成 %ls.prt，但 Creo 未切换窗口。可从「在会话中」打开。\n",name);
        return error;
    }
    part_text_add(&out,L"从空白件按 %d 个步骤重建 %ls.prt，实测外形 %.8g × %.8g × %.8g mm。已通过再生。结果尚未保存。",
        steps,name,after[0],after[1],after[2]);
    if(sheet)part_text_add(&out,L"板厚 %.8g mm。",copy_thickness);
    if(hole_count)part_text_add(&out,L"已回读 %d 个切除驱动直径 %.8g → %.8g mm。",hole_count,old_hole,new_hole);
    part_text_add(&out,L"%ls",diameter_report);
    return PRO_TK_NO_ERROR;
}
