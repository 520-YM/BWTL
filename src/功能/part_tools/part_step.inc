
        ProPath dir,path,base;WIN32_FILE_ATTRIBUTE_DATA info;
        if(!part_target_available(model,cmd->arg[2],L"stp")||!part_target_available(model,cmd->arg[2],L"step")){e=PRO_TK_BAD_CONTEXT;break;}
        e=ProDirectoryCurrentGet(dir);
        if(!e&&swprintf_s(base,PRO_PATH_SIZE,L"%ls\\%ls",dir,cmd->arg[2])<0)e=PRO_TK_BAD_INPUTS;
        /* PTC explicitly requires a filename WITHOUT its extension. */
        if(!e)e=ProIntf3DFileWriteWithDefaultProfile((ProSolid)model,PRO_INTF_EXPORT_STEP,base);
        if(!e&&swprintf_s(path,PRO_PATH_SIZE,L"%ls.stp",base)<0)e=PRO_TK_BAD_INPUTS;
        if(!e&&GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES){if(swprintf_s(path,PRO_PATH_SIZE,L"%ls.step",base)<0)e=PRO_TK_BAD_INPUTS;}
        if(!e&&(!GetFileAttributesExW(path,GetFileExInfoStandard,&info)||(!info.nFileSizeLow&&!info.nFileSizeHigh)))e=PRO_TK_GENERAL_ERROR;
        if(!e)swprintf_s(text,capacity,L"STEP导出成功，已核对输出文件非空：%ls。原零件未自动保存。",path);break;
    
