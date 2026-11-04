
        const wchar_t *kind=cmd->arg[3];count=cmd->count-4;
        if(!wcscmp(kind,L"HORIZONTAL"))constraint=PRO_CONSTRAINT_HORIZONTAL_ENT;
        else if(!wcscmp(kind,L"VERTICAL"))constraint=PRO_CONSTRAINT_VERTICAL_ENT;
        else if(!wcscmp(kind,L"PARALLEL"))constraint=PRO_CONSTRAINT_PARALLEL_ENTS;
        else if(!wcscmp(kind,L"PERPENDICULAR"))constraint=PRO_CONSTRAINT_ORTHOG_ENTS;
        else if(!wcscmp(kind,L"TANGENT"))constraint=PRO_CONSTRAINT_TANGENT_ENTS;
        else if(!wcscmp(kind,L"EQUAL_LENGTH"))constraint=PRO_CONSTRAINT_EQUAL_SEGMENTS;
        else if(!wcscmp(kind,L"EQUAL_RADIUS"))constraint=PRO_CONSTRAINT_EQUAL_RADII;
        else if(!wcscmp(kind,L"SYMMETRY"))constraint=PRO_CONSTRAINT_SYMMETRY;
        for(i=0;i<count;i++)if((e=fd_entity_reference(s.section,cmd->arg[4+i],&ids[i],&senses[i])))goto done;
        stage="create and read back sketch constraint";if((e=fd_constraint_create(s.section,constraint,count,ids,senses,&result_id)))goto done;
    
