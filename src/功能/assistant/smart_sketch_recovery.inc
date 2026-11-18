static int smart_sketch_can_check(const PartCommand *command,const PartSnapshot *snapshot){
    if(smart_readonly||command->verb!=PART_FOUNDATION||wcscmp(command->arg[0],L"SKETCH_CREATE")||
       snapshot->type!=PRO_MDL_PART||snapshot->subtype==PROMDLSTYPE_PART_SHEETMETAL||
       snapshot->in_assembly||!snapshot->model||snapshot->model!=snapshot->window_model)return 0;
    ProMdl target=NULL;
    return !part_check_binding(command,snapshot,&target);
}

static int smart_recovery_same_part(const PartCommand *command,const PartSnapshot *snapshot,
                                    const SmartPartSignature *before){
    ProMdl active=NULL;
    ProMdlName name;
    ProPath directory;
    if(!before->hash||before->error||ProMdlCurrentGet(&active)||active!=snapshot->model||
       ProMdlMdlnameGet(active,name)||_wcsicmp(name,snapshot->name)||_wcsicmp(name,command->arg[1])||
       ProDirectoryCurrentGet(directory)||_wcsicmp(directory,snapshot->directory)||part_health(active))return 0;
    SmartPartSignature after=smart_part_signature(active);
    if(after.error||after.hash!=before->hash)return 0;
    if(snapshot->has_volume){
        ProMassProperty mass;
        if(qa_geometry_mass(active,&mass)||!part_close(mass.volume,snapshot->volume))return 0;
    }
    return 1;
}

static int smart_sketch_rechecked(const PartCommand *command,const PartSnapshot *snapshot,
                                  const SmartPartSignature *before,ProError error){
    if((error!=PRO_TK_BAD_INPUTS&&error!=PRO_TK_E_NOT_FOUND)||
       fd_sketch_rolled_back_model!=snapshot->model||fd_sketch_rolled_back_error!=error||
       !smart_recovery_same_part(command,snapshot,before))return 0;
    trace("Verified sketch rollback before replanning",error);
    return 1;
}

static int smart_smt_cut_capture(const PartCommand *command,const PartSnapshot *snapshot,double *thickness){
    if(smart_readonly||command->verb!=PART_FOUNDATION||wcscmp(command->arg[0],L"SMT_CUT")||
       snapshot->type!=PRO_MDL_PART||snapshot->subtype!=PROMDLSTYPE_PART_SHEETMETAL||
       snapshot->in_assembly||!snapshot->model||snapshot->model!=snapshot->window_model||!snapshot->has_volume)return 0;
    ProMdl target=NULL;
    ProDimension measure;
    return !part_check_binding(command,snapshot,&target)&&!ProSmtPartThicknessGet((ProPart)target,&measure)&&
        !ProDimensionValueGet(&measure,thickness)&&isfinite(*thickness)&&*thickness>0;
}

static int smart_smt_cut_rechecked(const PartCommand *command,const PartSnapshot *snapshot,
                                   const SmartPartSignature *before,double thickness,ProError error){
    if(thickness<=0||sh_cut_rolled_back_model!=snapshot->model||sh_cut_rolled_back_error!=error||
       !smart_recovery_same_part(command,snapshot,before))return 0;
    ProDimension measure;
    double current;
    if(ProSmtPartThicknessGet((ProPart)snapshot->model,&measure)||
       ProDimensionValueGet(&measure,&current)||!part_close(current,thickness))return 0;
    trace("Verified sheetmetal cut rollback before replanning",error);
    return 1;
}
