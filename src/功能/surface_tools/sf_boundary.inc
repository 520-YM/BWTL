static ProError sf_boundary_curve(ProMdl model,const wchar_t *name,FdRef *ref){
    SxPath path={0};
    ProError e=sx_path_resolve(model,name,1,&path);
    if(!e){
        ref->owner_feature=path.parent;
        e=ProModelitemInit(model,path.parent,PRO_FEATURE,&ref->item);
        if(!e)e=ProSelectionAlloc(NULL,(ProModelitem *)&ref->item,&ref->selection);
    }
    sx_path_free(&path);
    return e;
}
static ProError sf_boundary(FdTxn *tx,const PartCommand *cmd){
    FdRef refs[2]={{0}};
    const wchar_t *prompts[]={L"FIRST_CURVE",L"SECOND_CURVE"};
    ProError e;
    for(int i=0;i<2;i++)if((e=sf_boundary_curve(tx->model,cmd->arg[3+i]+6,&refs[i])))goto done;
    e=sf_udf(tx,L"bwtl_boundary",refs,prompts,2);
done:
    for(int i=0;i<2;i++)fd_ref_free(&refs[i]);
    return e;
}
