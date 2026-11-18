typedef struct {
    int id;
    const wchar_t *name;
    int rows;
} CadMountDefinition;

static CadMountDefinition cad_electronics={54,NULL,3};
static CadMountDefinition cad_fasteners[]={
    {40,NULL,2},{41,NULL,2},{43,NULL,2},{46,NULL,2}
};

typedef struct {
    ProMdl target;
    ProModelitem original,geometric,trimmed;
    AxGeometry geometry;
    ReplayTrimmedFace samples;
    int geometric_count,trimmed_count;
} CadMountFace;

static ProError cad_mount_face_visit(ProSurface face,ProError status,ProAppData data){
    CadMountFace *search=data;
    ProModelitem item;
    AxGeometry test;
    if(status||ProSurfaceToGeomitem((ProSolid)search->target,face,&item)||
       ax_geometry(&item,&test)||test.type!=search->geometry.type)return PRO_TK_NO_ERROR;
    ProVector delta;
    for(int i=0;i<3;i++)delta[i]=test.point[i]-search->geometry.point[i];
    double cosine=ad_dot(test.direction,search->geometry.direction);
    if(fabs(cosine)<1-1e-7)return PRO_TK_NO_ERROR;
    if(test.type==AX_GEOM_CYLINDER){
        ProVector perpendicular;
        ad_cross(delta,test.direction,perpendicular);
        if(fabs(test.radius-search->geometry.radius)>0.0002||ad_dot(perpendicular,perpendicular)>4e-8)
            return PRO_TK_NO_ERROR;
    }else if(fabs(ad_dot(delta,test.direction))>0.0002)return PRO_TK_NO_ERROR;
    search->geometric=item;
    search->geometric_count++;
    if(search->samples.count&&replay_trimmed_face_matches(&item,&search->original,&search->samples)){
        search->trimmed=item;
        search->trimmed_count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError cad_mount_face(ImitationReplay *replay,ProModelitem *original,ProModelitem *mapped){
    ProReference ref=NULL;
    ProError error=ProReferenceAlloc(&ref);
    if(!error)error=ProReferenceSet(ref,NULL,original);
    if(!error)error=replay_reference_find(replay,ref,mapped,0);
    if(ref)ProReferenceFree(ref);
    if(!error)return PRO_TK_NO_ERROR;
    if(original->type!=PRO_SURFACE)return error;
    CadMountFace search={.target=replay->target,.original=*original};
    ProSurface surface;
    if((error=ax_geometry(original,&search.geometry))||(error=ProGeomitemToSurface(original,&surface)))return error;
    if(replay_trimmed_face_read(surface,(ProSolid)original->owner,&search.samples))search.samples.count=0;
    error=ProSolidSurfaceVisit((ProSolid)search.target,cad_mount_face_visit,NULL,&search);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    if(search.trimmed_count==1)*mapped=search.trimmed;
    else if(search.geometric_count==1)*mapped=search.geometric;
    else return search.geometric_count?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;
    return PRO_TK_NO_ERROR;
}

static ProError cad_mount_source(ProMdl source,ProMdl target,const CadMountDefinition *definition,
    ProAsmcomp *component,ProMdl *model,ProMatrix pose,double *source_mm,double *target_mm){
    ProMdlName name=L"";
    ProError error=ax_component_feature(source,definition->id,component);
    if(!error)error=ProAsmcompMdlGet(component,model);
    if(!error)error=ProMdlMdlnameGet(*model,name);
    if(!error&&_wcsicmp(name,definition->name))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=qa_mm_per_unit(source,source_mm);
    if(!error)error=qa_mm_per_unit(target,target_mm);
    if(!error)error=ax_actual_pose(source,component,pose);
    if(!error)for(int i=0;i<3;i++)pose[3][i]*=*source_mm / *target_mm;
    return error;
}

static ProError cad_place_electronics(ProMdl source_asm,ProMdl target_asm,ProAsmcomp *electronics,PartText *out){
    ProAsmcomp original;
    ProMdl model=NULL;
    ProMatrix pose;
    double source_mm=0,target_mm=0;
    ProError error=cad_mount_source(source_asm,target_asm,&cad_electronics,&original,&model,pose,&source_mm,&target_mm);
    if(!error)error=ax_place_model(target_asm,model,pose,AX_PLACE_FIXED,target_mm,electronics,out);
    if(!error)part_text_add(out,L"已装入标准电子件 %ls，源实例 %d → 新实例 %d；加工孔前按源位姿临时固定。\n",
        cad_electronics.name,cad_electronics.id,electronics->id);
    return error;
}

static ProError cad_mount_rows(ProAsmcomp *original,ProAsmcomppath *source_root,ProAsmcomppath *bracket_path,
    ProMdl model,ImitationReplay *replay,int expected,double offset_scale,ProAsmcompconstraint **wanted,PartText *out){
    ProAsmcompconstraint *prior=NULL;
    int count=0;
    ProError error=ax_read_constraints(original,source_root,&prior,&count);
    if(!error&&count!=expected)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProArrayAlloc(0,sizeof(**wanted),count,(ProArray *)wanted);
    for(int i=0;!error&&i<count;i++){
        ProAsmcompconstraint row=NULL;
        ProAsmcompConstrType type;
        ProSelection base=NULL,child=NULL,new_base=NULL,new_child=NULL;
        ProDatumside base_side,child_side;
        ProModelitem base_item={0},child_item={0},mapped={0};
        int flags=0;
        double offset=0;
        error=ProAsmcompconstraintTypeGet(prior[i],&type);
        if(!error&&type!=PRO_ASM_INSERT&&type!=PRO_ASM_ALIGN&&type!=PRO_ASM_MATE&&!ax_offset_type(type))
            error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProAsmcompconstraintAttributesGet(prior[i],&flags);
        if(!error)error=ProAsmcompconstraintAsmreferenceGet(prior[i],&base,&base_side);
        if(!error)error=ProAsmcompconstraintCompreferenceGet(prior[i],&child,&child_side);
        if(!error)error=ProSelectionModelitemGet(base,&base_item);
        if(!error)error=ProSelectionModelitemGet(child,&child_item);
        if(!error&&(base_item.owner!=replay->source||child_item.owner!=model))error=PRO_TK_BAD_CONTEXT;
        if(!error)error=cad_mount_face(replay,&base_item,&mapped);
        if(!error){
            AxGeometry before,after;
            if(base_side&&!ax_geometry(&base_item,&before)&&!ax_geometry(&mapped,&after)&&
               before.type==AX_GEOM_PLANE&&after.type==AX_GEOM_PLANE&&ad_dot(before.direction,after.direction)<-0.999999)
                base_side=(ProDatumside)-base_side;
        }
        if(!error)error=ProSelectionAlloc(bracket_path,&mapped,&new_base);
        if(!error)error=ProSelectionAlloc(NULL,&child_item,&new_child);
        if(!error)error=ProAsmcompconstraintAlloc(&row);
        if(!error)error=ProAsmcompconstraintTypeSet(row,type);
        if(!error)error=ProAsmcompconstraintAsmreferenceSet(row,new_base,base_side);
        if(!error)error=ProAsmcompconstraintCompreferenceSet(row,new_child,child_side);
        if(!error&&ax_offset_type(type))error=ProAsmcompconstraintOffsetGet(prior[i],&offset);
        if(!error&&ax_offset_type(type))error=ProAsmcompconstraintOffsetSet(row,offset*offset_scale);
        if(!error&&flags)error=ProAsmcompconstraintAttributesSet(row,flags);
        if(!error)error=ProArrayObjectAdd((ProArray *)wanted,PRO_VALUE_UNUSED,1,&row);
        if(new_base)ProSelectionFree(&new_base);
        if(new_child)ProSelectionFree(&new_child);
        if(error){
            if(row)ProAsmcompconstraintFree(row);
            part_text_add(out,L"源实例 %d 的第 %d 条约束未重建。钣金参照 %d，组件参照 %d，Creo %d。\n",
                original->id,i+1,base_item.id,child_item.id,error);
        }
    }
    if(prior)ProAsmcompconstraintArrayFree(prior);
    return error;
}

static ProError cad_mount_bind(ProMdl source,ProMdl target,ProAsmcomppath *bracket_path,
    ProAsmcomp *component,const CadMountDefinition *definition,ImitationReplay *replay,PartText *out){
    ProAsmcomp original;
    ProAsmcomppath old_root,new_root;
    ProAsmcompconstraint *wanted=NULL;
    ProMdl model=NULL,readback=NULL;
    ProMatrix pose,actual;
    ProBoolean old_under=PRO_B_TRUE,new_under=PRO_B_TRUE,packaged=PRO_B_TRUE;
    double source_mm=0,target_mm=0;
    int attempted=0,staged=component->id>0;
    const wchar_t *stage=L"读取源实例和位姿";
    ProError error=cad_mount_source(source,target,definition,&original,&model,pose,&source_mm,&target_mm);
#define CAD_MOUNT_STEP(label,call) do{stage=(label);if((error=(call)))goto done;}while(0)
    if(error)goto done;
    CAD_MOUNT_STEP(L"读取源约束状态",ProAsmcompIsUnderconstrained(&original,&old_under));
    CAD_MOUNT_STEP(L"读取源装配参照路径",ax_root_path(source,&old_root));
    CAD_MOUNT_STEP(L"读取新装配参照路径",ax_root_path(target,&new_root));
    CAD_MOUNT_STEP(L"把源约束映射到新钣金特征",cad_mount_rows(&original,&old_root,bracket_path,model,replay,
        definition->rows,source_mm/target_mm,&wanted,out));
    if(!staged)CAD_MOUNT_STEP(L"加入标准件",ProAsmcompAssemble((ProAssembly)target,(ProSolid)model,pose,component));
    attempted=1;
    CAD_MOUNT_STEP(L"建立原生约束",ProAsmcompConstraintsSet(NULL,component,wanted));
    CAD_MOUNT_STEP(L"再生新装配",part_regenerate(target));
    CAD_MOUNT_STEP(L"回读约束类型、定位参照和偏距",ax_compare_constraints(component,&new_root,wanted,definition->rows,target_mm));
    CAD_MOUNT_STEP(L"回读新实例模型",ProAsmcompMdlGet(component,&readback));
    CAD_MOUNT_STEP(L"回读新实例实际位姿",ax_actual_pose(target,component,actual));
    CAD_MOUNT_STEP(L"核对新实例装配状态",ProAsmcompIsPackaged(component,&packaged));
    CAD_MOUNT_STEP(L"核对约束自由度",ProAsmcompIsUnderconstrained(component,&new_under));
    if(readback!=model||packaged||(!old_under&&new_under)||!ax_same_pose(pose,actual,target_mm)){
        error=PRO_TK_INCOMPLETE;
        stage=L"对比源实例的实际位姿和定位状态";
        part_text_add(out,L"源位姿原点 %.8g, %.8g, %.8g；新位姿原点 %.8g, %.8g, %.8g。\n",
            pose[3][0],pose[3][1],pose[3][2],actual[3][0],actual[3][1],actual[3][2]);
    }
done:
#undef CAD_MOUNT_STEP
    if(wanted)ProAsmcompconstraintArrayFree(wanted);
    if(!error){
        part_text_add(out,L"%ls：源实例 %d → 新实例 %d，%d 条原生约束、实际位姿和装配状态已回读。\n",
            definition->name,definition->id,component->id,definition->rows);
        return PRO_TK_NO_ERROR;
    }
    part_text_add(out,L"%ls 源实例 %d 在「%ls」未完成，Creo %d。\n",definition->name,definition->id,stage,error);
    if(component->id>0&&(!staged||attempted)){
        ax_discard_occurrence(target,component,out);
        memset(component,0,sizeof(*component));
        if(staged&&!ax_recovery_uncertain){
            ProError restored=cad_place_electronics(source,target,component,out);
            if(restored)part_text_add(out,L"电子件的临时定位恢复失败，Creo %d。\n",restored);
        }
    }else if(staged)part_text_add(out,L"电子件仍处于源位姿的临时固定状态，原参照约束待重建。\n");
    return error;
}

static ProError cad_finish_mounts(ProMdl source_asm,ProMdl target_asm,ProAsmcomp *bracket,
    ProAsmcomp *electronics,ImitationReplay *replay,PartText *out){
    ProAsmcomp old_bracket;
    ProMdl original=NULL,rebuilt=NULL;
    ProAsmcomppath path;
    ProIdTable ids={bracket->id};
    ProError error=ax_component_feature(source_asm,39,&old_bracket);
    if(!error)error=ProAsmcompMdlGet(&old_bracket,&original);
    if(!error)error=ProAsmcompMdlGet(bracket,&rebuilt);
    if(!error&&(replay->source!=original||replay->target!=rebuilt))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProAsmcomppathInit((ProSolid)target_asm,ids,1,&path);
    if(error)return error;
    ProError first=cad_mount_bind(source_asm,target_asm,&path,electronics,&cad_electronics,replay,out);
    for(int i=0;i<_countof(cad_fasteners)&&!ax_recovery_uncertain;i++){
        ProAsmcomp fastener={0};
        error=cad_mount_bind(source_asm,target_asm,&path,&fastener,&cad_fasteners[i],replay,out);
        if(error&&!first)first=error;
    }
    return first;
}
