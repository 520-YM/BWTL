static ProError part_check_binding(const PartCommand *cmd,const PartSnapshot *s,ProMdl *model){
    ProMdlName name;ProWVerstamp stamp=NULL;ProPath dir;ProError e;int equal;
    {ProMdl window=NULL;if((e=ProMdlCurrentGet(&window)))return e;if(window!=s->window_model)return PRO_TK_BAD_CONTEXT;}
    if(s->model==s->window_model){if((e=ProMdlCurrentGet(model)))return e;}
    else if((e=part_target_get(model)))return e;
    if(*model!=s->model || (e=ProMdlMdlnameGet(*model,name)) || _wcsicmp(name,cmd->arg[1]) || _wcsicmp(name,s->name))return PRO_TK_BAD_CONTEXT;
    if((e=ProDirectoryCurrentGet(dir)) || _wcsicmp(dir,s->directory))return PRO_TK_BAD_CONTEXT;
    if((e=ProMdlVerstampGet(*model,&stamp)))return e;
    equal=ProVerstampEqual(stamp,s->stamp);ProVerstampFree(&stamp);
    if(equal && s->in_assembly){
        if((e=ProMdlVerstampGet(s->window_model,&stamp)))return e;
        equal=ProVerstampEqual(stamp,s->parent_stamp);ProVerstampFree(&stamp);
    }
    return equal?0:PRO_TK_BAD_CONTEXT;
}
