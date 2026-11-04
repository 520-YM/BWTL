
            if(!wcscmp(shape,L"CENTERLINE"))line.type=PRO_2D_CENTER_LINE;
            line.end1[0]=v[0];line.end1[1]=v[1];line.end2[0]=v[2];line.end2[1]=v[3];
            if((e=ProSectionEntityAdd(section,(Pro2dEntdef *)&line,&ids[0])))return e;*count=1;
        
