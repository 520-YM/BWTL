
        ProMdl target=NULL;ProMdlName target_name;ProPath dir,path;int window;
        wcscpy_s(target_name,PRO_NAME_SIZE,cmd->arg[2]);e=ProMdlnameInit(target_name,PRO_MDL_PART,&target);
        if(e){
            e=ProDirectoryCurrentGet(dir);
            if(!e&&swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls.prt",dir,target_name)<0)e=PRO_TK_BAD_INPUTS;
            if(!e)e=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&target);
        }
        if(!e)e=ProMdlMdlnameGet(target,name);
        if(!e&&_wcsicmp(name,target_name))e=PRO_TK_BAD_CONTEXT;
        if(!e)e=ProObjectwindowMdlnameCreate(target_name,PRO_PART,&window);
        if(!e)e=ProWindowCurrentSet(window);
        if(!e)e=ProMdlDisplay(target);
        if(!e)e=ProWindowActivate(window);
        if(!e)swprintf_s(text,capacity,L"已请求显示并激活零件 %ls，未保存或修改模型数据。",target_name);break;
    
