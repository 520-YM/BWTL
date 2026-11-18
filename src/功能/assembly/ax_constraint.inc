static ProError ax_csys_selection(ProMdl model,const wchar_t *token,int root,
    ProSelection *selection,ProCsysdata *frame){
    ProFeature feature;
    ProGeomitem geometry;
    ProCsys csys;
    ProGeomitemdata *raw=NULL;
    ProAsmcomppath path;
    ProIdTable ids={0};
    ProError error;
    if((error=ux_named_feature(model,token+5,&feature))||
       (error=fd_feature_geom(&feature,PRO_CSYS,&geometry))||
       (error=ProGeomitemToCsys(&geometry,&csys))||(error=ProCsysDataGet(csys,&raw)))return error;
    *frame=*raw->data.p_csys_data;
    ProGeomitemdataFree(&raw);
    if(root&&(error=ProAsmcomppathInit((ProSolid)model,ids,0,&path)))return error;
    return ProSelectionAlloc(root?&path:NULL,&geometry,selection);
}

static int ax_frames_aligned(ProCsysdata *root,ProCsysdata *part,ProMatrix pose,double scale){
    ProVector transformed;
    if(ProPntTrfEval(part->origin,pose,transformed)||!ad_near(root->origin,transformed,1e-5/scale))return 0;
    double *source[]={part->x_vector,part->y_vector,part->z_vector};
    double *target[]={root->x_vector,root->y_vector,root->z_vector};
    for(int i=0;i<3;i++)
        if(ProVectorTrfEval(source[i],pose,transformed)||!ad_near(target[i],transformed,1e-7))return 0;
    return 1;
}

#ifdef BWTL_ASSEMBLY_WORKBENCH_TEST
static int aw_test_failed_candidate;
#endif
static ProError ax_constrain(ProMdl assembly,const PartCommand *cmd,double scale,PartText *out){
    AxComponent component={0};
    ProCsysdata root_frame,part_frame;
    ProSelection root=NULL,part=NULL;
    ProAsmcompconstraint constraint=NULL,*constraints=NULL;
    ProMatrix previous,initial,actual;
    double part_scale;
    int count=0,attempted=0;
    ProError error;
#define AC_STEP(call) do{if((error=(call))){trace("assembly csys " #call,error);goto done;}}while(0)
    AC_STEP(ax_component_resolve(assembly,cmd->arg[2],&component));
    AC_STEP(ax_ordinary_occurrence(&component.feature,1));
    AC_STEP(ax_leaf_without_children(&component.feature));
    AC_STEP(ax_packaged(&component.feature));
    AC_STEP(qa_mm_per_unit(component.model,&part_scale));
    if(!part_close(scale,part_scale)){error=PRO_TK_BAD_CONTEXT;goto done;}
    AC_STEP(ax_actual_pose(assembly,&component.feature,previous));
    AC_STEP(ProAsmcompPositionGet(&component.feature,initial));
    AC_STEP(ax_csys_selection(assembly,cmd->arg[3],1,&root,&root_frame));
    AC_STEP(ax_csys_selection(component.model,cmd->arg[4],0,&part,&part_frame));
    AC_STEP(ProAsmcompconstraintAlloc(&constraint));
    AC_STEP(ProAsmcompconstraintTypeSet(constraint,PRO_ASM_CSYS));
    AC_STEP(ProAsmcompconstraintAsmreferenceSet(constraint,root,PRO_DATUM_SIDE_NONE));
    AC_STEP(ProAsmcompconstraintCompreferenceSet(constraint,part,PRO_DATUM_SIDE_NONE));
    AC_STEP(ProArrayAlloc(1,sizeof(*constraints),1,(ProArray *)&constraints));
    constraints[0]=constraint;
    attempted=1;
    AC_STEP(ProAsmcompConstraintsSet(NULL,&component.feature,constraints));
    AC_STEP(part_regenerate(assembly));
    AC_STEP(ax_constraint_count(&component.feature,&count));
    AC_STEP(ax_actual_pose(assembly,&component.feature,actual));
    if(count!=1||!ax_frames_aligned(&root_frame,&part_frame,actual,scale)){
        error=PRO_TK_GENERAL_ERROR;goto done;
    }
#ifdef BWTL_ASSEMBLY_WORKBENCH_TEST
    if(aw_test_failed_candidate){
        aw_test_failed_candidate=0;
        error=PRO_TK_BAD_INPUTS;
        goto done;
    }
#endif
    part_text_add(out,L"%ls 已建立坐标系重合约束。%ls 与 %ls 的原点和三个坐标轴已核对重合。\n",
        cmd->arg[2],cmd->arg[3],cmd->arg[4]);
done:
    if(error&&attempted){
        int unchanged=!ax_packaged(&component.feature)&&!ax_actual_pose(assembly,&component.feature,actual)&&
            ax_same_pose(previous,actual,scale)&&!part_health(assembly);
        if(!unchanged&&(ProAsmcompAllConstrRemove(&component.feature)||
           ax_set_position(assembly,&component.feature,initial)||ax_packaged(&component.feature)||
           ax_actual_pose(assembly,&component.feature,actual)||!ax_same_pose(previous,actual,scale)))ax_uncertain(out);
    }
    if(constraints)ProAsmcompconstraintArrayFree(constraints);
    else if(constraint)ProAsmcompconstraintFree(constraint);
    if(part)ProSelectionFree(&part);
    if(root)ProSelectionFree(&root);
    return error;
#undef AC_STEP
}
