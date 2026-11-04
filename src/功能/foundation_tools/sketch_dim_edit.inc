
        const wchar_t *kind=cmd->arg[3];count=!wcscmp(cmd->arg[5],L"NONE")?1:2;
        for(i=0;i<count;i++)if((e=fd_entity_reference(s.section,cmd->arg[4+i],&ids[i],&senses[i])))goto done;
        if(!wcscmp(kind,L"LENGTH"))dimension=PRO_TK_DIM_LINE;
        else if(!wcscmp(kind,L"DIAMETER"))dimension=PRO_TK_DIM_DIA;
        else if(!wcscmp(kind,L"RADIUS"))dimension=PRO_TK_DIM_RAD;
        else if(!wcscmp(kind,L"ANGLE"))dimension=PRO_TK_DIM_LINES_ANGLE;
        else if(!wcscmp(kind,L"DISTANCE_X"))dimension=PRO_TK_DIM_PNT_PNT_HORIZ;
        else if(!wcscmp(kind,L"DISTANCE_Y"))dimension=PRO_TK_DIM_PNT_PNT_VERT;
        else dimension=PRO_TK_DIM_PNT_PNT;
        if(dimension==PRO_TK_DIM_LINE||dimension==PRO_TK_DIM_LINES_ANGLE){
            for(i=0;i<count;i++){Pro2dEntdef *ent=NULL;if((e=ProSectionEntityGet(s.section,ids[i],&ent)))goto done;if((ent->type!=PRO_2D_LINE&&ent->type!=PRO_2D_CENTER_LINE)||senses[i]!=PRO_ENT_WHOLE){e=PRO_TK_INVALID_TYPE;goto done;}}
        }else if(dimension==PRO_TK_DIM_RAD||dimension==PRO_TK_DIM_DIA){
            Pro2dEntdef *ent=NULL;if((e=ProSectionEntityGet(s.section,ids[0],&ent)))goto done;
            if((ent->type!=PRO_2D_CIRCLE&&ent->type!=PRO_2D_ARC)||senses[0]!=PRO_ENT_WHOLE){e=PRO_TK_INVALID_TYPE;goto done;}
        }else if(senses[0]==PRO_ENT_WHOLE||senses[1]==PRO_ENT_WHOLE){e=PRO_TK_INVALID_TYPE;goto done;}
        part_number(cmd->arg[6],&value);if(dimension!=PRO_TK_DIM_LINES_ANGLE)value*=state.scale;
        stage="create or update strong sketch dimension";if((e=fd_dimension_create(s.section,dimension,count,ids,senses,value,state.scale,&result_id)))goto done;
    
