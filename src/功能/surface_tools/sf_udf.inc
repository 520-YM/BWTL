static ProError sf_udf(FdTxn *tx,const wchar_t *file,FdRef *refs,const wchar_t **prompts,int count){
    ProPath resources,path;
    ProUdfdata udf=NULL;
    ProUdfRequiredRef *required=NULL;
    ProUdfreference bindings[4]={0};
    ProUdfCreateOption placement=PROUDFOPT_FIX_MODEL_UI_OFF;
    ProGroup group={0};
    ProFeature *members=NULL,*placed=NULL;
    int n=0,m=0,bound=0;
    ProError e;
#define UDF_TRY(label,call) do{tx->stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    if(count<1||count>4)return PRO_TK_BAD_INPUTS;
    UDF_TRY("locate native surface template",ProToolkitApplTextPathGet(resources));
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\udf\\%ls.gph.1",resources,file)<0||
       GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES){e=PRO_TK_CANT_OPEN;goto done;}
    UDF_TRY("allocate surface template",ProUdfdataAlloc(&udf));
    UDF_TRY("open native surface template",ProUdfdataPathSet(udf,path));
    UDF_TRY("make placed feature independent from template file",ProUdfdataDependencySet(udf,PROUDFDEPENDENCY_INDEPENDENT));
    UDF_TRY("read surface template references",ProUdfdataRequiredreferencesGet(udf,&required));
    UDF_TRY("count surface template references",ProArraySizeGet((ProArray)required,&n));
    if(n!=count){e=PRO_TK_BAD_INPUTS;goto done;}
    for(int i=0;i<n;i++){
        ProLine prompt;
        int slot=-1;
        UDF_TRY("read native surface reference prompt",ProUdfrequiredrefPromptGet(required[i],prompt));
        for(int j=0;j<count;j++)if(!wcscmp(prompt,prompts[j]))slot=j;
        if(slot<0||(bound&(1<<slot))){e=PRO_TK_BAD_INPUTS;goto done;}
        bound|=1<<slot;
        UDF_TRY("bind actual surface reference",ProUdfreferenceAlloc(prompt,refs[slot].selection,PRO_B_FALSE,&bindings[i]));
        UDF_TRY("append surface reference",ProUdfdataReferenceAdd(udf,bindings[i]));
    }
    e=ProUdfCreate((ProSolid)tx->model,udf,NULL,&placement,1,&group);
    trace("place native surface template",e);
    if(group.owner==tx->model&&group.id>0&&!fd_was_feature(tx,group.id)){tx->feature=group;tx->created=1;}
    if(e)goto done;
    if(!tx->created){e=PRO_TK_GENERAL_ERROR;goto done;}
    UDF_TRY("read created native surface group",ProGroupFeaturesCollect(&group,&members));
    UDF_TRY("count surface group members",ProArraySizeGet((ProArray)members,&m));
    if(m!=2){e=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<m;i++)if(members[i].id!=group.id){
        ProFeattype type;
        if(placed||fd_was_feature(tx,members[i].id)){e=PRO_TK_GENERAL_ERROR;goto done;}
        UDF_TRY("read native surface feature type",ProFeatureTypeGet(&members[i],&type));
        if(type!=PRO_FEAT_DATUM_SURF&&type!=PRO_FEAT_OFFSET&&type!=PRO_FEAT_EXTEND){e=PRO_TK_INVALID_TYPE;goto done;}
        placed=&members[i];
    }
    if(!placed){e=PRO_TK_E_NOT_FOUND;goto done;}
    UDF_TRY("name native surface feature",ProModelitemNameSet((ProModelitem *)placed,tx->name));
    UDF_TRY("ungroup placed surface feature",ProGroupUngroup(&group));
    tx->feature=*placed;
    for(int i=0;i<count;i++)UDF_TRY("verify native surface source reference",sd_projection_reference(tx->model,&tx->feature,&refs[i]));
done:
    if(members)ProArrayFree((ProArray *)&members);
    if(required)ProUdfrequiredrefProarrayFree(required);
    for(int i=0;i<4;i++)if(bindings[i])ProUdfreferenceFree(bindings[i]);
    if(udf)ProUdfdataFree(udf);
    return e;
#undef UDF_TRY
}
