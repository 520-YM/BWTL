
        stage="remove explicitly requested sketch item";part_integer(cmd->arg[4],&id);
        if(!wcscmp(cmd->arg[3],L"ENTITY")){if(original_count<=1){e=PRO_TK_BAD_CONTEXT;goto done;}e=ProSectionEntityDelete(s.section,id);}
        else if(!wcscmp(cmd->arg[3],L"DIMENSION"))e=ProSecdimDelete(s.section,id);
        else e=ProSectionConstraintDelete(s.section,id);if(e)goto done;
    
