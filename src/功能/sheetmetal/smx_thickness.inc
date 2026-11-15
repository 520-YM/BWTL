
typedef struct {
    double thickness,mm,bounds[3],low[3],high[3],volume,flat[2];
    int bends;
} SmxMeasures;

static ProError smx_measures(ProMdl model,const wchar_t *fixed,SmxMeasures *m){
    ProOutline outline;
    ProMassProperty mass;
    ProError error=fab_thickness(model,&m->mm,&m->thickness);
    if(!error)error=ProSolidOutlineGet((ProSolid)model,outline);
    if(!error)error=qa_geometry_mass(model,&mass);
    if(error)return error;
    for(int i=0;i<3;i++){
        m->low[i]=outline[0][i]*m->mm;
        m->high[i]=outline[1][i]*m->mm;
        m->bounds[i]=m->high[i]-m->low[i];
    }
    m->volume=mass.volume*m->mm*m->mm*m->mm;
    FabBends bends={0};
    bends.model=model;bends.mm=m->mm;bends.thickness=m->thickness;
    error=ProSolidSurfaceVisit((ProSolid)model,fab_bend_visit,NULL,&bends);
    if(error)return error;
    m->bends=bends.count;
    if(!fixed)return PRO_TK_NO_ERROR;
    DeliveryOutput work={0};
    PartText details={0};
    wchar_t message[1024]=L"";
    details.text=message;details.size=_countof(message);
    error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_native_flat(model,L"flat",fixed);
    ProPath file;
    if(!error)error=fab_leaf_path(work.stage,L"flat",L".dxf",file);
    FabDxf *dxf=NULL;
    if(!error){
        dxf=calloc(1,sizeof(*dxf));
        if(!dxf)error=PRO_TK_OUT_OF_MEMORY;
    }
    if(!error)error=fab_dxf_read(file,dxf);
    if(!error)for(int i=0;i<2;i++)m->flat[i]=dxf->high[i]-dxf->low[i];
    free(dxf);
    return fab_discard(&work,error,&details);
}

static ProError smx_change_thickness(ProMdl model,double expected,double value,double mm){
    ProDimension dim;
    ProBoolean related;
    double old,actual;
    ProError error=ProSmtPartThicknessGet((ProPart)model,&dim);
    if(!error)error=ProDimensionIsReldriven(&dim,&related);
    if(!error&&related)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProDimensionValueGet(&dim,&old);
    if(!error&&fabs(old*mm-expected)>0.0001)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProDimensionValueSet(&dim,value/mm);
    if(!error)error=part_regenerate(model);
    if(!error)error=part_health(model);
    if(!error)error=ProDimensionValueGet(&dim,&actual);
    if(!error&&fabs(actual*mm-value)>0.0001)error=PRO_TK_GENERAL_ERROR;
    return error;
}

static ProError smx_copy_scratch(ProMdl model,ProMdl *copy){
    static LONG serial;
    ProMdlName name;
    swprintf_s(name,_countof(name),L"bwtlsm_%08lx_%08lx",GetCurrentProcessId(),InterlockedIncrement(&serial));
    return ProMdlnameCopy(model,name,copy);
}

static void smx_measures_report(const SmxMeasures *old,const SmxMeasures *now,PartText *out){
    part_text_add(out,L"板厚 %.8g → %.8g mm；折后 X/Y/Z 包络 %.8g/%.8g/%.8g → %.8g/%.8g/%.8g mm。\n",
        old->thickness,now->thickness,old->bounds[0],old->bounds[1],old->bounds[2],now->bounds[0],now->bounds[1],now->bounds[2]);
    part_text_add(out,L"原生展开 %.8g × %.8g → %.8g × %.8g mm；折弯面数 %d → %d；体积 %.8g → %.8g mm³。\n",
        old->flat[0],old->flat[1],now->flat[0],now->flat[1],old->bends,now->bends,old->volume,now->volume);
}

static ProError smx_thickness(ProMdl model,const PartCommand *cmd,int apply,PartText *out){
    const wchar_t *fixed=cmd->arg[apply?7:4];
    SmxMeasures old={0},trial={0},actual={0};
    double expected=as_arg(cmd,2),next=as_arg(cmd,3);
    int axis=apply?(int)(cmd->arg[4][0]-L'X'):0;
    double target=apply?as_arg(cmd,5):0,tol=apply?as_arg(cmd,6):0;
    ProError error=smx_measures(model,fixed,&old);
    if(error)return error;
    if(fabs(old.thickness-expected)>0.0001||fabs(next-expected)<0.0001){
        part_text_add(out,L"实测板厚 %.8g mm，请核对预期旧值和不同的新值。\n",old.thickness);
        return PRO_TK_BAD_INPUTS;
    }
    DeliveryOutput work={0};
    ProMdl copy=NULL;
    error=fab_scratch(&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=smx_copy_scratch(model,&copy);
    if(!error)error=smx_change_thickness(copy,expected,next,old.mm);
    if(!error)error=smx_measures(copy,fixed,&trial);
    if(copy){
        ProError erase=ProMdlErase(copy);
        if(!error)error=erase;
    }
    error=fab_discard(&work,error,out);
    if(error)return error;
    smx_measures_report(&old,&trial,out);
    part_text_add(out,L"孔边距、压铆适用范围和材料规则需要对应规则逐项复核。\n");
    if(!apply)return PRO_TK_NO_ERROR;
    if(fabs(trial.bounds[axis]-target)>tol||trial.bends!=old.bends){
        part_text_add(out,L"副本的 %ls 向包络 %.8g mm 未满足 %.8g ± %.8g mm，或折弯面数变化，原件未改。\n",
            cmd->arg[4],trial.bounds[axis],target,tol);
        return PRO_TK_BAD_CONTEXT;
    }
    error=smx_change_thickness(model,expected,next,old.mm);
    if(!error)error=smx_measures(model,fixed,&actual);
    if(!error&&(fabs(actual.bounds[axis]-target)>tol||actual.bends!=old.bends))error=PRO_TK_GENERAL_ERROR;
    if(error){
        ProDimension dimension;
        double value;
        ProError restore=ProSmtPartThicknessGet((ProPart)model,&dimension);
        if(!restore)restore=ProDimensionValueGet(&dimension,&value);
        if(!restore&&fabs(value*old.mm-expected)>0.0001)restore=smx_change_thickness(model,value*old.mm,expected,old.mm);
        SmxMeasures verified={0};
        if(!restore)restore=smx_measures(model,fixed,&verified);
        if(restore||fabs(verified.thickness-expected)>0.0001||verified.bends!=old.bends||
            !ad_near(old.bounds,verified.bounds,0.0001)||fabs(old.volume-verified.volume)>1e-5*fmax(1,old.volume)){
            sh_recovery_uncertain=1;
            part_text_add(out,L"原件恢复需要核对，本会话暂停钣金写入。\n");
        }else part_text_add(out,L"原件已恢复并复测原厚度、包络、折弯数及体积。\n");
        return error;
    }
    part_text_add(out,L"当前件板厚及 %ls 向包络 %.8g mm 已回读，展开核验通过。未保存。\n",cmd->arg[4],actual.bounds[axis]);
    return PRO_TK_NO_ERROR;
}

static ProError smx_frames(ProMdl model,const PartCommand *cmd,PartText *out){
    ProCsysdata form,flat;
    ProSelection a=NULL,b=NULL;
    double mm;
    ProError error=qa_mm_per_unit(model,&mm);
    if(!error)error=ax_csys_selection(model,cmd->arg[2],0,&a,&form);
    if(!error)error=ax_csys_selection(model,cmd->arg[3],0,&b,&flat);
    if(a)ProSelectionFree(&a);
    if(b)ProSelectionFree(&b);
    if(error)return error;
    SmxParameter values[]={{L"BWTL_FORM_CSYS",cmd->arg[2]+5},{L"BWTL_FLAT_CSYS",cmd->arg[3]+5}};
    error=smx_params(model,values,_countof(values),0,out);
    if(error)return error;
    ProVector delta;
    ad_sub(flat.origin,form.origin,delta);
    double *source[]={flat.x_vector,flat.y_vector,flat.z_vector};
    double *basis[]={form.x_vector,form.y_vector,form.z_vector};
    part_text_add(out,L"成品和展开坐标系标识已登记。展开原点在成品坐标中 (%.8g, %.8g, %.8g) mm。\n",
        ad_dot(delta,basis[0])*mm,ad_dot(delta,basis[1])*mm,ad_dot(delta,basis[2])*mm);
    for(int i=0;i<3;i++)part_text_add(out,L"展开 %lc 轴在成品坐标中 (%.8g, %.8g, %.8g)。\n",L'X'+i,
        ad_dot(source[i],basis[0]),ad_dot(source[i],basis[1]),ad_dot(source[i],basis[2]));
    part_text_add(out,L"坐标系关系为折后当前定义。模型未保存。\n");
    return PRO_TK_NO_ERROR;
}

static ProError smx_grain(ProMdl model,const PartCommand *cmd,PartText *out){
    CabinetEdge edge;
    InspectionPlane plane;
    double mm,thickness;
    ProError error=fab_thickness(model,&mm,&thickness);
    if(!error)error=in_plane(model,cmd->arg[2],&plane);
    if(!error)error=cm_edge_get(model,cmd->arg[3],&edge);
    if(error)return error;
    int face;
    if(!part_integer(cmd->arg[2]+8,&face)||face!=edge.face)return PRO_TK_BAD_CONTEXT;
    wchar_t direction[96];
    swprintf_s(direction,_countof(direction),L"%.10g %.10g %.10g",edge.axis[0],edge.axis[1],edge.axis[2]);
    SmxParameter values[]={{L"BWTL_FINISH_FACE",cmd->arg[2]},{L"BWTL_GRAIN_EDGE",cmd->arg[3]},
        {L"BWTL_GRAIN_VECTOR",direction},{L"BWTL_FACE_FINISH",cmd->arg[4]},{L"BWTL_FACE_FILM",cmd->arg[5]}};
    error=smx_params(model,values,_countof(values),0,out);
    if(!error)part_text_add(out,L"已登记外观面 %ls、同面纹向边 %ls、单位向量 (%ls) 和覆膜状态 %ls。模型未保存。\n",
        cmd->arg[2],cmd->arg[3],direction,cmd->arg[5]);
    return error;
}
