static ProError sd_blend_composite(ProGeomitem *item,ProError status,ProAppData data){
    FdCollect *found=data;
    ProCurve curve=NULL;
    ProEnttype type;
    ProError e;
    if(status)return status;
    if((e=ProCurveInit((ProSolid)item->owner,item->id,&curve))||(e=ProCurveTypeGet(curve,&type)))return e;
    if(type==PRO_ENT_CMP_CRV){found->item=*item;found->count++;}
    return PRO_TK_NO_ERROR;
}

static ProError sd_blend_section(ProMdl model,const wchar_t *name,FdRef *ref,ProMatrix transform,int circle_only){
    FdSketch sketch={0};
    FdCollect found={0};
    Pro2dEntdef *entity=NULL;
    ProSectionShape shape;
    int *ids=NULL,count=0;
    ProError e;
    if((e=fd_sketch_open(model,name,&sketch,0))||
       (e=ProSectionShapeGet(sketch.section,&shape))||
       (e=ProSectionEntityIdsGet(sketch.section,&ids,&count)))goto done;
    if(shape!=PRO_SECSHAPE_1_CLOSED_LOOP||count<1||(circle_only&&count!=1)){e=PRO_TK_BAD_INPUTS;goto done;}
    if(circle_only&&(e=ProSectionEntityGet(sketch.section,ids[0],&entity)))goto done;
    if(circle_only&&entity->type!=PRO_2D_CIRCLE){e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=ProSectionLocationGet(sketch.section,transform))||
       (e=ProFeatureGeomitemVisit(&sketch.feature,PRO_CURVE,sd_blend_composite,NULL,&found)))goto done;
    if(found.count!=1){e=found.count?PRO_TK_E_AMBIGUOUS:PRO_TK_E_NOT_FOUND;goto done;}
    ref->item=found.item;
    ref->owner_feature=sketch.feature.id;
    e=ProSelectionAlloc(NULL,(ProModelitem *)&ref->item,&ref->selection);
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&sketch);
    return e;
}

static ProError sd_blend_sections(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity,int circle_only){
    FdTxn tx={0};
    FdRef profiles[2]={{0}};
    ProMatrix position[2];
    ProPath resources,path;
    ProSolidBody body;
    ProSelection body_selection=NULL;
    ProUdfdata udf=NULL;
    ProUdfreference references[3]={0};
    ProUdfRequiredRef *required=NULL;
    ProUdfCreateOption placement=PROUDFOPT_FIX_MODEL_UI_OFF;
    ProGroup group={0};
    ProFeature *members=NULL;
    ProFeature *blend=NULL;
    int body_count=0,count,i,parents[2];
    unsigned bound=0;
    double normal_dot=0,separation=0,volume;
    ProError e;
#define BLEND_TRY(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    BLEND_TRY("validate parallel blend",fd_begin(&tx,model,cmd->arg[2],PRO_FEAT_PROTRUSION));
    BLEND_TRY("read blend body count",sd_body_count(model,&body_count));
    if(body_count>1||(body_count==1&&(!tx.has_volume||!isfinite(tx.volume)||tx.volume<=0))){e=PRO_TK_BAD_CONTEXT;goto done;}
    BLEND_TRY("resolve first closed blend section",sd_blend_section(model,cmd->arg[3],&profiles[0],position[0],circle_only));
    BLEND_TRY("resolve second closed blend section",sd_blend_section(model,cmd->arg[4],&profiles[1],position[1],circle_only));
    tx.stage="check distinct parallel section planes";
    for(i=0;i<3;i++){
        normal_dot+=position[0][2][i]*position[1][2][i];
        separation+=(position[1][3][i]-position[0][3][i])*position[0][2][i];
    }
    if(fabs(fabs(normal_dot)-1)>1e-6||fabs(separation)<0.001*tx.scale){e=PRO_TK_BAD_INPUTS;goto done;}
    BLEND_TRY("locate packaged native blend",ProToolkitApplTextPathGet(resources));
    if(swprintf_s(path,_countof(path),L"%ls\\udf\\bwtl_circle_blend.gph.1",resources)<0||
       GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES){e=PRO_TK_CANT_OPEN;goto done;}
    BLEND_TRY("get default blend body",ProSolidDefaultBodyGet((ProSolid)model,&body));
    BLEND_TRY("select default blend body",ProSelectionAlloc(NULL,(ProModelitem *)&body,&body_selection));
    BLEND_TRY("allocate native blend placement",ProUdfdataAlloc(&udf));
    BLEND_TRY("open native blend template",ProUdfdataPathSet(udf,path));
    BLEND_TRY("make blend feature independent",ProUdfdataDependencySet(udf,PROUDFDEPENDENCY_INDEPENDENT));
    BLEND_TRY("read template references",ProUdfdataRequiredreferencesGet(udf,&required));
    BLEND_TRY("count template references",ProArraySizeGet((ProArray)required,&count));
    if(count!=3){e=PRO_TK_GENERAL_ERROR;goto done;}
    for(i=0;i<count;i++){
        ProLine prompt;
        ProType type;
        int slot;
        BLEND_TRY("read template prompt",ProUdfrequiredrefPromptGet(required[i],prompt));
        BLEND_TRY("read template reference type",ProUdfrequiredrefTypeGet(required[i],&type));
        if(type==PRO_BODY)slot=2;
        else if(type==PRO_COMP_CRV)slot=wcscmp(prompt,L"bwtl_circle_blend")?1:0;
        else {e=PRO_TK_INVALID_TYPE;goto done;}
        if(bound&(1u<<slot)){e=PRO_TK_BAD_INPUTS;goto done;}
        bound|=1u<<slot;
        BLEND_TRY("bind native blend reference",ProUdfreferenceAlloc(prompt,slot==2?body_selection:profiles[slot].selection,PRO_B_FALSE,&references[i]));
        BLEND_TRY("attach native blend reference",ProUdfdataReferenceAdd(udf,references[i]));
    }
    tx.stage="place native parallel blend";
    e=ProUdfCreate((ProSolid)model,udf,NULL,&placement,1,&group);
    trace(tx.stage,e);
    if(group.owner==model&&group.id>0&&!fd_was_feature(&tx,group.id)){tx.feature=group;tx.created=1;}
    if(e)goto done;
    if(!tx.created){e=PRO_TK_GENERAL_ERROR;goto done;}
    BLEND_TRY("read native blend group",ProGroupFeaturesCollect(&group,&members));
    BLEND_TRY("count native blend members",ProArraySizeGet((ProArray)members,&count));
    if(count!=2){e=PRO_TK_GENERAL_ERROR;goto done;}
    for(i=0;i<count;i++){
        ProFeattype type;
        BLEND_TRY("read native blend member type",ProFeatureTypeGet(&members[i],&type));
        if(members[i].owner!=model||fd_was_feature(&tx,members[i].id)){e=PRO_TK_GENERAL_ERROR;goto done;}
        if(members[i].id==group.id&&type==PRO_FEAT_GROUP_HEAD)continue;
        if(blend||type!=PRO_FEAT_PROTRUSION){e=PRO_TK_INVALID_TYPE;goto done;}
        blend=&members[i];
    }
    if(!blend){e=PRO_TK_E_NOT_FOUND;goto done;}
    BLEND_TRY("name native blend feature",ProModelitemNameSet((ProModelitem *)blend,tx.name));
    BLEND_TRY("ungroup native blend feature",ProGroupUngroup(&group));
    tx.feature=*blend;
    parents[0]=profiles[0].owner_feature;parents[1]=profiles[1].owner_feature;
    BLEND_TRY("verify blend geometry and both sections",sd_profile_verify(&tx,0,parents,2,&volume));
    swprintf_s(report,capacity,L"已创建平行圆截面实体混合 %ls，关联草绘 %ls 和 %ls。特征 ID %d，零件体积 %.9g mm³。两条截面参照与重生成已核对，未保存。",
        cmd->arg[2],cmd->arg[3],cmd->arg[4],tx.feature.id,volume);
done:
    if(members)ProArrayFree((ProArray *)&members);
    if(required)ProUdfrequiredrefProarrayFree(required);
    for(i=0;i<3;i++)if(references[i])ProUdfreferenceFree(references[i]);
    if(udf)ProUdfdataFree(udf);
    if(body_selection)ProSelectionFree(&body_selection);
    for(i=0;i<2;i++)fd_ref_free(&profiles[i]);
    sd_profile_finish(&tx,e,body_count,report,capacity);
    return e;
#undef BLEND_TRY
}

static ProError sd_blend(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    return sd_blend_sections(cmd,model,report,capacity,1);
}