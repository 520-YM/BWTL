
        ProMdlName target;wcscpy_s(target,PRO_NAME_SIZE,cmd->arg[2]);
        if(!part_target_available(model,target,L"prt")){e=PRO_TK_BAD_CONTEXT;break;}
        e=ProMdlnameRename(model,target);if(!e)e=ProMdlMdlnameGet(model,name);
        if(!e&&_wcsicmp(name,target))e=PRO_TK_GENERAL_ERROR;
        if(e){ProMdlName old;wcscpy_s(old,PRO_NAME_SIZE,snapshot->name);if(ProMdlnameRename(model,old))wcscpy_s(text,capacity,L"重命名失败且恢复原名未成功，请检查模型；未保存。");}
        else swprintf_s(text,capacity,L"已将 %ls 重命名为 %ls，名称读回通过；未保存，也未删除原磁盘文件。",snapshot->name,name);break;
    
