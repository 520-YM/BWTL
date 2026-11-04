
            pts[0][0]=v[0];pts[0][1]=v[1];pts[1][0]=v[0]+v[2];pts[1][1]=v[1];pts[2][0]=v[0]+v[2];pts[2][1]=v[1]+v[3];pts[3][0]=v[0];pts[3][1]=v[1]+v[3];pts[4][0]=v[0];pts[4][1]=v[1];
            for(i=0;i<4;i++){memcpy(line.end1,pts[i],sizeof(Pro2dPnt));memcpy(line.end2,pts[i+1],sizeof(Pro2dPnt));if((e=ProSectionEntityAdd(section,(Pro2dEntdef *)&line,&ids[i])))return e;(*count)++;}
            for(i=0;i<4;i++){
                int constraint,refs[2]={ids[i],ids[(i+1)%4]};ProSectionPointType ends[2]={PRO_ENT_END,PRO_ENT_START};
                if((e=fd_constraint_create(section,i%2?PRO_CONSTRAINT_VERTICAL_ENT:PRO_CONSTRAINT_HORIZONTAL_ENT,1,&ids[i],&whole,&constraint)))return e;
                if((e=fd_constraint_create(section,PRO_CONSTRAINT_SAME_POINT,2,refs,ends,&constraint)))return e;
            }
            if((e=fd_dimension_create(section,PRO_TK_DIM_LINE,1,&ids[0],&whole,v[2],scale,&dim)))return e;
            if((e=fd_dimension_create(section,PRO_TK_DIM_LINE,1,&ids[1],&whole,v[3],scale,&dim)))return e;
        
