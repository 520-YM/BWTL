typedef struct {ProGeomitem items[AS_LIMIT];int count;} SxCurveSet;
typedef struct {FdRef items[AS_LIMIT];int count,parent;ProVector first,last;} SxPath;

static ProError sx_curve_item(ProGeomitem *item,ProError status,ProAppData data){
    SxCurveSet *set=data;
    ProCurve curve=NULL;
    ProEnttype type;
    ProError e;
    if(status)return status;
    if((e=ProGeomitemToCurve(item,&curve))||(e=ProCurveTypeGet(curve,&type)))return e;
    if(type!=PRO_ENT_CMP_CRV){
        if(set->count>=AS_LIMIT)return PRO_TK_BAD_INPUTS;
        set->items[set->count++]=*item;
    }
    return PRO_TK_NO_ERROR;
}

static ProError sx_path_resolve(ProMdl model,const wchar_t *name,double scale,SxPath *path){
    ProFeature feature;
    SxCurveSet set={0};
    ProCurve curves[AS_LIMIT];
    ProVector ends[AS_LIMIT][2];
    int start=-1,side=0,open=0,used[AS_LIMIT]={0};
    double tolerance=0.001*scale;
    ProError e;
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,(ProModelitem *)&feature))||
       (e=ProFeatureGeomitemVisit(&feature,PRO_CURVE,sx_curve_item,NULL,&set)))return e;
    path->parent=feature.id;
    for(int i=0;i<set.count;i++)
        if((e=ProGeomitemToCurve(&set.items[i],&curves[i]))||
           (e=ProCurveXyzdataEval(curves[i],0,ends[i][0],NULL,NULL))||
           (e=ProCurveXyzdataEval(curves[i],1,ends[i][1],NULL,NULL)))return e;
    for(int i=0;i<set.count;i++){
        if(ad_near(ends[i][0],ends[i][1],tolerance))return PRO_TK_BAD_INPUTS;
        for(int k=0;k<2;k++){
            int neighbors=0;
            for(int j=0;j<set.count;j++)if(i!=j)
                for(int q=0;q<2;q++)if(ad_near(ends[i][k],ends[j][q],tolerance))neighbors++;
            if(neighbors>1)return PRO_TK_E_AMBIGUOUS;
            if(!neighbors){if(start<0){start=i;side=k;}open++;}
        }
    }
    if(open!=2)return PRO_TK_BAD_INPUTS;
    while(start>=0){
        FdRef *ref=&path->items[path->count];
        if(!path->count)memcpy(path->first,ends[start][side],sizeof(path->first));
        memcpy(path->last,ends[start][1-side],sizeof(path->last));
        ref->item=set.items[start];
        ref->owner_feature=feature.id;
        if((e=ProSelectionAlloc(NULL,(ProModelitem *)&ref->item,&ref->selection)))return e;
        path->count++;
        used[start]=1;
        start=-1;
        for(int i=0;i<set.count;i++)if(!used[i])
            for(int k=0;k<2;k++)if(ad_near(path->last,ends[i][k],tolerance)){start=i;side=k;}
    }
    return path->count==set.count?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
}

static ProError sx_curve_collection(ProElement element,const SxPath *path){
    ProCrvcollinstr instructions[AS_LIMIT]={0};
    ProReference references[AS_LIMIT]={0};
    ProCollection collection=NULL;
    ProError e;
    if(path->count==1)return sd_curve_collection(element,&path->items[0]);
    e=ProCrvcollectionAlloc(&collection);
    if(e)return e;
    for(int i=0;i<path->count;i++){
        if((e=ProSelectionToReference(path->items[i].selection,&references[i]))||
           (e=ProCrvcollinstrAlloc(PRO_CURVCOLL_ADD_ONE_INSTR,&instructions[i]))||
           (e=ProCrvcollinstrReferenceAdd(instructions[i],references[i]))||
           (e=ProCrvcollectionInstructionAdd(collection,instructions[i])))break;
    }
    if(!e)e=ProElementCollectionSet(element,collection);
    ProCollectionFree(&collection);
    for(int i=0;i<path->count;i++){
        if(instructions[i])ProCrvcollinstrFree(instructions[i]);
        if(references[i])ProReferenceFree(references[i]);
    }
    return e;
}
