#define IMITATION_LOCAL_MAX 32

typedef struct {
    Pro2dCircledef circles[IMITATION_LOCAL_MAX];
    Pro2dLinedef lines[4];
    int circle_count,line_count;
    double offset[2],area;
} ImitationLocalProfile;

static int imitation_local_point_equal(const double *a,const double *b){
    return hypot(a[0]-b[0],a[1]-b[1])<0.001;
}

static ProError imitation_local_area(ImitationLocalProfile *profile){
    if(profile->circle_count&&!profile->line_count){
        for(int i=0;i<profile->circle_count;i++){
            double r=profile->circles[i].radius;
            if(!isfinite(r)||r<0.05)return PRO_TK_BAD_INPUTS;
            profile->area+=AS_PI*r*r;
        }
        return PRO_TK_NO_ERROR;
    }
    if(profile->circle_count||profile->line_count!=4)return PRO_TK_UNSUPPORTED;
    Pro2dPnt vertices[4];
    memcpy(vertices[0],profile->lines[0].end1,sizeof(vertices[0]));
    memcpy(vertices[1],profile->lines[0].end2,sizeof(vertices[1]));
    unsigned used=1;
    for(int i=1;i<4;i++){
        int found=0;
        for(int j=1;j<4&&!found;j++)if(!(used&(1u<<j))){
            Pro2dLinedef *line=&profile->lines[j];
            double *next=NULL;
            if(imitation_local_point_equal(vertices[i],line->end1))next=line->end2;
            else if(imitation_local_point_equal(vertices[i],line->end2))next=line->end1;
            if(!next)continue;
            found=1;used|=1u<<j;
            if(i<3)memcpy(vertices[i+1],next,sizeof(vertices[0]));
            else if(!imitation_local_point_equal(vertices[0],next))return PRO_TK_UNSUPPORTED;
        }
        if(!found)return PRO_TK_UNSUPPORTED;
    }
    for(int i=0;i<4;i++){
        double *a=vertices[i],*b=vertices[(i+1)%4],*c=vertices[(i+2)%4];
        double x=b[0]-a[0],y=b[1]-a[1],u=c[0]-b[0],v=c[1]-b[1];
        if(hypot(x,y)<0.1||fabs(x*u+y*v)>1e-6*hypot(x,y)*hypot(u,v))return PRO_TK_UNSUPPORTED;
        profile->area+=(a[0]*b[1]-a[1]*b[0])*0.5;
    }
    profile->area=fabs(profile->area);
    return profile->area>0.01?PRO_TK_NO_ERROR:PRO_TK_UNSUPPORTED;
}

static ProError imitation_local_profile(ProMdl source,int id,ImitationLocalProfile *profile){
    FdSketch cut={0},linked={0};
    double mm;
    int depth=0,removal=0,*ids=NULL,count=0;
    ProElement element;
    ProError error=qa_mm_per_unit(source,&mm);
    if(!error)error=ProFeatureInit((ProSolid)source,id,&cut.feature);
    ProFeattype type;
    if(!error)error=ProFeatureTypeGet(&cut.feature,&type);
    if(!error)error=ProFeatureElemtreeExtract(&cut.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&cut.tree);
    if(!error)error=mc_tree_element(cut.tree,(ProElemId[]){PRO_E_REMOVE_MATERIAL},1,&element);
    if(!error)error=ProElementIntegerGet(element,NULL,&removal);
    if(!error)error=mc_tree_element(cut.tree,(ProElemId[]){PRO_E_STD_EXT_DEPTH,PRO_E_EXT_DEPTH_TO,PRO_E_EXT_DEPTH_TO_TYPE},3,&element);
    if(!error)error=ProElementIntegerGet(element,NULL,&depth);
    if(!error&&(removal!=PRO_EXT_MATERIAL_REMOVE||
       (depth!=PRO_EXT_DEPTH_TO_ALL&&(type!=PRO_FEAT_CUT_SMT||depth!=PRO_EXT_DEPTH_TO_NEXT)))){
        trace("Imitation local source removal",removal);
        trace("Imitation local source depth",depth);
        error=PRO_TK_UNSUPPORTED;
    }
    if(!error){
        error=fd_section_from_tree(cut.tree,&cut.section);
        if(error||!cut.section){
            ProModelitem item;
            error=mc_tree_element(cut.tree,(ProElemId[]){PRO_E_STD_SECTION,PRO_E_SEC_USE_SKETCH},2,&element);
            if(!error)error=dv_reference_item(element,&item);
            if(!error&&item.owner!=source)error=PRO_TK_BAD_CONTEXT;
            if(!error&&item.type==PRO_FEATURE)linked.feature=item;
            else if(!error)error=ProGeomitemFeatureGet(&item,&linked.feature);
            if(!error)error=ProFeatureElemtreeExtract(&linked.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&linked.tree);
            if(!error)error=fd_section_from_tree(linked.tree,&linked.section);
        }
    }
    ProSection section=linked.section?linked.section:cut.section;
    if(!error)error=ProSectionEntityIdsGet(section,&ids,&count);
    for(int i=0;!error&&i<count;i++){
        Pro2dEntdef *entity=NULL;
        ProBoolean construction=0,projection=0;
        error=ProSectionEntityIsConstruction(section,ids[i],&construction);
        if(error||construction)continue;
        error=ProSectionEntityIsProjection(section,ids[i],&projection);
        if(error||projection)continue;
        error=ProSectionEntityGet(section,ids[i],&entity);
        if(error)break;
        if(entity->type!=PRO_2D_CIRCLE&&entity->type!=PRO_2D_LINE){
            error=PRO_TK_UNSUPPORTED;
            continue;
        }
        if(entity->type==PRO_2D_CIRCLE&&profile->circle_count<IMITATION_LOCAL_MAX){
            Pro2dCircledef *c=&profile->circles[profile->circle_count++];
            *c=*(Pro2dCircledef *)entity;
            c->center[0]*=mm;c->center[1]*=mm;c->radius*=mm;
        }else if(entity->type==PRO_2D_LINE&&profile->line_count<4){
            Pro2dLinedef *line=&profile->lines[profile->line_count++];
            *line=*(Pro2dLinedef *)entity;
            for(int axis=0;axis<2;axis++){line->end1[axis]*=mm;line->end2[axis]*=mm;}
        }else error=PRO_TK_OUT_OF_RANGE;
    }
    if(!error)error=imitation_local_area(profile);
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&linked);
    fd_sketch_free(&cut);
    return error;
}

static ProError imitation_local_draw(ProSection section,const ProMatrix frame,double scale,void *context){
    const ImitationLocalProfile *profile=context;
    for(int i=0;i<profile->circle_count;i++){
        const Pro2dCircledef *c=&profile->circles[i];
        int id;
        ProError error=model_circle(section,(c->center[0]+profile->offset[0])*scale,
            (c->center[1]+profile->offset[1])*scale,c->radius*scale,&id);
        if(error)return error;
    }
    for(int i=0;i<profile->line_count;i++){
        const Pro2dLinedef *line=&profile->lines[i];
        int id;
        ProError error=as_add_line(section,
            as_point((line->end1[0]+profile->offset[0])*scale,(line->end1[1]+profile->offset[1])*scale),
            as_point((line->end2[0]+profile->offset[0])*scale,(line->end2[1]+profile->offset[1])*scale),&id);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_local_command(const PartCommand *command,const PartSnapshot *snapshot,
    wchar_t *text,size_t capacity){
    DeliveryOutput work={0};
    PartText out={text,capacity,0};
    ProMdl target=NULL,source=NULL,copy=NULL;
    ProMdlName name;
    ProName sketch,cut;
    ProPath destination;
    ProMatrix frame;
    ProVector point,normal;
    ProMassProperty before,after;
    ImitationLocalProfile profile={0};
    double mm,thickness;
    int id;
    const wchar_t *stage=L"绑定当前钣金零件和会话中的源零件";
    text[0]=0;
#define LOCAL_STEP(call) do{error=(call);if(error)goto done;}while(0)
    ProError error;
    LOCAL_STEP(part_check_binding(command,snapshot,&target));
    if(snapshot->in_assembly||snapshot->subtype!=PROMDLSTYPE_PART_SHEETMETAL){
        error=PRO_TK_BAD_CONTEXT;goto done;
    }
    LOCAL_STEP(ProMdlnameInit((wchar_t *)command->arg[2],PRO_MDL_PART,&source));
    if(source==target){error=PRO_TK_BAD_CONTEXT;goto done;}
    LOCAL_STEP(part_health(source));
    LOCAL_STEP(part_health(target));
    LOCAL_STEP(dv_no_external_dependencies(target));
    LOCAL_STEP(fab_thickness(target,&mm,&thickness));
    LOCAL_STEP(qa_single_body(target,NULL));
    stage=L"读取指定拉伸贯穿切口的圆孔组或矩形窗口";
    part_integer(command->arg[3],&id);
    LOCAL_STEP(imitation_local_profile(source,id,&profile));
    part_number(command->arg[6],&profile.offset[0]);
    part_number(command->arg[7],&profile.offset[1]);
    stage=L"在目标的独立副本上建立关联草绘";
    LOCAL_STEP(imitation_plane(target,command->arg[4],point,normal));
    LOCAL_STEP(fab_scratch(&work));
    LOCAL_STEP(imitation_result_name(target,work.parent,name,destination));
    LOCAL_STEP(dv_stage_enter(&work));
    LOCAL_STEP(imitation_native_new(target,name,snapshot->subtype,&copy,&out));
    swprintf_s(sketch,_countof(sketch),L"BWTL_LOCAL_SK_%lX",(unsigned long)GetTickCount());
    swprintf_s(cut,_countof(cut),L"BWTL_LOCAL_CUT_%lX",(unsigned long)GetTickCount());
    LOCAL_STEP(qa_geometry_mass(copy,&before));
    LOCAL_STEP(smx_custom_sketch(copy,sketch,command->arg[4],command->arg[5],imitation_local_draw,&profile,frame));
    stage=L"贯穿切除及实际去料体积核对";
    LOCAL_STEP(imitation_operation(copy,&out,0,L"CREO_OP SMT_CUT %ls %ls SKETCH:%ls %ls",
        name,cut,sketch,command->arg[8]));
    LOCAL_STEP(qa_geometry_mass(copy,&after));
    double actual=(before.volume-after.volume)*mm*mm*mm;
    double expected=profile.area*thickness;
    if(fabs(actual-expected)>fmax(0.005,expected*1e-5)){
        part_text_add(&out,L"预期去料 %.8g mm³，实测 %.8g mm³。\n",expected,actual);
        error=PRO_TK_INCOMPLETE;goto done;
    }
    stage=L"核对目标原件版本与工作目录";
    LOCAL_STEP(ProDirectoryChange(work.parent));
    work.in_stage=0;
    LOCAL_STEP(part_check_binding(command,snapshot,&target));
done:
    if(error&&copy)ProMdlErase(copy);
    if(error)part_text_add(&out,L"局部仿照停在「%ls」，Creo %d。\n",stage,error);
    error=fab_discard(&work,error,&out);
    if(error)return error;
    error=dv_display_new(copy);
    part_text_add(&out,L"%ls.prt 已生成%ls，实际去料 %.8g mm³，草绘和切口可编辑。结果尚未保存。%ls",
        name,profile.circle_count?L"圆孔组":L"矩形窗口",actual,error?L"可从「在会话中」打开。":L"");
    return error;
#undef LOCAL_STEP
}
