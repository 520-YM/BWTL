
        ProMdl copy=NULL;ProMdlName target;ProMdlType type;
        if(!part_target_available(model,cmd->arg[2],L"prt")){e=PRO_TK_BAD_CONTEXT;break;}
        wcscpy_s(target,PRO_NAME_SIZE,cmd->arg[2]);e=ProMdlnameCopy(model,target,&copy);
        if(!e)e=ProMdlMdlnameGet(copy,name);if(!e)e=ProMdlTypeGet(copy,&type);
        if(!e&&(_wcsicmp(name,target)||type!=PRO_MDL_PART))e=PRO_TK_GENERAL_ERROR;
        if(!e)e=part_regenerate(copy);
        if(!e)swprintf_s(text,capacity,L"已复制 %ls 为 %ls.prt，类型及重生成检查通过；副本未保存，也未切换当前窗口。",snapshot->name,name);break;
    
