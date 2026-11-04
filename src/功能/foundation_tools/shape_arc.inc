
        Pro2dArcdef arc={0};arc.type=PRO_2D_ARC;arc.center[0]=v[0];arc.center[1]=v[1];arc.radius=v[2];
        arc.start_angle=v[3]*3.14159265358979323846/180;arc.end_angle=v[4]*3.14159265358979323846/180;
        if((e=ProSectionEntityAdd(section,(Pro2dEntdef *)&arc,&ids[0])))return e;*count=1;
        if((e=fd_dimension_create(section,PRO_TK_DIM_RAD,1,&ids[0],&whole,v[2],scale,&dim)))return e;
    
