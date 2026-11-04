
        Pro2dEntdef *ent=NULL;stage="translate existing entity";
        if((e=fd_entity_reference(s.section,cmd->arg[3],&id,&sense))||(e=ProSectionEntityGet(s.section,id,&ent)))goto done;type=ent->type;
        if(type==PRO_2D_LINE)line=*(Pro2dLinedef *)ent;else if(type==PRO_2D_CIRCLE)circle=*(Pro2dCircledef *)ent;else if(type==PRO_2D_ARC)arc=*(Pro2dArcdef *)ent;else{e=PRO_TK_INVALID_TYPE;goto done;}
        part_number(cmd->arg[4],&dx);part_number(cmd->arg[5],&dy);vector[0]=dx*scale;vector[1]=dy*scale;
        {
            if((e=ProSectionEntityTranslate(s.section,id,vector)))goto done;
            if(!ProSectionEntityGet(s.section,id,&ent)&&type==PRO_2D_CIRCLE){trace("translated circle x*1000",(int)lround(((Pro2dCircledef *)ent)->center[0]*1000));trace("translated circle y*1000",(int)lround(((Pro2dCircledef *)ent)->center[1]*1000));}
        }
    
