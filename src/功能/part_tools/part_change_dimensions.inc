static ProError part_change_dimensions(const PartCommand *cmd,const PartSnapshot *s,ProMdl model,wchar_t *text,size_t capacity){
    ProDimension dims[8];double old[8],next[8],value;ProFeature feature;ProFeattype type;ProFeatStatus status;
    int n=(cmd->count-3)/3,id,i,j,changed=0;ProError e=0,rollback=0;const PartFeature *expected;
    part_integer(cmd->arg[2],&id);expected=part_find_feature(s,id);
    if(!expected||expected->status!=PRO_FEAT_ACTIVE)return PRO_TK_BAD_CONTEXT;
    if((e=ProFeatureInit((ProSolid)model,id,&feature))||(e=ProFeatureTypeGet(&feature,&type))||
       (e=ProFeatureStatusGet(&feature,&status)))return e;
    if(type!=expected->type||status!=expected->status)return PRO_TK_BAD_CONTEXT;
    for(i=0;i<n;i++){
        const PartDimension *d=NULL;ProBoolean related,negative;ProFeature owner;ProName symbol;
        part_number(cmd->arg[4+3*i],&old[i]);part_number(cmd->arg[5+3*i],&next[i]);
        for(j=0;j<s->dimensions;j++)if(!_wcsicmp(s->dimension[j].symbol,cmd->arg[3+3*i])){d=&s->dimension[j];break;}
        if(!d||d->feature!=id||d->related||d->negative||next[i]<0||old[i]<0||!part_close(d->value,old[i]))return PRO_TK_BAD_CONTEXT;
        if(d->type!=PRODIMTYPE_LINEAR&&d->type!=PRODIMTYPE_RADIUS&&d->type!=PRODIMTYPE_DIAMETER&&d->type!=PRODIMTYPE_ANGLE&&d->type!=PRODIMTYPE_ARC_LENGTH)return PRO_TK_BAD_CONTEXT;
        if((d->type==PRODIMTYPE_DIAMETER||d->type==PRODIMTYPE_RADIUS||d->type==PRODIMTYPE_ARC_LENGTH)&&next[i]<=0)return PRO_TK_BAD_INPUTS;
        if(d->type==PRODIMTYPE_ANGLE && fabs(next[i])>360)return PRO_TK_BAD_INPUTS;
        dims[i]=d->handle;
        if((e=ProDimensionValueGet(&dims[i],&value))||(e=ProDimensionIsReldriven(&dims[i],&related))||
           (e=ProDimensionIsRegenednegative(&dims[i],&negative))||(e=ProDimensionOwnerfeatureGet(&dims[i],&owner))||(e=ProDimensionSymbolGet(&dims[i],symbol)))return e;
        if(related||negative||owner.id!=id||!part_close(value,old[i])||_wcsicmp(symbol,cmd->arg[3+3*i]))return PRO_TK_BAD_CONTEXT;
    }
    for(i=0;i<n;i++){
        /* Include even a failed setter in restoration: the SDK may have
           entered a partially changed state before returning its error. */
        changed=i+1;e=ProDimensionValueSet(&dims[i],next[i]);if(e)goto undo;
    }
    if((e=part_regenerate_context(model,s)))goto undo;
    if(s->has_volume && s->volume>0){
        ProMassProperty mass;e=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass);
        if(e||!isfinite(mass.volume)||mass.volume<=0){if(!e)e=PRO_TK_GENERAL_ERROR;goto undo;}
    }
    for(i=0;i<n;i++)if((e=ProDimensionValueGet(&dims[i],&value))||!part_close(value,next[i])){if(!e)e=PRO_TK_GENERAL_ERROR;goto undo;}
    swprintf_s(text,capacity,L"已修改零件 %ls 的特征 %d，共%d个尺寸；重生成及逐项读回通过。修改保留在内存，未自动保存。",s->name,id,n);return 0;
undo:
    for(i=changed-1;i>=0;i--)if(ProDimensionValueSet(&dims[i],old[i]))rollback=1;
    if(part_regenerate_context(model,s))rollback=1;
    for(i=0;i<changed;i++)if(ProDimensionValueGet(&dims[i],&value)||!part_close(value,old[i]))rollback=1;
    swprintf_s(text,capacity,L"尺寸修改失败，错误码%d；%ls。未保存。",e,rollback?L"回滚未能完整验证，请检查当前模型":L"旧尺寸已恢复并读回验证");return e;
}
