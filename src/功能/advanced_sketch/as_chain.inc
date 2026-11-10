typedef struct {ProGeomitem items[AS_LIMIT];int n;} AsCurveList;

static ProError as_collect_curve(ProGeomitem *item,ProError status,ProAppData data){
    AsCurveList *curves=data;
    ProCurve curve;
    ProEnttype type;
    ProError e;
    if(status)return status;
    if((e=ProGeomitemToCurve(item,&curve))||(e=ProCurveTypeGet(curve,&type)))return e;
    if(type==PRO_ENT_CMP_CRV)return PRO_TK_NO_ERROR;
    if(curves->n>=AS_LIMIT)return PRO_TK_BAD_INPUTS;
    curves->items[curves->n++]=*item;
    return PRO_TK_NO_ERROR;
}

static ProError as_chain_ends(FdRef *ref,ProVector a,ProVector b){
    ProVector deriv[3];
    ProError e;
    if(ref->item.type==PRO_EDGE){
        ProEdge edge;
        if((e=ProGeomitemToEdge(&ref->item,&edge))||(e=ProEdgeXyzdataEval(edge,0,a,deriv[0],deriv[1],deriv[2])))return e;
        return ProEdgeXyzdataEval(edge,1,b,deriv[0],deriv[1],deriv[2]);
    }
    {
        ProCurve curve;
        if((e=ProGeomitemToCurve(&ref->item,&curve))||(e=ProCurveXyzdataEval(curve,0,a,NULL,NULL)))return e;
        return ProCurveXyzdataEval(curve,1,b,NULL,NULL);
    }
}

static ProError as_chain(ProMdl model,FdSketch *s,const PartCommand *cmd,double scale,AsEdit *edit){
    AsCurveList list={0};
    ProVector ends[AS_LIMIT][2],last;
    int used[AS_LIMIT]={0};
    const int by_curve=cmd->count==4;
    const double tol=0.001*scale;
    int i,j,k,sketch_number,start=0,first_end=0;
    ProError e;
    if((e=ProFeatureNumberGet(&s->feature,&sketch_number)))return e;
    if(by_curve){
        ProFeature feature;
        if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)cmd->arg[3]+6,(ProModelitem *)&feature))||
           (e=ProFeatureGeomitemVisit(&feature,PRO_CURVE,as_collect_curve,NULL,&list)))return e;
        edit->ref_count=list.n;
        for(i=0;i<list.n;i++){
            FdRef *ref=&edit->references[i];
            ref->item=list.items[i];ref->owner_feature=feature.id;
            if((e=ProSelectionAlloc(NULL,(ProModelitem *)&ref->item,&ref->selection)))return e;
        }
    }else{
        edit->ref_count=cmd->count-3;
        for(i=0;i<edit->ref_count;i++)if((e=fd_ref_resolve(model,cmd->arg[i+3],PRO_EDGE,&edit->references[i])))return e;
    }
    if(edit->ref_count<2)return PRO_TK_BAD_INPUTS;
    for(i=0;i<edit->ref_count;i++){
        ProFeature owner;
        int number;
        FdRef *ref=&edit->references[i];
        if((e=ProFeatureInit((ProSolid)model,ref->owner_feature,&owner))||(e=ProFeatureNumberGet(&owner,&number))||
           (e=as_chain_ends(ref,ends[i][0],ends[i][1])))return e;
        if(number>=sketch_number||ad_near(ends[i][0],ends[i][1],tol))return PRO_TK_BAD_CONTEXT;
    }
    /* Reject branches and find an open-chain endpoint when reading a feature. */
    for(i=0;i<edit->ref_count;i++)for(j=0;j<2;j++){
        int joins=0;
        for(k=0;k<edit->ref_count;k++)if(k!=i)
            joins+=ad_near(ends[i][j],ends[k][0],tol)+ad_near(ends[i][j],ends[k][1],tol);
        if(joins>1)return PRO_TK_E_AMBIGUOUS;
        if(by_curve&&!joins){start=i;first_end=j;}
    }
    if(!by_curve&&(ad_near(ends[0][0],ends[1][0],tol)||ad_near(ends[0][0],ends[1][1],tol)))first_end=1;
    j=start;
    for(i=0;i<edit->ref_count;i++){
        if(i){
            for(j=by_curve?0:i;j<(by_curve?edit->ref_count:i+1);j++){
                if(used[j])continue;
                if(ad_near(last,ends[j][0],tol)){first_end=0;break;}
                if(ad_near(last,ends[j][1],tol)){first_end=1;break;}
            }
            if(j==(by_curve?edit->ref_count:i+1))return PRO_TK_BAD_INPUTS;
        }
        used[j]=1;
        memcpy(last,ends[j][1-first_end],sizeof(last));
        if((e=ProSectionEntityUseEdge(s->section,edit->references[j].selection,&edit->ids[i])))return e;
        edit->count++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError as_verify_chain(ProMdl model,FdSketch *check,const AsEdit *edit,const AsSnapshot *actual,double tol){
    const AsEntity *a,*b;
    ProError e;
    int i;
    for(i=0;i<edit->ref_count;i++)if((e=sd_projection_reference(model,&check->feature,&edit->references[i])))return e;
    for(i=1;i<edit->count;i++){
        a=as_find(actual,edit->ids[i-1]);b=as_find(actual,edit->ids[i]);
        if(!a||!b||!a->sampled||!b->sampled||
           !(as_near(a->points[0],b->points[0],tol)||as_near(a->points[0],b->points[4],tol)||
             as_near(a->points[4],b->points[0],tol)||as_near(a->points[4],b->points[4],tol)))return PRO_TK_GENERAL_ERROR;
    }
    return PRO_TK_NO_ERROR;
}
