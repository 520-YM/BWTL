static ProError ax_load_part(const wchar_t *name,ProMdl *part,int *loaded_here){
    return ax_load_native(name,PRO_MDL_PART,part,loaded_here);
}
static ProError ax_default_set(ProAsmcomp *feature){
    ProAsmcompconstraint *rows=NULL;
    ProError error=ProArrayAlloc(0,sizeof(*rows),1,(ProArray *)&rows);
    if(!error)error=ax_add_relation(&rows,NULL,NULL,NULL,NULL,PRO_ASM_DEF_PLACEMENT,0,NULL,NULL);
    if(!error)error=ProAsmcompConstraintsSet(NULL,feature,rows);
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}

static ProError ax_default_verify(ProAsmcomp *feature){
    ProAsmcompconstraint *rows=NULL;
    ProAsmcompConstrType type;
    ProFeatStatus state;
    ProBoolean packaged,under;
    int count;
    ProError error=ax_read_constraints(feature,NULL,&rows,&count);
    if(!error&&count!=1)error=PRO_TK_GENERAL_ERROR;
    if(!error)error=ProAsmcompconstraintTypeGet(rows[0],&type);
    if(!error)error=ProFeatureStatusGet(feature,&state);
    if(!error)error=ProAsmcompIsPackaged(feature,&packaged);
    if(!error)error=ProAsmcompIsUnderconstrained(feature,&under);
    if(!error&&(type!=PRO_ASM_DEF_PLACEMENT||state!=PRO_FEAT_ACTIVE||packaged||under))
        error=PRO_TK_GENERAL_ERROR;
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}

enum {AX_PLACE_FREE,AX_PLACE_DEFAULT,AX_PLACE_FIXED};

static int ax_initial_pose(const PartCommand *cmd,int first,double mm,ProMatrix pose){
    const ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    memcpy(pose,identity,sizeof(identity));
    if(cmd->count==first||!wcscmp(cmd->arg[first],L"DEFAULT"))return AX_PLACE_DEFAULT;
    for(int i=0;i<3;i++)pose[3][i]=as_arg(cmd,first+i)/mm;
    if(cmd->count>=first+6){
        const double radians=acos(-1.0)/180.0;
        /* Apply X, Y, then Z rotations about the parent coordinate axes. */
        for(int axis=0;axis<3;axis++){
            const double a=as_arg(cmd,first+3+axis)*radians;
            int x=(axis+1)%3,y=(axis+2)%3;
            double c=cos(a),s=sin(a);
            for(int row=0;row<3;row++){
                double before_x=pose[row][x],before_y=pose[row][y];
                pose[row][x]=c*before_x-s*before_y;
                pose[row][y]=s*before_x+c*before_y;
            }
        }
    }
    return !wcscmp(cmd->arg[cmd->count-1],L"FIX")?AX_PLACE_FIXED:AX_PLACE_FREE;
}

static ProError ax_fixed_verify(ProAsmcomp *feature){
    ProAsmcompconstraint *rows=NULL;
    ProAsmcompConstrType type;
    ProBoolean packed=PRO_B_TRUE,under=PRO_B_TRUE;
    int count=0;
    ProError error=ax_read_constraints(feature,NULL,&rows,&count);
    if(!error&&count!=1)error=PRO_TK_GENERAL_ERROR;
    if(!error)error=ProAsmcompconstraintTypeGet(rows[0],&type);
    if(!error&&type!=PRO_ASM_FIX)error=PRO_TK_GENERAL_ERROR;
    if(!error)error=ProAsmcompIsPackaged(feature,&packed);
    if(!error)error=ProAsmcompIsUnderconstrained(feature,&under);
    if(!error&&(packed||under))error=PRO_TK_GENERAL_ERROR;
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}

static ProError ax_place_model(ProMdl assembly,ProMdl model,ProMatrix wanted,int mode,double mm,
    ProAsmcomp *placed,PartText *out){
    ProAsmcomp occurrence={0};
    ProAsmcompconstraint *rows=NULL;
    ProMdl actual_model=NULL;
    ProMatrix actual;
    ProFeatStatus state;
    double model_mm;
    if(placed)memset(placed,0,sizeof(*placed));
    ProError error=qa_mm_per_unit(model,&model_mm);
    if(!error&&!part_close(model_mm,mm))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)model,wanted,&occurrence);
    if(!error&&mode==AX_PLACE_DEFAULT)error=ax_default_set(&occurrence);
    if(!error&&mode==AX_PLACE_FIXED){
        error=ProArrayAlloc(0,sizeof(*rows),1,(ProArray *)&rows);
        if(!error)error=ax_add_relation(&rows,NULL,NULL,NULL,NULL,PRO_ASM_FIX,0,NULL,NULL);
        if(!error)error=ProAsmcompConstraintsSet(NULL,&occurrence,rows);
        if(rows)ProAsmcompconstraintArrayFree(rows);
    }
    if(!error)error=part_regenerate(assembly);
    if(!error)error=part_health(assembly);
    if(!error)error=ProAsmcompMdlGet(&occurrence,&actual_model);
    if(!error)error=ProFeatureStatusGet(&occurrence,&state);
    if(!error&&(actual_model!=model||state!=PRO_FEAT_ACTIVE))error=PRO_TK_GENERAL_ERROR;
    if(!error)error=ax_actual_pose(assembly,&occurrence,actual);
    if(!error){
        if(mode==AX_PLACE_DEFAULT)error=ax_default_verify(&occurrence);
        else if(mode==AX_PLACE_FIXED)error=ax_fixed_verify(&occurrence);
        else error=ax_packaged(&occurrence);
    }
    if(!error&&!ax_same_pose(wanted,actual,mm))error=PRO_TK_GENERAL_ERROR;
    if(error&&occurrence.owner==assembly&&occurrence.id>0){
        if(ax_leaf_without_children(&occurrence)||model_remove_feature((ProSolid)assembly,occurrence.id)||part_regenerate(assembly))
            ax_uncertain(out);
    }
    if(!error&&placed)*placed=occurrence;
    return error;
}

static ProError ax_insert(ProMdl assembly,const PartCommand *cmd,double scale,PartText *out){
    ProMdl model=NULL;
    ProAsmcomp feature={0};
    ProMassProperty geometry;
    int loaded_here=0;
    ProMatrix wanted;
    const int mode=ax_initial_pose(cmd,3,scale,wanted);
    ProError error=ax_load_part(cmd->arg[2],&model,&loaded_here);
    if(!error)error=part_health(model);
    if(!error)error=qa_geometry_mass(model,&geometry);
    if(!error)error=ax_place_model(assembly,model,wanted,mode,scale,&feature,out);
#ifdef BWTL_COMPANY_SOURCE_BINDING
    if(!error)company_source_note_placement(model);
#endif
    if(error){
        if(error==PRO_TK_E_NOT_FOUND)
            part_text_add(out,L"未找到 %ls.prt。已查询当前会话、工作目录和本实例的公司标准件库。\n",cmd->arg[2]);
        if(error==PRO_TK_E_FOUND)
            part_text_add(out,L"公司标准件库的多个产品目录都有 %ls.prt。请明确完整路径或唯一版本，重新读取后再装入。\n",cmd->arg[2]);
        if(error==PRO_TK_E_IN_USE)
            part_text_add(out,L"所选零件版本无法核对，或 Creo 中的同名模型已被修改。请核对模型来源后重试。\n");
        if(loaded_here&&!ax_recovery_uncertain)ProMdlErase(model);
        return error;
    }
    if(mode==AX_PLACE_DEFAULT)
        part_text_add(out,L"已用 Creo 原生默认定位装入 %ls，组件 ID:%d。已核对零件身份、定位约束及原点和方向。未自动保存。\n",
            cmd->arg[2],feature.id);
    else
        part_text_add(out,L"已放入 %ls，组件 ID:%d，%ls，位置 (%.9g, %.9g, %.9g) mm。实际位置和方向已核对。\n",
            cmd->arg[2],feature.id,mode==AX_PLACE_FIXED?L"固定":L"未约束初始位置",
            wanted[3][0]*scale,wanted[3][1]*scale,wanted[3][2]*scale);
    return PRO_TK_NO_ERROR;
}