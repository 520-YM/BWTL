
        if((e=model_circle(section,v[0],v[1],v[2],&ids[0])))return e;*count=1;
        if((e=fd_dimension_create(section,PRO_TK_DIM_DIA,1,&ids[0],&whole,2*v[2],scale,&dim)))return e;
    
