#define AX_PLACEMENT_LIMIT 12

#ifdef BWTL_PLACEMENT_FAULT_TEST
static int ax_placement_fault;
#endif

static ProError ax_check_default_set(ProAsmcomp *feature){
    ProElement tree=NULL,sets=NULL;
    const ProElemId id=PRO_E_COMPONENT_SETS;
    ProError error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(error)return error;
    error=mc_tree_element(tree,&id,1,&sets);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error&&sets){
        int count;
        error=ProElementArrayCount(sets,NULL,&count);
        if(!error&&count>0)error=PRO_TK_BAD_CONTEXT;
    }
    ProFeatureElemtreeFree(feature,tree);
    return error;
}

static int ax_selection_equal(ProSelection a,ProSelection b,int component_id){
    ProModelitem i,j;
    ProAsmcomppath p={0},q={0};
    if(!a||!b||ProSelectionModelitemGet(a,&i)||ProSelectionModelitemGet(b,&j)||
       i.type!=j.type||i.id!=j.id||i.owner!=j.owner)return 0;
    ProSelectionAsmcomppathGet(a,&p);
    ProSelectionAsmcomppathGet(b,&q);
    if(component_id){
        /* A component-side selection may be stored local to the part or
           returned by Creo with the occurrence prepended. */
        return (!p.table_num||(p.table_num==1&&p.comp_id_table[0]==component_id))&&
               (!q.table_num||(q.table_num==1&&q.comp_id_table[0]==component_id));
    }
    return p.table_num==q.table_num&&p.table_num>=0&&p.table_num<=AX_PATH_DEPTH&&
        !memcmp(p.comp_id_table,q.comp_id_table,(size_t)p.table_num*sizeof(p.comp_id_table[0]));
}

static int ax_constraint_equal(ProAsmcompconstraint a,ProAsmcompconstraint b,double mm,int component_id){
    ProAsmcompConstrType x,y;
    int flags_a,flags_b;
    if(ProAsmcompconstraintTypeGet(a,&x)||ProAsmcompconstraintTypeGet(b,&y)||
       ProAsmcompconstraintAttributesGet(a,&flags_a)||ProAsmcompconstraintAttributesGet(b,&flags_b))return 0;
    if(x!=y||flags_a!=flags_b){
        trace("expected constraint type",x);trace("actual constraint type",y);
        trace("expected constraint flags",flags_a);trace("actual constraint flags",flags_b);
        return 0;
    }
    if(x==PRO_ASM_FIX)return 1;
    ProSelection aa=NULL,ab=NULL,ba=NULL,bb=NULL;
    ProDatumside side_aa,side_ab,side_ba,side_bb;
    if(ProAsmcompconstraintAsmreferenceGet(a,&aa,&side_aa)||ProAsmcompconstraintAsmreferenceGet(b,&ab,&side_ab)||
       ProAsmcompconstraintCompreferenceGet(a,&ba,&side_ba)||ProAsmcompconstraintCompreferenceGet(b,&bb,&side_bb))return 0;
    if(!ax_selection_equal(aa,ab,0)||!ax_selection_equal(ba,bb,component_id)||
       (side_aa!=side_ab&&!(side_aa==PRO_DATUM_SIDE_NONE&&side_ab==PRO_DATUM_SIDE_YELLOW))||
       (side_ba!=side_bb&&!(side_ba==PRO_DATUM_SIDE_NONE&&side_bb==PRO_DATUM_SIDE_YELLOW)))return 0;
    if(ax_offset_type(x)||ax_angle_type(x)){
        double u,v;
        if(ProAsmcompconstraintOffsetGet(a,&u)||ProAsmcompconstraintOffsetGet(b,&v)||fabs(u-v)*(ax_angle_type(x)?1:mm)>1e-5)return 0;
    }
    return 1;
}

static ProError ax_compare_constraints(ProAsmcomp *feature,ProAsmcomppath *root,
    ProAsmcompconstraint *expected,int count,double mm){
    ProAsmcompconstraint *actual=NULL;
    int n;
    ProError error=ax_read_constraints(feature,root,&actual,&n);
    if(!error&&n!=count)error=PRO_TK_GENERAL_ERROR;
    for(int i=0;!error&&i<n;i++){
        if(!ax_constraint_equal(expected[i],actual[i],mm,feature->id)){
            trace("assembly constraint read-back mismatch index",i+1);
            error=PRO_TK_GENERAL_ERROR;
        }
    }
    if(actual)ProAsmcompconstraintArrayFree(actual);
    return error;
}

static ProError ax_model_to_root(ProSelection selection,AxGeometry *shape){
    ProAsmcomppath path={0};
    ProError error=ProSelectionAsmcomppathGet(selection,&path);
    if(error)return error;
    if(!path.table_num)return PRO_TK_NO_ERROR;
    ProMatrix pose;
    AxGeometry local=*shape;
    if((error=ProAsmcomppathTrfGet(&path,PRO_B_TRUE,pose))||
       (error=ProPntTrfEval(local.point,pose,shape->point))||
       (error=ProVectorTrfEval(local.direction,pose,shape->direction)))return error;
    return ad_unit(shape->direction)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_verify_relation(ProAsmcompconstraint row,ProMatrix pose,double mm){
    ProAsmcompConstrType type;
    ProError error=ProAsmcompconstraintTypeGet(row,&type);
    if(error||type==PRO_ASM_FIX)return error;
    ProSelection a=NULL,b=NULL;
    ProDatumside side;
    ProGeomitem ai,bi;
    AxGeometry base,part;
    if((error=ProAsmcompconstraintAsmreferenceGet(row,&a,&side))||
       (error=ProAsmcompconstraintCompreferenceGet(row,&b,&side)))return error;
    if(!a||!b)return PRO_TK_E_NOT_FOUND;
    if((error=ProSelectionModelitemGet(a,&ai))||(error=ProSelectionModelitemGet(b,&bi))||
       (error=ax_geometry(&ai,&base))||(error=ax_geometry(&bi,&part))||
       (error=ax_model_to_root(a,&base)))return error;
    if(base.type!=part.type||
       (type==PRO_ASM_INSERT&&base.type!=AX_GEOM_CYLINDER)||
       (type==PRO_ASM_ALIGN&&base.type==AX_GEOM_CYLINDER)||
       (type!=PRO_ASM_INSERT&&type!=PRO_ASM_ALIGN&&base.type!=AX_GEOM_PLANE))
        return PRO_TK_INVALID_TYPE;
    ProVector point,axis,delta,radial;
    if((error=ProPntTrfEval(part.point,pose,point))||
       (error=ProVectorTrfEval(part.direction,pose,axis)))return error;
    if(!ad_unit(axis))return PRO_TK_BAD_CONTEXT;
    ad_sub(point,base.point,delta);
    double cosine=ad_dot(axis,base.direction),distance=ad_dot(delta,base.direction)*mm;
    if(base.type!=AX_GEOM_PLANE){
        ad_cross(delta,base.direction,radial);
        return fabs(cosine)>1-1e-7&&sqrt(ad_dot(radial,radial))*mm<0.001?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    }
    if(ax_angle_type(type)){
        double wanted;
        if((error=ProAsmcompconstraintOffsetGet(row,&wanted)))return error;
        if(type==PRO_ASM_MATE_ANG_OFF)cosine=-cosine;
        const double actual=acos(fmax(-1,fmin(1,cosine)))*180.0/3.14159265358979323846;
        return fabs(actual-fabs(wanted))<0.001?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    }
    int mating=type==PRO_ASM_MATE||type==PRO_ASM_MATE_OFF;
    if((mating?-cosine:cosine)<1-1e-7)return PRO_TK_GENERAL_ERROR;
    if(type==PRO_ASM_ORIENT)return PRO_TK_NO_ERROR;
    double offset=0;
    if(ax_offset_type(type)&&(error=ProAsmcompconstraintOffsetGet(row,&offset)))return error;
    if(type==PRO_ASM_ALIGN_OFF)return fabs(distance-offset*mm)<0.001?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
    return fabs(fabs(distance)-fabs(offset*mm))<0.001?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static ProError ax_add_relation(ProAsmcompconstraint **rows,AxComponent *base,AxComponent *part,
    const wchar_t *base_token,const wchar_t *part_token,ProAsmcompConstrType type,
    double offset,AxGeometry *base_geometry,AxGeometry *part_geometry){
    ProSelection a=NULL,b=NULL;
    ProAsmcompconstraint row=NULL;
    ProError error=ProAsmcompconstraintAlloc(&row);
    if(error)return error;
    if(type!=PRO_ASM_FIX&&type!=PRO_ASM_DEF_PLACEMENT){
        error=ax_ref_resolve(base,base_token,1,&a,base_geometry);
        if(!error)error=ax_ref_resolve(part,part_token,0,&b,part_geometry);
        if(error)goto done;
        if(type==PRO_ASM_INSERT){
            if(base_geometry->type==AX_GEOM_AXIS&&part_geometry->type==AX_GEOM_AXIS)type=PRO_ASM_ALIGN;
            else if(base_geometry->type!=AX_GEOM_CYLINDER||part_geometry->type!=AX_GEOM_CYLINDER){
                error=PRO_TK_INVALID_TYPE;goto done;
            }
        }else if(base_geometry->type!=AX_GEOM_PLANE||part_geometry->type!=AX_GEOM_PLANE){
            error=PRO_TK_INVALID_TYPE;goto done;
        }
    }
    error=ProAsmcompconstraintTypeSet(row,type);
    if(!error&&a)error=ProAsmcompconstraintAsmreferenceSet(row,a,PRO_DATUM_SIDE_NONE);
    if(!error&&b)error=ProAsmcompconstraintCompreferenceSet(row,b,PRO_DATUM_SIDE_NONE);
    if(!error&&(ax_offset_type(type)||ax_angle_type(type)))error=ProAsmcompconstraintOffsetSet(row,offset);
    if(!error)error=ProArrayObjectAdd((ProArray *)rows,PRO_VALUE_UNUSED,1,&row);
done:
    if(error)ProAsmcompconstraintFree(row);
    if(a)ProSelectionFree(&a);
    if(b)ProSelectionFree(&b);
    return error;
}

static ProError ax_simple_rows(ProAsmcompconstraint *rows,int count){
    for(int i=0;i<count;i++){
        ProAsmcompConstrType type;
        int flags;
        ProError error=ProAsmcompconstraintTypeGet(rows[i],&type);
        if(!error)error=ProAsmcompconstraintAttributesGet(rows[i],&flags);
        if(error)return error;
        if((flags&~PRO_ASM_CONSTR_ATTR_FORCE)||
           !(type==PRO_ASM_MATE||type==PRO_ASM_ALIGN||type==PRO_ASM_INSERT||
             type==PRO_ASM_ORIENT||type==PRO_ASM_CSYS||ax_offset_type(type)||ax_angle_type(type)))return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}

static int ax_placement_restored(ProMdl assembly,AxComponent *component,ProAsmcomppath *root,
    ProAsmcompconstraint *original,int count,ProMatrix previous,double mm){
    ProMatrix actual;
    return !ax_compare_constraints(&component->feature,root,original,count,mm)&&
        (count||!ax_packaged(&component->feature))&&
        !ax_actual_pose(assembly,&component->feature,actual)&&ax_same_pose(previous,actual,mm)&&
        !part_health(assembly);
}

static ProError ax_placement_write(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxComponent component={0},base={0};
    ProAsmcomppath root;
    ProAsmcompconstraint *original=NULL,*requested=NULL;
    ProMatrix previous,initial,actual;
    AxGeometry bases[3],parts[3];
    double part_mm,base_mm;
    int count=0,requested_count=0,attempted=0,edit_index=-1;
    int added=kind==AX_EDIT_OFFSET?0:kind==AX_THREE?3:(kind==AX_FASTENER||kind==AX_PLANE_ANGLE)?2:1;
    ProError error;
    const wchar_t *const descriptions[]={L"固定",L"平面贴合",L"平面平行",L"平面偏距",L"轴孔同轴",
        L"三面定位",L"紧固件定位",L"定位偏距修改"};
#define AP_STEP(call) do{if((error=(call))){trace("assembly placement " #call,error);goto done;}}while(0)
    AP_STEP(ax_root_path(assembly,&root));
    AP_STEP(ax_component_resolve(assembly,cmd->arg[2],&component));
    AP_STEP(ax_ordinary_occurrence(&component.feature,1));
    AP_STEP(ax_leaf_without_children(&component.feature));
    AP_STEP(ax_check_default_set(&component.feature));
    AP_STEP(qa_mm_per_unit(component.model,&part_mm));
    if(!part_close(part_mm,mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
    AP_STEP(ax_actual_pose(assembly,&component.feature,previous));
    AP_STEP(ProAsmcompPositionGet(&component.feature,initial));
    AP_STEP(ax_read_constraints(&component.feature,&root,&original,&count));
    if(count+added>AX_PLACEMENT_LIMIT||((kind==AX_FIX||kind==AX_THREE||kind==AX_FASTENER||kind==AX_PLANE_ANGLE)&&count)){
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    AP_STEP(ax_simple_rows(original,count));
    if(count)AP_STEP(ax_read_constraints(&component.feature,&root,&requested,&requested_count));
    else{
        AP_STEP(ax_packaged(&component.feature));
        AP_STEP(ProArrayAlloc(0,sizeof(*requested),3,(ProArray *)&requested));
    }
    if(requested_count!=count){error=PRO_TK_GENERAL_ERROR;goto done;}
    if(kind==AX_EDIT_OFFSET){
        int index;
        double old;
        ProAsmcompConstrType type;
        part_integer(cmd->arg[3],&index);
        edit_index=index-1;
        if(edit_index>=count){error=PRO_TK_E_NOT_FOUND;goto done;}
        AP_STEP(ProAsmcompconstraintTypeGet(requested[edit_index],&type));
        if(!ax_offset_type(type)){error=PRO_TK_INVALID_TYPE;goto done;}
        AP_STEP(ProAsmcompconstraintOffsetGet(requested[edit_index],&old));
        if(fabs(old*mm-as_arg(cmd,4))>1e-5){
            part_text_add(out,L"第 %d 条约束的当前偏距为 %.10g mm，与你给出的旧值不符。\n",index,old*mm);
            error=PRO_TK_BAD_CONTEXT;goto done;
        }
        AP_STEP(ProAsmcompconstraintOffsetSet(requested[edit_index],as_arg(cmd,5)/mm));
    }else if(kind==AX_FIX){
        AP_STEP(ax_add_relation(&requested,NULL,NULL,NULL,NULL,PRO_ASM_FIX,0,NULL,NULL));
    }else{
        AP_STEP(ax_ref_occurrence(assembly,cmd->arg[3],&base));
        if(base.path.table_num&&base.path.comp_id_table[0]==component.feature.id){error=PRO_TK_BAD_CONTEXT;goto done;}
        AP_STEP(qa_mm_per_unit(base.model,&base_mm));
        if(!part_close(base_mm,mm)){error=PRO_TK_BAD_CONTEXT;goto done;}
        for(int i=0;i<added;i++){
            ProAsmcompConstrType type=PRO_ASM_ALIGN;
            if(kind==AX_MATE||(kind==AX_FASTENER&&i==1))type=PRO_ASM_MATE;
            if(kind==AX_COAX||(kind==AX_FASTENER&&i==0))type=PRO_ASM_INSERT;
            if(kind==AX_PARALLEL)type=PRO_ASM_ORIENT;
            if(kind==AX_OFFSET)type=PRO_ASM_ALIGN_OFF;
            if(kind==AX_PLANE_ANGLE)type=i?PRO_ASM_ALIGN_ANG_OFF:PRO_ASM_INSERT;
            const double offset=kind==AX_OFFSET?as_arg(cmd,6)/mm:kind==AX_PLANE_ANGLE&&i?as_arg(cmd,8):0;
            AP_STEP(ax_add_relation(&requested,&base,&component,cmd->arg[4+2*i],cmd->arg[5+2*i],
                type,offset,&bases[i],&parts[i]));
        }
        if(kind==AX_PLANE_ANGLE){
            ProVector base_delta,part_delta;
            ad_sub(bases[0].point,bases[1].point,base_delta);
            ad_sub(parts[0].point,parts[1].point,part_delta);
            if(bases[0].type!=AX_GEOM_AXIS||parts[0].type!=AX_GEOM_AXIS||
               fabs(ad_dot(bases[0].direction,bases[1].direction))>1e-7||
               fabs(ad_dot(parts[0].direction,parts[1].direction))>1e-7||
               fabs(ad_dot(base_delta,bases[1].direction))*mm>0.001||
               fabs(ad_dot(part_delta,parts[1].direction))*mm>0.001){
                part_text_add(out,L"角度定位要求两侧的基准轴分别位于所选基准面内。\n");
                error=PRO_TK_BAD_INPUTS;goto done;
            }
        }
        if(kind==AX_FASTENER){
            FdRef bore_ref={0};
            QualityBore bore;
            error=qa_bore(base.model,cmd->arg[4],&bore_ref,&bore);
            fd_ref_free(&bore_ref);
            if(error||parts[0].radius>bore.radius+0.001/mm){
                part_text_add(out,L"紧固件杆面必须是能装入指定孔壁的圆柱面。\n");
                if(!error)error=PRO_TK_BAD_CONTEXT;
                goto done;
            }
        }
        if(kind==AX_THREE){
            for(int i=0;i<3;i++)for(int j=i+1;j<3;j++){
                if(fabs(ad_dot(bases[i].direction,bases[j].direction))>1e-7||
                   fabs(ad_dot(parts[i].direction,parts[j].direction))>1e-7){error=PRO_TK_BAD_INPUTS;goto done;}
            }
        }
    }
    attempted=1;
    AP_STEP(ProAsmcompConstraintsSet(&root,&component.feature,requested));
    AP_STEP(part_regenerate(assembly));
    AP_STEP(part_health(assembly));
    AP_STEP(ax_compare_constraints(&component.feature,&root,requested,count+added,mm));
    AP_STEP(ax_actual_pose(assembly,&component.feature,actual));
#ifdef BWTL_PLACEMENT_FAULT_TEST
    if(ax_placement_fault){error=PRO_TK_GENERAL_ERROR;goto done;}
#endif
    if(kind==AX_FIX&&!ax_same_pose(previous,actual,mm)){error=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<count+added;i++){
        if(i>=count||i==edit_index)AP_STEP(ax_verify_relation(requested[i],actual,mm));
    }
    ProBoolean under;
    AP_STEP(ProAsmcompIsUnderconstrained(&component.feature,&under));
    if(under&&(kind==AX_THREE||kind==AX_FIX)){error=PRO_TK_GENERAL_ERROR;goto done;}
    part_text_add(out,L"%ls 已完成%ls，已重生成并核对定位，未保存。%ls\n",
        cmd->arg[2],kind==AX_PLANE_ANGLE?L"平面角度定位":descriptions[kind-AX_FIX],under?L" Creo 报告该组件仍有自由度。":L"");
done:
    if(error&&attempted){
        if(!ax_placement_restored(assembly,&component,&root,original,count,previous,mm)){
            ProError restore;
            if(count){
                restore=ProAsmcompPositionSet(NULL,&component.feature,initial);
                if(!restore)restore=ProAsmcompConstraintsSet(&root,&component.feature,original);
                if(!restore)restore=part_regenerate(assembly);
            }else{
                restore=ProAsmcompAllConstrRemove(&component.feature);
                if(!restore)restore=ax_set_position(assembly,&component.feature,initial);
            }
            if(restore)trace("assembly placement restore",restore);
        }
        if(ax_placement_restored(assembly,&component,&root,original,count,previous,mm))
            part_text_add(out,L"已核对原有约束和组件位置已恢复，未保存。\n");
        else ax_uncertain(out);
    }
    if(requested)ProAsmcompconstraintArrayFree(requested);
    if(original)ProAsmcompconstraintArrayFree(original);
    return error;
#undef AP_STEP
}
