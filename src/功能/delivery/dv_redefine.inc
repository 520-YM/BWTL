static ProError dv_datum_tree(ProElement tree,ProElement *reference,ProElement *offset){
    ProElement constraints,entry,type;
    int value;
    ProError error;
    if((error=mc_tree_element(tree,(ProElemId[]){PRO_E_DTMPLN_CONSTRAINTS},1,&constraints)))return error;
    ProElement *children=NULL;
    if((error=ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&children)))return error;
    int count=0;
    error=ProElementChildrenGet(constraints,NULL,&children);
    if(!error)error=ProArraySizeGet((ProArray)children,&count);
    ProArrayFree((ProArray *)&children);
    if(error)return error;
    if(count!=1)return PRO_TK_BAD_CONTEXT;
    ProElemId path[]={PRO_E_DTMPLN_CONSTRAINTS,0,PRO_E_DTMPLN_CONSTR_TYPE};
    if((error=md_array_value(tree,path,3,1,0,&type))||(error=ProElementIntegerGet(type,NULL,&value)))return error;
    if(value!=PRO_DTMPLN_OFFS)return PRO_TK_BAD_CONTEXT;
    path[2]=PRO_E_DTMPLN_CONSTR_REF;
    if((error=md_array_value(tree,path,3,1,0,&entry)))return error;
    *reference=entry;
    path[2]=PRO_E_DTMPLN_CONSTR_REF_OFFSET;
    return md_array_value(tree,path,3,1,0,offset);
}

static ProError dv_depth_tree(ProElement tree,ProElement *depth){
    ProElement element;
    int value;
    ProError error;
    if((error=mc_tree_element(tree,(ProElemId[]){PRO_E_FEATURE_FORM},1,&element))||
       (error=ProElementIntegerGet(element,NULL,&value)))return error;
    if(value!=PRO_EXTRUDE)return PRO_TK_BAD_CONTEXT;
    if((error=mc_tree_element(tree,(ProElemId[]){PRO_E_STD_EXT_DEPTH,PRO_E_EXT_DEPTH_FROM,PRO_E_EXT_DEPTH_FROM_TYPE},3,&element))||
       (error=ProElementIntegerGet(element,NULL,&value)))return error;
    if(value!=PRO_EXT_DEPTH_FROM_NONE)return PRO_TK_BAD_CONTEXT;
    if((error=mc_tree_element(tree,(ProElemId[]){PRO_E_STD_EXT_DEPTH,PRO_E_EXT_DEPTH_TO,PRO_E_EXT_DEPTH_TO_TYPE},3,&element))||
       (error=ProElementIntegerGet(element,NULL,&value)))return error;
    if(value!=PRO_EXT_DEPTH_TO_BLIND)return PRO_TK_BAD_CONTEXT;
    return mc_tree_element(tree,(ProElemId[]){PRO_E_STD_EXT_DEPTH,PRO_E_EXT_DEPTH_TO,PRO_E_EXT_DEPTH_TO_VALUE},3,depth);
}

static ProError dv_reference_item(ProElement element,ProModelitem *item){
    ProReference reference=NULL;
    ProSelection selection=NULL;
    ProError error=ProElementReferenceGet(element,NULL,&reference);
    if(!error)error=ProReferenceToSelection(reference,&selection);
    if(!error)error=ProSelectionModelitemGet(selection,item);
    if(selection)ProSelectionFree(&selection);
    if(reference)ProReferenceFree(reference);
    return error;
}

static int dv_reference_missing(ProElement element){
    ProReference reference=NULL;
    ProRefState state;
    ProError error=ProElementReferenceGet(element,NULL,&reference);
    if(error==PRO_TK_EMPTY)return 1;
    if(error)return 0;
    error=ProReferenceStatusGet(reference,&state);
    ProReferenceFree(reference);
    return !error&&(state==PRO_REF_MISSING||state==PRO_REF_NOT_FOUND||state==PRO_REF_INVALID||state==PRO_REF_SUPPRESSED);
}

static ProError dv_plane_verify(ProFeature *feature,ProModelitem *parent,double expected){
    ProGeomitem plane;
    ProVector point,normal,base,direction,delta;
    ProError error;
    if((error=fd_feature_geom(feature,PRO_SURFACE,&plane))||
       (error=fd_plane_data(&plane,point,normal))||(error=fd_plane_data(parent,base,direction)))return error;
    if(!ad_unit(direction)||!ad_unit(normal)||fabs(ad_dot(normal,direction))<1.0-1e-8)return PRO_TK_GENERAL_ERROR;
    ad_sub(point,base,delta);
    return part_close(ad_dot(delta,direction),expected)?PRO_TK_NO_ERROR:PRO_TK_GENERAL_ERROR;
}

static int dv_original_verified(ProMdl model,ProFeature *feature,int plane,int missing,
    const ProModelitem *previous,double old_value,const ProMassProperty *before,int had_material){
    ProElement tree=NULL,reference=NULL,element=NULL;
    double actual;
    ProError error=ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=plane?dv_datum_tree(tree,&reference,&element):dv_depth_tree(tree,&element);
    if(!error)error=ProElementDoubleGet(element,NULL,&actual);
    int verified=!error&&part_close(actual,old_value);
    if(verified&&missing){
        unsigned flags;
        verified=dv_reference_missing(reference)&&!ProFeatureStatusflagsGet(feature,&flags)&&(flags&PRO_FEAT_STAT_FAILED);
    }else if(verified){
        verified=!part_health(model);
        if(verified&&plane){
            ProModelitem readback;
            verified=!dv_reference_item(reference,&readback)&&readback.owner==previous->owner&&
                readback.type==previous->type&&readback.id==previous->id&&!dv_plane_verify(feature,&readback,old_value);
        }
        ProMassProperty after;
        if(verified&&had_material)
            verified=!qa_geometry_mass(model,&after)&&part_close(before->volume,after.volume)&&
                part_close(before->surface_area,after.surface_area);
    }
    if(tree)ProFeatureElemtreeFree(feature,tree);
    return verified;
}

static ProError dv_redefine(ProMdl model,const PartCommand *cmd,PartText *out){
    int repair=dv_kind(cmd->arg[0])==DV_REF;
    int missing=repair&&!wcscmp(cmd->arg[3],L"MISSING");
    int plane=repair||!wcscmp(cmd->arg[3],L"OFFSET");
    ProFeature feature,replacement_owner;
    ProFeattype type;
    ProFeatStatus state;
    unsigned flags;
    int number,parent_number;
    ProElement original=NULL,tree=NULL,checked=NULL,reference=NULL,value=NULL;
    ProReference replacement=NULL;
    FdRef new_reference={0};
    ProModelitem previous={0},actual={0};
    ProFeatureCreateOptions *options=NULL;
    ProErrorlist errors={0};
    ProError error;
    double scale,old_value,new_value,readback;
    ProMassProperty before;
    int attempted=0,had_material=0;
#define DR_STEP(call) do{if((error=(call))){trace("delivery redefine " #call,error);goto done;}}while(0)
    DR_STEP(dv_feature(model,cmd->arg[2],&feature,&number));
    DR_STEP(ProFeatureTypeGet(&feature,&type));
    if(plane?type!=PRO_FEAT_DATUM:type!=PRO_FEAT_CUT&&type!=PRO_FEAT_PROTRUSION){
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    DR_STEP(ProFeatureStatusGet(&feature,&state));
    DR_STEP(ProFeatureStatusflagsGet(&feature,&flags));
    if(missing?!(flags&PRO_FEAT_STAT_FAILED):state!=PRO_FEAT_ACTIVE||(flags&PRO_FEAT_STAT_FAILED)){
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    DR_STEP(qa_mm_per_unit(model,&scale));
    DR_STEP(ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&original));
    DR_STEP(ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree));
    if(plane)DR_STEP(dv_datum_tree(tree,&reference,&value));
    else DR_STEP(dv_depth_tree(tree,&value));
    DR_STEP(ProElementDoubleGet(value,NULL,&old_value));

    new_value=old_value;
    if(repair){
        ProVector point,normal;
        DR_STEP(fd_ref_resolve(model,cmd->arg[4],PRO_SURFACE,&new_reference));
        DR_STEP(fd_plane_data(&new_reference.item,point,normal));
        DR_STEP(ProFeatureInit((ProSolid)model,new_reference.owner_feature,&replacement_owner));
        DR_STEP(ProFeatureNumberGet(&replacement_owner,&parent_number));
        DR_STEP(ProFeatureStatusGet(&replacement_owner,&state));
        DR_STEP(ProFeatureStatusflagsGet(&replacement_owner,&flags));
        if(parent_number>=number||state!=PRO_FEAT_ACTIVE||(flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED))){
            error=PRO_TK_BAD_CONTEXT;goto done;
        }
        if(missing){
            if(!dv_reference_missing(reference)){error=PRO_TK_BAD_CONTEXT;goto done;}
        }else{
            int expected_id;
            part_integer(cmd->arg[3]+8,&expected_id);
            DR_STEP(dv_reference_item(reference,&previous));
            if(previous.owner!=model||previous.type!=PRO_SURFACE||previous.id!=expected_id||
               previous.id==new_reference.item.id){error=PRO_TK_BAD_CONTEXT;goto done;}
        }
        DR_STEP(ProSelectionToReference(new_reference.selection,&replacement));
        DR_STEP(ProElementReferenceSet(reference,replacement));
    }else{
        if(!part_close(old_value*scale,wcstod(cmd->arg[4],NULL))){error=PRO_TK_BAD_CONTEXT;goto done;}
        new_value=wcstod(cmd->arg[5],NULL)/scale;
        if(part_close(old_value,new_value)){error=PRO_TK_NO_CHANGE;goto done;}
        if(plane)DR_STEP(dv_reference_item(reference,&previous));
        DR_STEP(ProElementDoubleSet(value,new_value));
    }
    DR_STEP(ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options));
    options[0]=PRO_FEAT_CR_NO_OPTS;
    if(!missing)had_material=!qa_geometry_mass(model,&before);
    attempted=1;
    DR_STEP(ProFeatureWithoptionsRedefine(NULL,&feature,tree,options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors));
    DR_STEP(part_regenerate(model));
    DR_STEP(part_health(model));
    DR_STEP(ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&checked));
    if(plane)DR_STEP(dv_datum_tree(checked,&reference,&value));
    else DR_STEP(dv_depth_tree(checked,&value));
    DR_STEP(ProElementDoubleGet(value,NULL,&readback));
    if(!part_close(readback,new_value)){error=PRO_TK_GENERAL_ERROR;goto done;}
    if(plane){
        ProModelitem *wanted=repair?&new_reference.item:&previous;
        DR_STEP(dv_reference_item(reference,&actual));
        if(actual.owner!=wanted->owner||actual.id!=wanted->id||actual.type!=wanted->type){
            error=PRO_TK_GENERAL_ERROR;goto done;
        }
        DR_STEP(dv_plane_verify(&feature,&actual,new_value));
    }
    if(repair)part_text_add(out,L"已修复 %ls 的偏移面参照，改用 %ls，保持 %.10g mm 偏移。新参照与实际面位置已核对。\n",
        cmd->arg[2],cmd->arg[4],new_value*scale);
    else part_text_add(out,L"已重定义 %ls（特征 ID %d），%ls从 %.10g 改为 %.10g mm。原生读回和重生成通过。\n",
        cmd->arg[2],feature.id,plane?L"偏移":L"盲深",old_value*scale,new_value*scale);
done:
    if(error&&attempted&&!dv_original_verified(model,&feature,plane,missing,&previous,old_value,&before,had_material)){
        ProErrorlist restore_errors={0};
        ProFeatureWithoptionsRedefine(NULL,&feature,original,options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&restore_errors);
        if(!dv_original_verified(model,&feature,plane,missing,&previous,old_value,&before,had_material)){
            dv_recovery_uncertain=1;
            part_text_add(out,L"原特征恢复未能核实，本次会话已停止交付模块写入。\n");
        }
    }
    if(checked)ProFeatureElemtreeFree(&feature,checked);
    if(tree)ProFeatureElemtreeFree(&feature,tree);
    if(original)ProFeatureElemtreeFree(&feature,original);
    if(replacement)ProReferenceFree(replacement);
    if(options)ProArrayFree((ProArray *)&options);
    fd_ref_free(&new_reference);
    return error;
#undef DR_STEP
}
