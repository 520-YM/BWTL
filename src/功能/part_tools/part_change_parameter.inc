static ProError part_change_parameter(const PartCommand *cmd,const PartSnapshot *s,ProMdl model,wchar_t *text,size_t capacity){
    ProModelitem item;ProParameter parameter;ProParamvalue old,next,current;ProLockstatus lock;int i,found=0;ProError e,rollback=0;
    if(!part_param_parse(cmd->arg[3],cmd->arg[4],&old)||!part_param_parse(cmd->arg[3],cmd->arg[5],&next))return PRO_TK_BAD_INPUTS;
    for(i=0;i<s->parameters;i++)if(!_wcsicmp(s->parameter[i].name,cmd->arg[2])){
        if(s->parameter[i].lock!=PRO_PARAMLOCKSTATUS_UNLOCKED||!part_param_equal(&s->parameter[i].value,&old))return PRO_TK_BAD_CONTEXT;found=1;break;
    }if(!found)return PRO_TK_BAD_CONTEXT;
    if((e=ProMdlToModelitem(model,&item))||(e=ProParameterInit(&item,(wchar_t *)cmd->arg[2],&parameter))||
       (e=ProParameterLockstatusGet(&parameter,&lock))||(e=ProParameterValueWithUnitsGet(&parameter,&current,NULL)))return e;
    if(lock!=PRO_PARAMLOCKSTATUS_UNLOCKED||!part_param_equal(&current,&old))return PRO_TK_BAD_CONTEXT;
    e=ProParameterValueWithUnitsSet(&parameter,&next,NULL);
    if(!e)e=part_regenerate_context(model,s);
    if(!e)e=ProParameterValueWithUnitsGet(&parameter,&current,NULL);
    if(!e&&!part_param_equal(&current,&next))e=PRO_TK_GENERAL_ERROR;
    if(e){
        if(ProParameterValueWithUnitsSet(&parameter,&old,NULL)||part_regenerate_context(model,s)||
           ProParameterValueWithUnitsGet(&parameter,&current,NULL)||!part_param_equal(&current,&old))rollback=1;
        swprintf_s(text,capacity,L"参数修改失败，错误码%d；%ls；未保存。",e,rollback?L"回滚未能完整验证，请检查当前模型":L"已恢复原值并读回验证");return e;
    }
    swprintf_s(text,capacity,L"已修改 %ls 的参数 %ls，重生成及读回通过；未自动保存。",s->name,cmd->arg[2]);return 0;
}
