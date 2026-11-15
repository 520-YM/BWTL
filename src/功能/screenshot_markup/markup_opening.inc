/* Read the actual cut sketch. Pixel geometry only locates the opening. */
static int markup_opening_section(ProFeature *feature,ProElement tree,FdSketch *linked,ProSection *section){
    if(!fd_section_from_tree(tree,section)&&*section)return 1;
    ProElement element;
    ProModelitem item;
    if(mc_tree_element(tree,(ProElemId[]){PRO_E_STD_SECTION,PRO_E_SEC_USE_SKETCH},2,&element)||
       dv_reference_item(element,&item)||item.owner!=feature->owner||item.type!=PRO_FEATURE)return 0;
    linked->feature=item;
    int *children=NULL,count=0;
    int single=!ProFeatureChildrenGet(&linked->feature,&children,&count)&&count==1&&children[0]==feature->id;
    ProArrayFree((ProArray *)&children);
    return single&&!ProFeatureElemtreeExtract(&linked->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&linked->tree)&&
        !fd_section_from_tree(linked->tree,section)&&*section;
}

static int markup_opening_profile(ProSection section,ImitationLocalProfile *profile,int line_ids[4]){
    int *ids=NULL,count=0,valid=1;
    if(ProSectionEntityIdsGet(section,&ids,&count))return 0;
    for(int i=0;i<count&&valid;i++){
        ProBoolean construction,projection;
        Pro2dEntdef *entity=NULL;
        valid=!ProSectionEntityIsConstruction(section,ids[i],&construction)&&
            !ProSectionEntityIsProjection(section,ids[i],&projection);
        if(!valid||construction||projection)continue;
        valid=profile->line_count<4&&!ProSectionEntityGet(section,ids[i],&entity)&&entity->type==PRO_2D_LINE;
        if(valid){
            line_ids[profile->line_count]=ids[i];
            profile->lines[profile->line_count++]=*(Pro2dLinedef *)entity;
        }
    }
    ProArrayFree((ProArray *)&ids);
    return valid&&!imitation_local_area(profile);
}

static void markup_opening_drivers(ProSection section,ProFeature *owner,const ImitationLocalProfile *profile,
    const int line_ids[4],MarkupOpening *opening){
    int *ids=NULL,*solid_ids=NULL,count=0,found[2]={0};
    if(!ProSectionDimensionIdsGet(section,&ids,&solid_ids,&count))for(int i=0;i<count;i++){
        ProSecdimType type;
        if(ProSecdimTypeGet(section,ids[i],&type)||type!=PRO_TK_DIM_LINE)continue;
        int *refs=NULL,n=0,line=-1;
        ProSectionPointType *points=NULL;
        if(!ProSecdimReferencesGet(section,ids[i],&refs,&points,&n)&&n==1&&points[0]==PRO_ENT_WHOLE)
            for(int j=0;j<4;j++)if(refs[0]==line_ids[j])line=j;
        ProArrayFree((ProArray *)&refs);
        ProArrayFree((ProArray *)&points);
        if(line<0)continue;
        const Pro2dLinedef *edge=&profile->lines[line];
        int axis=fabs(edge->end2[1]-edge->end1[1])>fabs(edge->end2[0]-edge->end1[0]);
        ProDimension dimension;
        ProDimensiontype dimension_type;
        ProFeature dimension_owner;
        ProBoolean driving,related,negative;
        double old;
        if(!ProModelitemInit(owner->owner,solid_ids[i],PRO_DIMENSION,&dimension)&&
           !ProDimensionOwnerfeatureGet(&dimension,&dimension_owner)&&dimension_owner.id==owner->id&&
           !ProDimensionTypeGet(&dimension,&dimension_type)&&dimension_type==PRODIMTYPE_LINEAR&&
           !ProDimensionIsDriving(&dimension,&driving)&&driving&&
           !ProDimensionIsReldriven(&dimension,&related)&&!related&&
           !ProDimensionIsRegenednegative(&dimension,&negative)&&!negative&&
           !ProDimensionValueGet(&dimension,&old)&&part_close(old,opening->size[axis])){
            opening->dimension[axis]=dimension.id;
            found[axis]++;
        }
    }
    for(int axis=0;axis<2;axis++)if(found[axis]!=1)opening->dimension[axis]=-1;
    ProArrayFree((ProArray *)&ids);
    ProArrayFree((ProArray *)&solid_ids);
}

static int markup_opening_project(MarkupProjection *map,ProSection section,const ImitationLocalProfile *profile,
    MarkupOpening *opening){
    ProMatrix frame;
    if(ProSectionLocationGet(section,frame)||fabs(ad_dot(frame[2],map->toward_viewer))<0.45)return 0;
    double low[2]={HUGE_VAL,HUGE_VAL},high[2]={-HUGE_VAL,-HUGE_VAL};
    for(int i=0;i<4;i++){
        const Pro2dLinedef *line=&profile->lines[i];
        double dx=fabs(line->end2[0]-line->end1[0]),dy=fabs(line->end2[1]-line->end1[1]);
        if(fmin(dx,dy)>1e-6*fmax(dx,dy))return 0;
        for(int axis=0;axis<2;axis++){
            low[axis]=fmin(low[axis],fmin(line->end1[axis],line->end2[axis]));
            high[axis]=fmax(high[axis],fmax(line->end1[axis],line->end2[axis]));
        }
    }
    ProPoint3d mid={(low[0]+high[0])*0.5,(low[1]+high[1])*0.5,0},world;
    Pro2dPnt center;
    if(ProPntTrfEval(mid,frame,world)||!markup_project(map,world,center))return 0;
    ProRay ray;
    for(int i=0;i<3;i++){
        ray.start_point[i]=world[i]+map->span*map->toward_viewer[i];
        ray.dir_vector[i]=-map->toward_viewer[i];
    }
    QualityRay hits;
    if(qa_ray(map->model,&ray,1e-6,&hits)||hits.count)return 0;
    opening->x=center[0];opening->y=center[1];
    for(int axis=0;axis<2;axis++)opening->size[axis]=high[axis]-low[axis];
    for(int i=0;i<4;i++){
        ProPoint3d corner={(i==1||i==2)?high[0]:low[0],i>=2?high[1]:low[1],0};
        if(ProPntTrfEval(corner,frame,world)||!markup_project(map,world,opening->corners[i]))return 0;
    }
    int visible=0;
    for(int i=0;i<4;i++){
        double *a=opening->corners[i],*b=opening->corners[(i+1)%4];
        if(a[0]<0||a[0]>=map->image->width||a[1]<0||a[1]>=map->image->height||
           hypot(a[0]-b[0],a[1]-b[1])<5)return 0;
        Pro2dPnt rim={(a[0]+b[0])*0.5,(a[1]+b[1])*0.5};
        visible+=markup_rim_contrast(map,center,rim);
    }
    return visible>=3;
}

static ProError markup_opening_feature(ProFeature *feature,ProError status,ProAppData data){
    MarkupProjection *map=data;
    if(status)return status;
    ProFeattype type;
    ProFeatStatus state;
    ProPatternStatus pattern;
    if(ProFeatureTypeGet(feature,&type)||(type!=PRO_FEAT_CUT&&type!=PRO_FEAT_CUT_SMT)||
       ProFeatureStatusGet(feature,&state)||state!=PRO_FEAT_ACTIVE||
       ProFeaturePatternStatusGet(feature,&pattern))return PRO_TK_NO_ERROR;
    ProElement tree=NULL,element;
    ProSection section=NULL;
    FdSketch linked={0};
    int form,material,depth,line_ids[4];
    ImitationLocalProfile profile={0};
    MarkupOpening opening={.feature=feature->id,.owner=feature->id,.dimension={-1,-1}};
    if(!ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)&&
       !mc_tree_element(tree,(ProElemId[]){PRO_E_FEATURE_FORM},1,&element)&&
       !ProElementIntegerGet(element,NULL,&form)&&form==PRO_EXTRUDE&&
       !mc_tree_element(tree,(ProElemId[]){PRO_E_REMOVE_MATERIAL},1,&element)&&
       !ProElementIntegerGet(element,NULL,&material)&&material==PRO_EXT_MATERIAL_REMOVE&&
       !mc_tree_element(tree,(ProElemId[]){PRO_E_STD_EXT_DEPTH,PRO_E_EXT_DEPTH_TO,PRO_E_EXT_DEPTH_TO_TYPE},3,&element)&&
       !ProElementIntegerGet(element,NULL,&depth)&&depth==PRO_EXT_DEPTH_TO_ALL&&
       markup_opening_section(feature,tree,&linked,&section)&&
       markup_opening_profile(section,&profile,line_ids)&&markup_opening_project(map,section,&profile,&opening)){
        if(linked.tree)opening.owner=linked.feature.id;
        if(pattern==PRO_PATTERN_NONE)
            markup_opening_drivers(section,linked.tree?&linked.feature:feature,&profile,line_ids,&opening);
        MarkupImage *image=map->image;
        if(image->opening_count==MARKUP_MAX_OPENINGS)image->openings_truncated=1;
        else image->openings[image->opening_count++]=opening;
    }
    fd_sketch_free(&linked);
    if(tree)ProFeatureElemtreeFree(feature,tree);
    return PRO_TK_NO_ERROR;
}
