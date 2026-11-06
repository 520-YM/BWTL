static int solid_plan_valid(int n,wchar_t a[][PART_TOKEN_MAX]){
    double v;int id;
    if(!wcscmp(a[0],L"SKETCH_MOVE"))return n==6&&fd_name(a[2])&&part_integer(a[3],&id)&&part_number(a[4],&v)&&fabs(v)<=1000000&&part_number(a[5],&v)&&fabs(v)<=1000000?1:-1;
    if(!wcscmp(a[0],L"SKETCH_REMOVE"))return n==5&&fd_name(a[2])&&(!wcscmp(a[3],L"ENTITY")||!wcscmp(a[3],L"DIMENSION")||!wcscmp(a[3],L"CONSTRAINT"))&&part_integer(a[4],&id)?1:-1;
    if(!wcscmp(a[0],L"SKETCH_PROJECT"))return n==4&&fd_name(a[2])&&fd_reference_token(a[3])?1:-1;
    if(!wcscmp(a[0],L"SOLID_EXTRUDE")){
        if(n!=8||!fd_name(a[2])||!fd_name(a[3])|| (wcscmp(a[4],L"ADD")&&wcscmp(a[4],L"CUT")) || (wcscmp(a[7],L"POS")&&wcscmp(a[7],L"NEG")))return -1;
        if(!wcscmp(a[5],L"THROUGH"))return !wcscmp(a[4],L"CUT")&&!wcscmp(a[6],L"0")?1:-1;
        return !wcscmp(a[5],L"BLIND")&&part_number(a[6],&v)&&v>=0.01&&v<=1000000?1:-1;
    }
    if(!wcscmp(a[0],L"SOLID_REVOLVE")||!wcscmp(a[0],L"SOLID_REVOLVE_CUT"))
        return n==7&&fd_name(a[2])&&fd_name(a[3])&&fd_reference_token(a[4])&&
            (wcscmp(a[0],L"SOLID_REVOLVE_CUT")||!wcsncmp(a[4],L"AXIS:",5))&&
            part_number(a[5],&v)&&v>=0.01&&v<=360&&(!wcscmp(a[6],L"POS")||!wcscmp(a[6],L"NEG"))?1:-1;
    return 0;
}
