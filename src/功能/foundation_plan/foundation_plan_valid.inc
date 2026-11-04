static int foundation_plan_valid(int n,wchar_t args[][PART_TOKEN_MAX]){
    const wchar_t *verb=args[0];double a[6];int i;
    {int result=solid_plan_valid(n,args);if(result)return result;}
    if(!wcscmp(verb,L"REFERENCES"))return n==2||(n==3&&fd_reference_token(args[2]))?1:-1;
    if(wcscmp(verb,L"DATUM_PLANE")&&wcscmp(verb,L"DATUM_AXIS")&&wcscmp(verb,L"DATUM_POINT")&&wcscmp(verb,L"DATUM_CSYS")&&wcscmp(verb,L"DATUM_CURVE")&&wcscmp(verb,L"SKETCH_BEGIN")&&wcscmp(verb,L"SKETCH_CREATE")&&wcscmp(verb,L"SKETCH_ADD")&&wcscmp(verb,L"SKETCH_DIM")&&wcscmp(verb,L"SKETCH_CONSTRAINT")&&wcscmp(verb,L"SKETCH_INFO"))return 0;
    if(n<3||!fd_name(args[2]))return -1;
    if(!wcscmp(verb,L"SKETCH_INFO"))return n==3?1:-1;
    if(!wcscmp(verb,L"DATUM_PLANE"))return n==5&&fd_reference_token(args[3])&&part_number(args[4],&a[0])&&fabs(a[0])<=1000000&&fabs(a[0])>=0.001?1:-1;
    if(!wcscmp(verb,L"DATUM_AXIS")){
        if(n==6&&!wcscmp(args[3],L"PLANES")&&fd_reference_token(args[4])&&fd_reference_token(args[5])&&wcscmp(args[4],args[5]))return 1;
        if(n==5&&!wcscmp(args[3],L"CYLINDER")&&fd_reference_token(args[4]))return 1;
        return -1;
    }
    if(!wcscmp(verb,L"DATUM_POINT")||!wcscmp(verb,L"DATUM_CSYS")||!wcscmp(verb,L"DATUM_CURVE")){
        int count=!wcscmp(verb,L"DATUM_POINT")?3:6;
        if(n!=4+count||!fd_reference_token(args[3]))return -1;
        for(i=0;i<count;i++)if(!part_number(args[4+i],&a[i])||fabs(a[i])>1000000)return -1;
        if(!wcscmp(verb,L"DATUM_CSYS"))for(i=3;i<6;i++)if(fabs(a[i])>360)return -1;
        if(!wcscmp(verb,L"DATUM_CURVE")&&sqrt((a[3]-a[0])*(a[3]-a[0])+(a[4]-a[1])*(a[4]-a[1])+(a[5]-a[2])*(a[5]-a[2]))<0.01)return -1;
        return 1;
    }
    if(!wcscmp(verb,L"SKETCH_BEGIN"))return n==5&&fd_reference_token(args[3])&&fd_reference_token(args[4])?1:-1;
    if(!wcscmp(verb,L"SKETCH_CREATE"))return n>=9&&fd_reference_token(args[3])&&fd_reference_token(args[4])&&fd_shape_valid(n,args,5)?1:-1;
    if(!wcscmp(verb,L"SKETCH_ADD"))return fd_shape_valid(n,args,3)?1:-1;
    if(!wcscmp(verb,L"SKETCH_DIM")){
        if(n!=7||!fd_entity_token(args[4])||!part_number(args[6],&a[0])||a[0]<=0||a[0]>1000000)return -1;
        if(!wcscmp(args[3],L"LENGTH")||!wcscmp(args[3],L"DIAMETER")||!wcscmp(args[3],L"RADIUS"))return !wcscmp(args[5],L"NONE")?1:-1;
        if(!wcscmp(args[3],L"ANGLE")||!wcscmp(args[3],L"DISTANCE_X")||!wcscmp(args[3],L"DISTANCE_Y")||!wcscmp(args[3],L"DISTANCE"))return fd_entity_token(args[5])&&wcscmp(args[4],args[5])&&(!wcscmp(args[3],L"ANGLE")?a[0]<=360:1)?1:-1;
        return -1;
    }
    if(!wcscmp(verb,L"SKETCH_CONSTRAINT")){
        int refs=2;
        if(!wcscmp(args[3],L"HORIZONTAL")||!wcscmp(args[3],L"VERTICAL"))refs=1;
        else if(!wcscmp(args[3],L"SYMMETRY"))refs=3;
        else if(wcscmp(args[3],L"COINCIDENT")&&wcscmp(args[3],L"PARALLEL")&&wcscmp(args[3],L"PERPENDICULAR")&&wcscmp(args[3],L"TANGENT")&&wcscmp(args[3],L"EQUAL_LENGTH")&&wcscmp(args[3],L"EQUAL_RADIUS"))return -1;
        if(n!=4+refs)return -1;for(i=0;i<refs;i++)if(!fd_entity_token(args[4+i]))return -1;
        return 1;
    }return -1;
}
