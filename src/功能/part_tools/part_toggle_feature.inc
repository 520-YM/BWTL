static ProError part_toggle_feature(const PartCommand *cmd,const PartSnapshot *s,ProMdl model,wchar_t *text,size_t capacity){
    int id,type,*ids=NULL,*children=NULL,n=0,i;ProFeature f;ProFeatStatus state,desired;
    ProFeatureDeleteOptions *del=NULL;ProFeatureResumeOptions *resume=NULL;ProError e,rollback=0;const PartFeature *old;
    part_integer(cmd->arg[2],&id);part_integer(cmd->arg[3],&type);old=part_find_feature(s,id);
    /* First solids and datums are deliberately excluded. */
    if(type!=PRO_FEAT_HOLE&&type!=PRO_FEAT_ROUND&&type!=PRO_FEAT_CHAMFER&&type!=PRO_FEAT_CUT)return PRO_TK_BAD_INPUTS;
    desired=cmd->verb==PART_SUPPRESS?PRO_FEAT_SUPPRESSED:PRO_FEAT_ACTIVE;
    if(!old||old->type!=type||old->status!=(cmd->verb==PART_SUPPRESS?PRO_FEAT_ACTIVE:PRO_FEAT_SUPPRESSED))return PRO_TK_BAD_CONTEXT;
    if((e=ProFeatureInit((ProSolid)model,id,&f))||(e=ProFeatureTypeGet(&f,&i))||i!=type)return PRO_TK_BAD_CONTEXT;
    if((e=ProFeatureChildrenGet(&f,&children,&n)))return e;
    if(children)ProArrayFree((ProArray *)&children);if(n)return PRO_TK_BAD_CONTEXT;
    if((e=ProArrayAlloc(1,sizeof(int),1,(ProArray *)&ids)))goto done;ids[0]=id;
    if((e=ProArrayAlloc(1,sizeof(*del),1,(ProArray *)&del)))goto done;del[0]=PRO_FEAT_DELETE_NO_OPTS;
    if((e=ProArrayAlloc(1,sizeof(*resume),1,(ProArray *)&resume)))goto done;resume[0]=PRO_FEAT_RESUME_NO_OPTS;
    e=cmd->verb==PART_SUPPRESS?ProFeatureWithoptionsSuppress((ProSolid)model,ids,del,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL):
        ProFeatureWithoptionsResume((ProSolid)model,ids,resume,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
    if(!e)e=part_regenerate_context(model,s);
    if(!e)e=ProFeatureStatusGet(&f,&state);
    if(!e&&state!=desired)e=PRO_TK_GENERAL_ERROR;
    if(e){
        if(ProFeatureStatusGet(&f,&state))rollback=1;
        else if(state!=old->status){
            ProError r=cmd->verb==PART_SUPPRESS?ProFeatureWithoptionsResume((ProSolid)model,ids,resume,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL):
                ProFeatureWithoptionsSuppress((ProSolid)model,ids,del,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
            if(r)rollback=1;
        }
        if(part_regenerate_context(model,s)||ProFeatureStatusGet(&f,&state)||state!=old->status)rollback=1;
        swprintf_s(text,capacity,L"特征状态修改失败，错误码%d；%ls；未保存。",e,rollback?L"未能完整验证回滚，请检查模型":L"原状态已恢复并验证");
    }else swprintf_s(text,capacity,L"零件 %ls 的特征%d已%ls，重生成及状态读回通过；未保存。",s->name,id,cmd->verb==PART_SUPPRESS?L"抑制":L"恢复");
done:
    if(ids)ProArrayFree((ProArray *)&ids);if(del)ProArrayFree((ProArray *)&del);if(resume)ProArrayFree((ProArray *)&resume);return e;
}
